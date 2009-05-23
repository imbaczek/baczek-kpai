#include <algorithm>
#include <strstream>
#include <cmath>
#include <boost/foreach.hpp>
#include <boost/timer.hpp>

#include "ExternalAI/IGlobalAICallback.h"
#include "ExternalAI/IAICheats.h"
#include "float3.h"

#include "KPCommands.h"
#include "Log.h"
#include "Goal.h"
#include "TopLevelAI.h"
#include "BaczekKPAI.h"
#include "Unit.h"
#include "RNG.h"


TopLevelAI::TopLevelAI(BaczekKPAI* theai)
{
	builderRetreatGoalId = -1;
	ai = theai;
	builders = new UnitGroupAI(theai);
	bases = new UnitGroupAI(theai);
	expansions = new UnitGroupAI(theai);
	InitBattleGroups();

	// initially, gather
	attackState = AST_GATHER;
	lastRetreatTime = lastBattleRetreatTime = -10000;

	queuedConstructors = 0;
}

TopLevelAI::~TopLevelAI(void)
{
	delete builders; builders = 0;
	delete bases; bases = 0;
	delete expansions; expansions = 0;
}

void TopLevelAI::Update()
{
	boost::timer total;

	int frameNum = ai->cb->GetCurrentFrame();

	// check if builder's rally point is ok
	if (builders->rallyPoint.x < 0 && !bases->units.empty()) {
		builders->rallyPoint = random_offset_pos(ai->cb->GetUnitPos(bases->units.begin()->first), SQUARE_SIZE*10, SQUARE_SIZE*40);
	}

	if (frameNum % (GAME_SPEED * 10) == 1) {
		// dispatch before looking for goals
		DispatchPackets();
	}

	if (frameNum % (GAME_SPEED * 10) == 3) {
		FindGoals();
	}

	if (frameNum % GAME_SPEED == 0) {
		ProcessGoalStack(frameNum);
	}

	// update unit groups
	builders->Update();
	if (frameNum % GAME_SPEED == 1) {
		builders->RetreatUnusedUnits();
	} else if (frameNum % GAME_SPEED == 2) {
		FindPointerTargets();
	} 

	bases->Update();
	expansions->Update();
	BOOST_FOREACH(UnitGroupAI& it, groups) {
		it.Update();
	}

	ailog->info() << __FUNCTION__ << " " << total.elapsed() << std::endl;
}


struct RemoveGoalFromSkipped : public std::unary_function<Goal&, void> {
	TopLevelAI& self;
	RemoveGoalFromSkipped(TopLevelAI& s):self(s) {}
	void operator()(Goal& g) { self.skippedGoals.erase(g.id); }
};

GoalProcessor::goal_process_t TopLevelAI::ProcessGoal(Goal* g)
{
	if (!g)
		return PROCESS_POP_CONTINUE;
	if (g->is_finished())
		return PROCESS_POP_CONTINUE;

	switch (g->type) {
		case BUILD_EXPANSION:
			ProcessBuildExpansion(g);
			break;
		case BUILD_CONSTRUCTOR:
			ProcessBuildConstructor(g);
			break;
		case DEFEND_AREA:
			ProcessDefend(g);
			break;
		default:
			ailog->info() << "unknown goal type: " << g->type << " params "
				<< g->params.size() << std::endl;
			std::stringstream ss;
			BOOST_FOREACH(Goal::param_variant p, g->params) {
				ss << p << ", ";
			}
			ailog->info() << "params: " << ss.str() << endl;
	}
	return PROCESS_CONTINUE;
}


void TopLevelAI::ProcessBuildExpansion(Goal* g)
{
	ailog->info() << "goal " << g->id << ": BUILD_EXPANSION (" << g->params[0] << ")" << std::endl;
	if (!g->is_executing() && skippedGoals.find(g->id) == skippedGoals.end()) {
		// add goal for builder group
		Goal *newgoal = Goal::GetGoal(Goal::CreateGoal(g->priority, BUILD_EXPANSION));
		newgoal->params.push_back(g->params[0]);
		newgoal->parent = g->id;

		// events for current goal
		g->OnAbort(AbortGoal(*newgoal));
		g->OnAbort(RemoveGoalFromSkipped(*this));
		g->OnComplete(CompleteGoal(*newgoal));
		g->OnComplete(RemoveGoalFromSkipped(*this));

		// events for subgoal
		newgoal->OnComplete(CompleteGoal(*g));
		newgoal->OnStart(StartGoal(*g));
		newgoal->OnAbort(AbortGoal(*g));

		builders->AddGoal(newgoal);
		skippedGoals.insert(g->id);
	}
}

void TopLevelAI::ProcessDefend(Goal* g)
{
	ailog->info() << "goal " << g->id << ": DEFEND_AREA (" << g->params[0] << ")" << std::endl;
	if (g->is_executing() || skippedGoals.find(g->id) == skippedGoals.end())
		return;
	const float3& pos = boost::get<float3>(g->params[0]);
	float3 realpos;
	int inf = 0;
	ai->influence->FindLocalMinNear(pos, realpos, inf);
	ailog->info() << "defense: sent to (" << realpos << ") - influence " << inf << std::endl;

	Goal* newgoal = Goal::GetGoal(Goal::CreateGoal(g->priority*10, MOVE));

	g->OnAbort(AbortGoal(*newgoal));
	g->OnAbort(RemoveGoalFromSkipped(*this));
	g->OnComplete(CompleteGoal(*newgoal));
	g->OnComplete(RemoveGoalFromSkipped(*this));

	newgoal->OnComplete(CompleteGoal(*g));
	newgoal->OnStart(StartGoal(*g));
	newgoal->OnAbort(AbortGoal(*g));

	newgoal->params.push_back(realpos);

	skippedGoals.insert(g->id);

	groups[currentAssignGroup].AddGoal(newgoal);
}

void TopLevelAI::ProcessBuildConstructor(Goal* g)
{
	ailog->info() << "goal " << g->id << ": BUILD_CONSTRUCTOR" << std::endl;
	if (!g->is_executing()) {
		Goal *newgoal = Goal::GetGoal(Goal::CreateGoal(g->priority, BUILD_CONSTRUCTOR));
		newgoal->parent = g->id;
		
		g->OnAbort(AbortGoal(*newgoal));
		g->OnComplete(CompleteGoal(*newgoal));
		
		newgoal->OnComplete(CompleteGoal(*g));
		// start only when child starts
		newgoal->OnStart(StartGoal(*g));

		bases->AddGoal(newgoal);
		g->start();		
	}
}

//////////////////////////////////////////////////////////////////////////////////////
// finding goals
//////////////////////////////////////////////////////////////////////////////////////


/// high-level routine
void TopLevelAI::FindGoals()
{
	boost::timer t;

	std::vector<float3> badSpots;
	FindGoalsExpansion(badSpots); // badSpots gets modified here
	FindGoalsRetreatBuilders(badSpots); // and used here

	FindGoalsBuildConstructors();
	FindBaseBuildGoals();

	FindBattleGroupGoals();

	ailog->info() << __FUNCTION__ << " took " << t.elapsed() << std::endl;
}

/// find suitable expansion spots
/// also find spots that can't be expanded on, return them in badSpots
void TopLevelAI::FindGoalsExpansion(std::vector<float3>& badSpots)
{
	boost::timer t;

	// find free geo spots to build expansions on
	ailog->info() << "FindGoal() expansions" << std::endl;
	BOOST_FOREACH(float3 geo, ai->geovents) {
		// check if the expansion spot is taken
		std::vector<int> stuff;
		ai->GetAllUnitsInRadius(stuff, geo, 8);
		bool badspot = false;
		BOOST_FOREACH(int id, stuff) {
			// TODO switch to CanBuildAt?
			const UnitDef* ud = ai->cheatcb->GetUnitDef(id);
			bool alive = ai->cheatcb->GetUnitHealth(id) > 0;
			assert(ud);
			// TODO make configurable
			if (alive && (Unit::IsBase(ud) || Unit::IsExpansion(ud) || Unit::IsSuperWeapon(ud))) {
				badspot = true;
				ailog->info() << "found blocking " << ud->name << " at  " << ai->cheatcb->GetUnitPos(id) << std::endl;
				break;
			}
		}
		if (badspot) {
			badSpots.push_back(geo);
			ailog->info() << geo << " is a bad spot" << std::endl;
			continue;
		}

		// calculate priority
		float minDistance = bases->DistanceClosestUnit(geo, 0, 0);

		// can't reach
		if (minDistance < 0) {
			ailog->info() << "can't reach geo at " << geo << std::endl;
			continue;
		}

		int influence = ai->influence->GetAtXY(geo.x, geo.z);
		// TODO make constant configurable
		if (influence < 0) {
			ailog->info() << "too risky to build an expansion at " << geo << std::endl;
			continue;
		}

		// TODO make constant configurable
		float k = 15;
		float divider = (float)(ai->map.w + ai->map.h);
		int priority = ai->python->GetBuildSpotPriority(minDistance, influence, ai->map.w, ai->map.h, INT_MAX);
		if (priority == INT_MAX)
			priority = influence - (int)((minDistance/divider)*k);
		ailog->info() << "geo at " << geo << " distance to nearest base squared " << minDistance
			<< " influence " << influence << " priority " << priority << std::endl;
		// check if there already is a goal with this position
		bool dontadd = false;
		BOOST_FOREACH(int gid, goals) {
			Goal* goal = Goal::GetGoal(gid);
			if (!goal) {
				ailog->info() << "Goal " << gid << " doesn't exist" << endl;
				continue;
			}
			if (goal->type != BUILD_EXPANSION)
				continue;

			if (goal->params.empty()) {
				ailog->error() << "TopLevel BUILD_EXPANSION without param, removing" << endl;
				Goal::RemoveGoal(goal);
				continue;
			}
			float3 *param = boost::get<float3>(&goal->params[0]);
			if (!param) {
				ailog->error() << "TopLevel BUILD_EXPANSION with param 0 not float3 (" << goal->params[0] << "), removing" << endl;
				Goal::RemoveGoal(goal);
				continue;
			}

			// try to avoid duplicate goals, don't abort goals which are being executed
			if (param->SqDistance2D(geo) < 1) {
				if (goal->priority == priority || goal->is_executing()) {
					dontadd = true;
					break;
				} else {
					ailog->info() << "aborting old BUILD_EXPANSION goal " << goal->id << " at " << *param << endl;
					Goal::RemoveGoal(goal);
				}
			}
		}
		// add the goal
		if (!dontadd) {
			Goal *g = Goal::GetGoal(Goal::CreateGoal(priority, BUILD_EXPANSION));
			g->params.push_back(geo);
			g->timeoutFrame = ai->cb->GetCurrentFrame() + 5*60*GAME_SPEED;
			AddGoal(g);
		}
	}

	ailog->info() << __FUNCTION__ << " took " << t.elapsed() << std::endl;
}

/// remove BUILD_EXPANSION goals that are placed on spots which are now bad
/// also, when there are no spots left to build on, retreat whole builder group
void TopLevelAI::FindGoalsRetreatBuilders(std::vector<float3>& badSpots)
{
	boost::timer t;
	// filter out goals on bad spots
	// also check if there are BUILD_EXPANSION goals at all
	// if there are none, issue a RETREAT goal
	int expansionGoals = 0;
	bool hasRetreat = false;
	BOOST_FOREACH(int gid, goals) {
		Goal* goal = Goal::GetGoal(gid);
		if (!goal)
			continue;
		if (goal->type != BUILD_EXPANSION) {
			if (goal->type == RETREAT)
				hasRetreat = true;
			continue;
		}
		++expansionGoals;
		if (skippedGoals.find(gid) != skippedGoals.end())
			continue;
		if (goal->is_executing())
			continue;
		float3 *param = boost::get<float3>(&goal->params[0]);
		assert(param);

		BOOST_FOREACH(float3 geo, badSpots) {
			if (geo == *param) {
				Goal::RemoveGoal(goal);
				--expansionGoals;
				break;
			}
		}
	}
	
	assert(expansionGoals >= 0);
	if (expansionGoals == 0) {
		ailog->info() << "no expansion goals found" << std::endl;
	}
	// retreat if needed
	if (expansionGoals == 0 && !hasRetreat) {
		// there are no expansions left to take, retreat builders
		// retreat to one of the bases
		// unless there are no bases or the group is reasonably close to the base
		if (!bases->units.empty()) {
			// TODO move to data file
			const int maxDist = ai->python->GetIntValue("builderRetreatMaxDist", 40*SQUARE_SIZE);
			const int minDist = ai->python->GetIntValue("builderRetreatMinDist", 10*SQUARE_SIZE);
			const int checkOffset = ai->python->GetIntValue("builderRetreatCheckOffset", 10*SQUARE_SIZE);
			const int checkDist = maxDist+checkOffset;
			float3 basePos = ai->cb->GetUnitPos(bases->units.begin()->second->owner->id);
			float3 midPos = builders->GetGroupMidPos();
			if (midPos.SqDistance2D(basePos) > checkDist*checkDist) {
				// not close enough
				float3 dest = random_offset_pos(basePos, minDist, maxDist);
				Goal* goal = Goal::GetGoal(Goal::CreateGoal(1, RETREAT));
				goal->params.push_back(dest);
				goal->timeoutFrame = ai->python->GetBuilderRetreatTimeout(ai->cb->GetCurrentFrame());
				builders->AddGoal(goal);
				builderRetreatGoalId = goal->id;
			}
		}
	} else if (expansionGoals > 0 && hasRetreat) {
		// retreat should be aborted due to new construction goal
		ailog->info() << "aborting builder RETREAT goal" << std::endl;
		Goal* retreat = Goal::GetGoal(builderRetreatGoalId);
		if (retreat) {
			Goal::RemoveGoal(retreat);
			builderRetreatGoalId = -1;
		}
	}
	ailog->info() << __FUNCTION__ << " took " << t.elapsed() << std::endl;
}

/// decides whether to build constructors
void TopLevelAI::FindGoalsBuildConstructors()
{
	boost::timer t;
	/////////////////////////////////////////////////////
	// count own constructors and BUILD_CONSTRUCTOR goals
	ailog->info() << "FindGoal() constructors" << std::endl;

	// clunky...
	class IsConstructor : std::unary_function<int, bool> {
	public:
		BaczekKPAI *ai;
		IsConstructor(BaczekKPAI* a):ai(a) {}
		bool operator()(int uid) {
			return ai->unitTable[uid]->is_constructor;
		}
	};

	int bldcnt = std::count_if(ai->myUnits.begin(), ai->myUnits.end(), IsConstructor(ai));
	ailog->info() << "FindGoal() found " << bldcnt  << " constructors" << std::endl;
	
	int goalcnt = 0;
	BOOST_FOREACH(int gid, goals) {
		Goal* g = Goal::GetGoal(gid);
		if (!g)
			continue;
		if (g->type == BUILD_CONSTRUCTOR)
			++goalcnt;
	}
	ailog->info() << "FindGoal() found " << goalcnt  << " BUILD_CONSTRUCTOR goals" << std::endl;

	// determine the amount of needed constructors
	int wantedCtors = ai->python->GetWantedConstructors(ai->geovents.size(), ai->map.w, ai->map.h);
	if (builders->units.empty()
				|| goalcnt + bldcnt + queuedConstructors < wantedCtors - expansions->units.empty() - groups[currentBattleGroup].units.empty()) {
		ailog->info() << "adding BUILD_CONSTRUCTOR goal" << std::endl;
		Goal* g = Goal::GetGoal(Goal::CreateGoal(1, BUILD_CONSTRUCTOR));
		assert(g);
		AddGoal(g);
		++queuedConstructors;
	}

	std::sort(goals.begin(), goals.end(), goal_priority_less());
	ailog->info() << __FUNCTION__ << " took " << t.elapsed() << std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// battle group goals


void TopLevelAI::FindBattleGroupGoals()
{
	boost::timer t;

	ailog->info() << "assign group size: " << groups[currentAssignGroup].units.size()
		<< " battle group size: " << groups[currentBattleGroup].units.size() << std::endl;

	int frameNum = ai->cb->GetCurrentFrame();
	float3 midpos = groups[currentBattleGroup].GetGroupMidPos();

	// swap groups with some probability
	// TODO be smarter here
	bool swapped = false;
	int cagsize = groups[currentAssignGroup].units.size();
	int cbgsize = groups[currentBattleGroup].units.size();
	if (!groups[currentAssignGroup].units.empty()
			&& cagsize * 0.8 > cbgsize
			&& random() < 0.5 && lastSwapTime + 180*GAME_SPEED < frameNum
			&& !ImportantTargetInRadius(midpos, 1000)) {
		ai->cb->SendTextMsg("Battle groups swapped!", 0);
		SwapBattleGroups();
		swapped = true;
	}


	// change state with some probability
	// TODO be smarter about this
	// TODO move constants to config file
	bool healthDepleted = (float)groups[currentBattleGroup].GetGroupHealth()/(float)attackStartHealth < 0.2;
	if (!groups[currentBattleGroup].units.empty()) {
		if (!healthDepleted && attackState == AST_GATHER
				&& ai->python->GetIntValue("attackStateChangeTimeout", lastStateChangeTime + 90*GAME_SPEED) < frameNum
				&& random() < 0.25) {
			// try to be smart: if health isn't depleted, attack
			ai->cb->SendTextMsg("set mode to attack (!hd)", 0);
			SetAttackState(AST_ATTACK);
		} else if (healthDepleted && attackState == AST_ATTACK) {
			// else, retreat
			SetAttackState(AST_GATHER);
			ai->cb->SendTextMsg("set mode to gather (hd)", 0);
		}
		else if (!ImportantTargetInRadius(midpos, ai->python->GetFloatValue("importantRadius", 1000)) && random() < 0.1) {
			if (attackState == AST_ATTACK) {
				
				// not so smart, toggle state
				SetAttackState(AST_GATHER);
				ai->cb->SendTextMsg("set mode to gather", 0);
			}
			else {
				ai->cb->SendTextMsg("set mode to attack", 0);
				SetAttackState(AST_ATTACK);
			}
		}
	} else if (groups[currentBattleGroup].units.empty()) {
			SetAttackState(AST_GATHER);
			ai->cb->SendTextMsg("set mode to gather due to empty group", 0);
	}

	FindGoalsGather();
	FindGoalsAttack();
	ailog->info() << __FUNCTION__ << " took " << t.elapsed() << std::endl;
}


void TopLevelAI::FindGoalsGather()
{
	boost::timer t;

	// initial gather spot: current spot
	if (groups[currentAssignGroup].units.empty())
		return;

	const float gatherMinOffset = ai->python->GetFloatValue("gatherMinOffset", 256);
	const float gatherMaxOffset = ai->python->GetFloatValue("gatherMaxOffset", 768);
	float3 gatherSpot = random_offset_pos(groups[currentAssignGroup].GetGroupMidPos(), gatherMinOffset, gatherMaxOffset);
	float3 rootSpot = gatherSpot + float3(0, 100, 0); // only for debugging

	std::vector<int> values;
	std::vector<float3> positions;
	
	// assign group
	// find enemies near base (or constructors or expansions)
	int enemies[MAX_UNITS];
	int numenemies;

	const float baseDefenseRadius = ai->python->GetFloatValue("baseDefenseRadius", 1536);
	numenemies = ai->cheatcb->GetEnemyUnits(enemies, gatherSpot, baseDefenseRadius);
	// find the closest and sent group there
	float sqdist = FLT_MAX;
	float3 foundSpot;
	int found = -1;
	
	int rushBaseUnitCount = ai->python->GetIntValue("rushBaseUnitCount", 250);
	if (rushBaseUnitCount <= 0)
		rushBaseUnitCount = 250;
	if (groups[currentAssignGroup].units.size() >= (size_t)rushBaseUnitCount) {
		// rush enemy hq
		for (std::set<int>::iterator it = ai->enemyBases.begin(); it != ai->enemyBases.end(); ++it) {
			const UnitDef* ud = ai->cheatcb->GetUnitDef(*it);
			if (!ud)
				continue;
			found = 1;
			foundSpot = ai->cheatcb->GetUnitPos(*it);
			goto assign_group_found;
		}
		// no enemy bases, go to some expansion
		if (found == -1) {
			found = 1;
			// FIXME copypasta
			std::vector<int> candidates;
			for (std::vector<float3>::iterator it = ai->geovents.begin(); it != ai->geovents.end(); ++it) {
				numenemies = ai->cheatcb->GetEnemyUnits(enemies, *it, 256);
				if (numenemies) {
					candidates.push_back(it - ai->geovents.begin());
				}
			}

			if (!candidates.empty()) {
				int chosen = randint(0, candidates.size()-1);
				groups[currentBattleGroup].rallyPoint = ai->geovents[candidates[chosen]];
				goto assign_group_found;
			}
		}
	}

	for (int i = 0; i<numenemies; ++i) {
		float3 pos = ai->cheatcb->GetUnitPos(enemies[i]);
		float tmp = pos.SqDistance2D(gatherSpot);
		if (tmp < sqdist) {
			foundSpot = pos;
			sqdist = tmp;
			found = enemies[i];
		}
	}

assign_group_found:;

	// if no enemies found, stay at base
	if (found != -1) {
		groups[currentAssignGroup].rallyPoint = foundSpot;
		ai->cb->SendTextMsg("GATHER: reacting to enemy near expansion", 0);
	}
	else
		groups[currentAssignGroup].rallyPoint = gatherSpot;



	std::vector<int> candidates;
	for (std::vector<float3>::iterator it = ai->geovents.begin(); it != ai->geovents.end(); ++it) {
		numenemies = ai->cheatcb->GetEnemyUnits(enemies, *it, 256);
		if (numenemies) {
			candidates.push_back(it - ai->geovents.begin());
		}
	}

	if (!candidates.empty()) {
		int chosen = randint(0, candidates.size()-1);
		groups[currentBattleGroup].rallyPoint = ai->geovents[candidates[chosen]];
	}

	// issue retreat goals every 30s or so
	int frameNum = ai->cb->GetCurrentFrame();
	if (lastRetreatTime + 10*GAME_SPEED < frameNum) {
		lastRetreatTime = frameNum;
		RetreatGroup(&groups[currentAssignGroup]);
		ai->cb->CreateLineFigure(rootSpot+float3(0, 50, 0), groups[currentAssignGroup].rallyPoint+float3(0, 50, 0), 5, 10, 900, 0);
		ai->cb->SendTextMsg("retreating assign group", 0);
	}

	float3 midpos = groups[currentBattleGroup].GetGroupMidPos();
	if (attackState == AST_GATHER && lastBattleRetreatTime + 10*GAME_SPEED < frameNum && !ImportantTargetInRadius(midpos, 1000)) {
		lastBattleRetreatTime = frameNum;
		groups[currentBattleGroup].AttackMoveToSpot(groups[currentBattleGroup].rallyPoint);
		ai->cb->CreateLineFigure(rootSpot+float3(0, 50, 0), groups[currentBattleGroup].rallyPoint+float3(0, 50, 0), 5, 10, 900, 0);
		ai->cb->SendTextMsg("retreating battle group", 0);
	}
	ailog->info() << __FUNCTION__ << " took " << t.elapsed() << std::endl;
}


void TopLevelAI::SwapBattleGroups()
{
	int tmp = currentAssignGroup;
	currentAssignGroup = currentBattleGroup;
	currentBattleGroup = tmp;
}

void TopLevelAI::SetAttackState(TopLevelAI::AttackState state)
{
	if (state == attackState)
		return;
	
	// switch state and do postprocessing
	attackState = state;
	lastStateChangeTime = ai->cb->GetCurrentFrame();
	switch (state) {
		case AST_ATTACK:
			// reset retreat counter
			lastRetreatTime = -10000;
			attackStartHealth = groups[currentBattleGroup].GetGroupHealth();
			break;
		case AST_GATHER:
			break;
	}
}

void TopLevelAI::FindGoalsAttack()
{
	boost::timer t;

	if (attackState != AST_ATTACK)
		return;

	std::vector<int> values;
	std::vector<float3> positions;
	ai->influence->FindLocalMinima(256, values, positions);

	if (values.empty()) {
		ailog->info() << "FindLocalMinima didn't return any interesting points" << std::endl;
		return;
	}

	// find maximum minimum, move there, face minimim minimum (doesn't sound too good, yeah)
	int maxmin = INT_MIN;
	int maxminidx = -1;
	int minmin = INT_MAX;
	int minminidx = -1;
	for (std::vector<int>::iterator it = values.begin(); it != values.end(); ++it) {
		if (*it > maxmin) {
			maxmin = *it;
			maxminidx = it - values.begin();
		}
		if (*it < minmin) {
			minmin = *it;
			minminidx = it - values.begin();
		}
	}

	if (!groups.empty()) {
		if (minminidx != -1) {
			// check if there's a minifac or expansion near the spot
			// if there is, attack there
			std::vector<int> enemies;
			ai->GetEnemiesInRadius(positions[minminidx], 1024, enemies);
			if (!enemies.empty()) {
				for (std::vector<int>::iterator it = enemies.begin(); it != enemies.end(); ++it) {
					const UnitDef* unitdef = ai->cheatcb->GetUnitDef(*it);
					if (unitdef && (Unit::IsBase(unitdef) || Unit::IsExpansion(unitdef) || Unit::IsSuperWeapon(unitdef))) {
						// found a suitable target
						Goal* g = Goal::GetGoal(Goal::CreateGoal(11, ATTACK));
						g->timeoutFrame = 120*GAME_SPEED;
						g->params.push_back(*it);
						groups[currentBattleGroup].AddGoal(g);
						ai->cb->CreateLineFigure(ai->cheatcb->GetUnitPos(*it)+float3(0, 100, 0),
							positions[minminidx]+float3(0, 100, 0), 5, 5, 600, 0);
						ailog->info() << "proceeding to attack " << unitdef->name << " at " << ai->cheatcb->GetUnitPos(*it) << std::endl;
						break;
					}
				}
			}
			// good target not found, setup formation
			else if (minminidx != maxminidx) {
				groups[currentBattleGroup].AttackMoveToSpot(positions[maxminidx]);
				ai->cb->CreateLineFigure(positions[minminidx]+float3(0, 100, 0), positions[maxminidx], 5, 5, 600, 0);
			}
			else {
				groups[currentBattleGroup].AttackMoveToSpot(positions[minminidx]);
				ai->cb->CreateLineFigure(positions[minminidx]+float3(0, 100, 0), float3(ai->map.w*0.5f, 0, ai->map.h*0.5f), 5, 5, 600, 0);
			}
		} else {
			groups[currentBattleGroup].MoveTurnTowards(ai->cb->GetUnitPos(bases->units.begin()->first), float3(ai->map.w*0.5f, 0, ai->map.h*0.5f));
			ai->cb->CreateLineFigure(ai->cb->GetUnitPos(bases->units.begin()->first)+float3(0, 100, 0), float3(ai->map.w*0.5f, 0, ai->map.h*0.5f), 5, 5, 600, 0);
		}
	}
	ailog->info() << __FUNCTION__ << " took " << t.elapsed() << std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////

void TopLevelAI::FindBaseBuildGoals()
{
	if (bases->units.empty())
		return;

	// FIXME all wrong!
	int baseid = bases->units.begin()->first;
	
	if (ai->cb->GetCurrentUnitCommands(baseid)->size() > 2 || builders->units.empty())
		return;

	int totalUnits = 0;
	for (size_t i = 0; i<groups.size(); ++i) {
		totalUnits += groups[i].units.size();
	}

	// XXX
	switch (totalUnits > 10 ? randint(0, 2) : 0) {
		case 0: { // bits
			for (int i = 0; i<randint(1, 5); ++i) {
				Command build;
				std::string unit = ai->GetRoleUnitName("spam");
				const UnitDef* unitdef = ai->cb->GetUnitDef(unit.c_str());
				if (unitdef) {
					build.id = -unitdef->id;
					ai->cb->GiveOrder(baseid, &build);
				}
			}
			break;
		}
		case 1: { // byte
			Command build;
			std::string unit = ai->GetRoleUnitName("heavy");
			const UnitDef* unitdef = ai->cb->GetUnitDef(unit.c_str());
			if (unitdef) {
				build.id = -unitdef->id;
				ai->cb->GiveOrder(baseid, &build);
			}
		}
		case 2: { // pointer
			Command build;
			std::string unit = ai->GetRoleUnitName("arty");
			const UnitDef* unitdef = ai->cb->GetUnitDef(unit.c_str());
			if (unitdef) {
				build.id = -unitdef->id;
				ai->cb->GiveOrder(baseid, &build);
			}
		}
		default:
			break;
	}
}


struct RemoveSuspendedPointerGoal : std::unary_function<Goal&, void>
{
	TopLevelAI& self;
	RemoveSuspendedPointerGoal(TopLevelAI& s):self(s) {}
	void operator()(Goal& g) { self.suspendedPointerGoals.erase(g.id); }
};

void TopLevelAI::FindPointerTargets()
{
	boost::timer t;

	int enemies[MAX_UNITS];
	int numenemies;

	for (UnitGroupVector::iterator git = groups.begin(); git != groups.end(); ++git) {
		for (UnitGroupAI::UnitAISet::iterator it = git->units.begin(); it != git->units.end(); ++it) {
			int myid = it->first;
			const UnitDef* myud = ai->cb->GetUnitDef(myid);

			if (myud->name != "pointer" && myud->name != "dos" && myud->name != "flow")
				continue;

			float3 pos = ai->cb->GetUnitPos(it->first);
			// first, check if it's safe to stop
			if (ai->influence->GetAtXY(pos.x, pos.z) < 0)
				continue;

			float radius = ai->python->GetFloatValue((myud->name + "_radius").c_str(), 1000);
			numenemies = ai->cb->GetEnemyUnits(enemies, pos, radius);
			int smallTargets = 0;
			bool stopMoving = false;
			int foundid = -1;
			for (int i = 0; i<numenemies; ++i) {
				const UnitDef* unitdef = ai->cb->GetUnitDef(enemies[i]);
				assert(unitdef);
				if (Unit::IsSpam(unitdef)) {
					// target not worthy firing at, but we should stop moving anyway
					++smallTargets;
					continue;
				}
				// pointers are base killers
				else if (myud->name != "pointer"
					&& (Unit::IsExpansion(unitdef) || Unit::IsBase(unitdef)
					|| Unit::IsSuperWeapon(unitdef))) {
					foundid = enemies[i];
					break;
				}
				// doses are heavy unit disablers, flows are skirmishers
				else if ((myud->name == "dos" || myud->name == "flow")
					&& !(Unit::IsExpansion(unitdef) || Unit::IsBase(unitdef)
					|| Unit::IsSuperWeapon(unitdef))) {
					foundid = enemies[i];
					break;
				}
			}

			assert(ai->GetUnit(myid)->ai);
			Goal* goal = Goal::GetGoal(ai->GetUnit(myid)->ai->currentGoalId);
			UnitAI* unitai = ai->GetUnit(myid)->ai.get();

			if (foundid != -1) {
				// suspend goal and attack
				ailog->info() << "pointer " << myid << " suspending goal due to good target" << std::endl;
				if (goal) {
					unitai->SuspendCurrentGoal();
					if (suspendedPointerGoals.find(goal->id) == suspendedPointerGoals.end()) {
						suspendedPointerGoals.insert(goal->id);
						goal->OnAbort(RemoveSuspendedPointerGoal(*this));
						goal->OnComplete(RemoveSuspendedPointerGoal(*this));
						goal->OnContinue(RemoveSuspendedPointerGoal(*this));
					}
				}

				Command attack;
				attack.id = CMD_ATTACK;
				attack.AddParam(foundid);
				ai->cb->GiveOrder(myid, &attack);
			} else {
				// target in range and LOS not found, check for enemy bases or minifacs in range but not LOS
				numenemies = ai->cheatcb->GetEnemyUnits(enemies, pos, 1400);
				foundid = -1;
				for (int i = 0; i<numenemies; ++i) {
					const UnitDef* unitdef = ai->cheatcb->GetUnitDef(enemies[i]);
					assert(unitdef);
					if (Unit::IsBase(unitdef) || Unit::IsExpansion(unitdef) || Unit::IsSuperWeapon(unitdef)) {
						foundid = enemies[i];
						break;
					}
				}

				if (foundid != -1) {
					ailog->info() << "pointer " << myid << " suspending goal due to out-of-los fac target" << std::endl;
					if (goal) {
						unitai->SuspendCurrentGoal();
						if (suspendedPointerGoals.find(goal->id) == suspendedPointerGoals.end()) {
							suspendedPointerGoals.insert(goal->id);
							goal->OnAbort(RemoveSuspendedPointerGoal(*this));
							goal->OnComplete(RemoveSuspendedPointerGoal(*this));
							goal->OnContinue(RemoveSuspendedPointerGoal(*this));
						}
					}
					float3 nmypos = ai->cheatcb->GetUnitPos(foundid);
					Command attack;
					attack.id = CMD_ATTACK;
					attack.AddParam(nmypos.x);
					attack.AddParam(nmypos.y);
					attack.AddParam(nmypos.z);
					ai->cb->GiveOrder(myid, &attack);
				}
				else if (smallTargets >= 1
						&& (randint(1, 20) < smallTargets || ai->influence->GetAtXY(pos.x, pos.z) < 0)) { // FIXME move constant to data
					// if there is a lot of enemies nearby, suspend current goal and stop
					ailog->info() << "pointer " << myid << " suspending goal due to danger" << std::endl;
					if (goal) {
						unitai->SuspendCurrentGoal();
						if (suspendedPointerGoals.find(goal->id) == suspendedPointerGoals.end()) {
							suspendedPointerGoals.insert(goal->id);
							goal->OnAbort(RemoveSuspendedPointerGoal(*this));
							goal->OnComplete(RemoveSuspendedPointerGoal(*this));
							goal->OnContinue(RemoveSuspendedPointerGoal(*this));
						}
					}

					Command stop;
					stop.id = CMD_STOP;
					ai->cb->GiveOrder(myid, &stop);
				} else {
					// continue goal if it was aborted recently
					// TODO keep account of which goals were suspended here

					if (goal && goal->is_suspended() && suspendedPointerGoals.find(goal->id) != suspendedPointerGoals.end()) {
						ailog->info() << "pointer " << myid << " continuing goal after suspension" << std::endl;
						ai->GetUnit(myid)->ai->ContinueCurrentGoal();
					}
				}
			}
		}
	}
	ailog->info() << __FUNCTION__ << " took " << t.elapsed() << std::endl;
}


bool TopLevelAI::ImportantTargetInRadius(float3 pos, float radius)
{
	int num;
	int enemies[MAX_UNITS];
	num = ai->cheatcb->GetEnemyUnits(enemies, pos, radius);

	for (int i = 0; i<num; ++i) {
		const UnitDef* ud = ai->cheatcb->GetUnitDef(enemies[i]);
		if (ud && (Unit::IsExpansion(ud) || Unit::IsBase(ud) || ud->name == "pointer"))
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

void TopLevelAI::DispatchPackets()
{
	std::vector<int> exits;

	// find exits
	for (std::set<int>::iterator it = ai->myUnits.begin(); it != ai->myUnits.end(); ++it) {
		Unit* unit = ai->GetUnit(*it);
		// check if unit is completed
		if (!unit)
			continue;
		const UnitDef* ud = ai->cb->GetUnitDef(*it);
		if (ud && (ud->name == "port" || ud->name == "connection"))
			exits.push_back(*it);
	}

	if (exits.empty())
		return;

	// TODO something smarter here
	int chosen = exits[randint(0, exits.size()-1)];
	float3 pos = random_offset_pos(ai->cb->GetUnitPos(chosen), 64, 512);
	Command c;
	c.id = CMD_DISPATCH;
	c.AddParam(pos.x);
	c.AddParam(pos.y);
	c.AddParam(pos.z);
	c.options = ALT_KEY;
	ai->cb->GiveOrder(chosen, &c);
	const UnitDef* ud = ai->cb->GetUnitDef(chosen);
	ailog->info() << "dispatching packets to " << pos << " from unit " << chosen << " " << ud->name << std::endl;
}


//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

void TopLevelAI::InitBattleGroups()
{
	if (groups.empty()) {
		// add 2 groups
		groups.push_back(new UnitGroupAI(ai));
		groups.push_back(new UnitGroupAI(ai));
		// currently adding to group 0
		currentAssignGroup = 0;
		currentBattleGroup = 1;

		lastSwapTime = -10000;
		attackStartHealth = 1;
	}
}

void TopLevelAI::AssignUnitToGroup(Unit* unit)
{
	if (unit->is_killed)
		return;

	// easy stuff
	if (unit->is_base) {
		bases->AssignUnit(unit);
		return;
	}
	else if (unit->is_constructor) {
		builders->AssignUnit(unit);
		return;
	}
	else if (unit->is_expansion) {
		expansions->AssignUnit(unit);
		return;
	}

	groups[currentAssignGroup].AssignUnit(unit);

	ailog->info() << "unit " << unit->id << " assigned to combat group " << currentAssignGroup << std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////


void TopLevelAI::HandleExpansionCommands(Unit* expansion)
{
	assert(expansion);
	if (!expansion->ai)
		expansion->ai.reset(new UnitAI(ai, expansion));

	Command repeat;
	repeat.id = CMD_REPEAT;
	repeat.params.push_back(1);
	Command build;
	build.id = -expansion->ai->FindSpamUnitDefId();
	assert(build.id < 0);

	ai->cb->GiveOrder(expansion->id, &repeat);
	ai->cb->GiveOrder(expansion->id, &build);
}


void TopLevelAI::HandleBaseStartCommands(Unit* base)
{
	assert(base);
	if (!base->ai)
		base->ai.reset(new UnitAI(ai, base));

	Command build;
	build.id = -base->ai->FindSpamUnitDefId();
	assert(build.id < 0);
	
	ai->cb->GiveOrder(base->id, &build);
	ai->cb->GiveOrder(base->id, &build);
	ai->cb->GiveOrder(base->id, &build);
}


void TopLevelAI::RetreatGroup(UnitGroupAI *group, const float3 &dest)
{
	Goal* goal = Goal::GetGoal(Goal::CreateGoal(10, RETREAT));
	goal->params.push_back(dest);

	goal->timeoutFrame = ai->cb->GetCurrentFrame()
		+ ai->python->GetIntValue("retreatGroupTimeout", 15*GAME_SPEED);
	group->AddGoal(goal);
}

void TopLevelAI::RetreatGroup(UnitGroupAI *group)
{
	RetreatGroup(group, group->rallyPoint);
}

void TopLevelAI::UnitFinished(Unit* unit)
{
	if (unit->is_constructor)
		--queuedConstructors;
	assert(queuedConstructors >= 0);
}

void TopLevelAI::UnitIdle(Unit* unit)
{
	if (unit->is_base)
		FindBaseBuildGoals();
	
	if (unit->ai) {
		Goal* g = Goal::GetGoal(unit->ai->currentGoalId);
		if (g && !g->is_suspended())
			unit->ai->CompleteCurrentGoal();
	}
}

void TopLevelAI::UnitDamaged(Unit* unit, int attackerId, float damage, float3 dir)
{
	if (ai->cb->GetUnitAllyTeam(attackerId) == ai->cb->GetMyAllyTeam())
		return;
	if (damage < 0.1)
		return;
	if (ai->cb->GetUnitHealth(unit->id) <= 0)
		return;

	const UnitDef* ud = ai->cb->GetUnitDef(unit->id);
	assert(ud);


	int frameNum = ai->cb->GetCurrentFrame();

	// add defend goal
	if (unit->last_attacked_frame + 20*GAME_SPEED < frameNum
				&& (unit->is_base || unit->is_expansion || ud->name == "pointer")) {
		Goal* goal = Goal::GetGoal(Goal::CreateGoal(15 + unit->is_base, DEFEND_AREA));
		if (attackerId > 0) {
			goal->params.push_back(ai->cheatcb->GetUnitPos(attackerId));
		} else {
			goal->params.push_back(ai->cb->GetUnitPos(unit->id));
		}
		goal->timeoutFrame = frameNum + GAME_SPEED*20;
		ailog->info() << "adding DEFEND goal " << goal->id << std::endl;
		AddGoal(goal);
	}
	
	unit->last_attacked_frame = frameNum;
}


void TopLevelAI::EnemyDestroyed(int enemy, Unit* attacker)
{
	const UnitDef* ud = ai->cheatcb->GetUnitDef(enemy);


	if (!ud || Unit::IsBase(ud) || Unit::IsExpansion(ud) || Unit::IsSuperWeapon(ud)) {
		// recalculate attack goals
		float3 midpos = groups[currentBattleGroup].GetGroupMidPos();
		float importantRadius = ai->python->GetFloatValue("importantRadius", 1000);
		if (!ImportantTargetInRadius(midpos, importantRadius)) {
			FindBattleGroupGoals();
		}
		// TODO if a base was here, leave something so it cannot be easily rebuilt
	}
}
