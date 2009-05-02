#include <algorithm>
#include <strstream>
#include <boost/foreach.hpp>
#include <cmath>

#include "ExternalAI/IGlobalAICallback.h"
#include "ExternalAI/IAICheats.h"
#include "float3.h"

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
	lastRetreatTime = -10000;

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
	int frameNum = ai->cb->GetCurrentFrame();

	// check if builder's rally point is ok
	if (builders->rallyPoint.x < 0 && !bases->units.empty()) {
		builders->rallyPoint = random_offset_pos(ai->cb->GetUnitPos(bases->units.begin()->first), SQUARE_SIZE*10, SQUARE_SIZE*40);
	}

	if (frameNum % 300 == 1) {
		FindGoals();
	}
	if (frameNum % 30 == 0) {
		ProcessGoalStack(frameNum);
	}

	// update unit groups
	builders->Update();
	if (frameNum %30 == 1) {
		builders->RetreatUnusedUnits();
		FindPointerTargets();
	}
	bases->Update();
	BOOST_FOREACH(UnitGroupAI& it, groups) {
		it.Update();
	}
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
	std::vector<float3> badSpots;
	FindGoalsExpansion(badSpots); // badSpots gets modified here
	FindGoalsRetreatBuilders(badSpots); // and used here

	FindGoalsBuildConstructors();
	FindBaseBuildGoals();

	FindBattleGroupGoals();
}

/// find suitable expansion spots
/// also find spots that can't be expanded on, return them in badSpots
void TopLevelAI::FindGoalsExpansion(std::vector<float3>& badSpots)
{
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
			if (alive && (ud->name == "socket" || ud->name == "port" || ud->name == "window"
					|| ud->name == "terminal" || ud->name == "firewall" || ud->name == "obelisk"
					|| ud->name == "kernel" || ud->name == "carrier" || ud->name == "hole")) {
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
		// TODO priority should be a function of distance and risk
		float minDistance = bases->SqDistanceClosestUnit(geo, 0, 0);
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
		float k = 100;
		float divider = (float)(ai->map.w*ai->map.w + ai->map.h*ai->map.h);
		int priority = influence - (int)((minDistance/divider)*k);
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
}

/// remove BUILD_EXPANSION goals that are placed on spots which are now bad
/// also, when there are no spots left to build on, retreat whole builder group
void TopLevelAI::FindGoalsRetreatBuilders(std::vector<float3>& badSpots)
{
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
			const int maxDist = 40;
			const int minDist = 10;
			const int checkOffset = 10;
			const int checkDist = maxDist+checkOffset;
			float3 basePos = ai->cb->GetUnitPos(bases->units.begin()->second->owner->id);
			float3 midPos = builders->GetGroupMidPos();
			if (midPos.SqDistance2D(basePos) > SQUARE_SIZE*SQUARE_SIZE*checkDist*checkDist) {
				// not close enough
				float3 dest = random_offset_pos(basePos, SQUARE_SIZE*10, SQUARE_SIZE*40);
				Goal* goal = Goal::GetGoal(Goal::CreateGoal(1, RETREAT));
				goal->params.push_back(dest);
				// FIXME move constant to data file
				goal->timeoutFrame = ai->cb->GetCurrentFrame() + 30*GAME_SPEED;
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
}

/// decides whether to build constructors
void TopLevelAI::FindGoalsBuildConstructors()
{
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

	// FIXME magic number
	// should be a function of time passed and map size/free geospots
	if (goalcnt + bldcnt + queuedConstructors < 4 - expansions->units.empty() - groups[currentBattleGroup].units.empty()) {
		ailog->info() << "adding BUILD_CONSTRUCTOR goal" << std::endl;
		Goal* g = Goal::GetGoal(Goal::CreateGoal(1, BUILD_CONSTRUCTOR));
		assert(g);
		AddGoal(g);
		++queuedConstructors;
	}

	std::sort(goals.begin(), goals.end(), goal_priority_less());
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// battle group goals


void TopLevelAI::FindBattleGroupGoals()
{
	ailog->info() << "assign group size: " << groups[currentAssignGroup].units.size()
		<< " battle group size: " << groups[currentBattleGroup].units.size() << std::endl;

	int frameNum = ai->cb->GetCurrentFrame();

	// swap groups with some probability
	// TODO be smarter here
	bool swapped = false;
	int cagsize = groups[currentAssignGroup].units.size();
	int cbgsize = groups[currentBattleGroup].units.size();
	if (!groups[currentAssignGroup].units.empty()
			&& cagsize * 0.8 > cbgsize
			&& random() < 0.5 && lastSwapTime + 180*GAME_SPEED < frameNum) {
		ai->cb->SendTextMsg("Battle groups swapped!", 0);
		SwapBattleGroups();
		swapped = true;
	}

	// change state with some probability
	// TODO be smarter about this
	// TODO move constants to config file
	bool healthDepleted = (float)groups[currentBattleGroup].GetGroupHealth()/(float)attackStartHealth < 0.5;
	if (!swapped && !groups[currentBattleGroup].units.empty()) {
		if (!healthDepleted && attackState == AST_GATHER
				&& lastStateChangeTime + 90*GAME_SPEED < frameNum
				&& random() < 0.25) {
			// try to be smart: if health isn't depleted, attack
			ai->cb->SendTextMsg("set mode to attack (!hd)", 0);
			SetAttackState(AST_ATTACK);
		} else if (healthDepleted && attackState == AST_ATTACK) {
			// else, retreat
			SetAttackState(AST_GATHER);
			ai->cb->SendTextMsg("set mode to gather (hd)", 0);
		}
		else if (random() < 0.1) {
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
}


void TopLevelAI::FindGoalsGather()
{
	// initial gather spot: base
	float3 gatherSpot = random_offset_pos(bases->GetGroupMidPos(), 256, 768);
	
	// find enemies near base (or constructors or expansions)
	int enemies[MAX_UNITS];
	int numenemies;
	numenemies = ai->cheatcb->GetEnemyUnits(enemies, gatherSpot, 1536);
	// find the closest and sent group there
	float sqdist = FLT_MAX;
	float3 foundSpot;
	int found = -1;
	for (int i = 0; i<numenemies; ++i) {
		float3 pos = ai->cheatcb->GetUnitPos(enemies[i]);
		float tmp = pos.SqDistance2D(gatherSpot);
		if (tmp < sqdist) {
			foundSpot = pos;
			sqdist = tmp;
			found = enemies[i];
		}
	}
	// if no enemies found, stay at base
	if (found != -1) {
		groups[currentAssignGroup].rallyPoint = foundSpot;
		ai->cb->SendTextMsg("GATHER: reacting to enemy", 0);
		ai->cb->CreateLineFigure(gatherSpot, foundSpot, 3, 1, 100, 0);
	}
	else
		groups[currentAssignGroup].rallyPoint = gatherSpot;

	// if there are no expansions, gather at base
	if (expansions->units.empty()) {
		groups[currentBattleGroup].rallyPoint = gatherSpot;
	} else {
		// better spot - expansion closest to enemy 
		// TODO cache use of FindLocalMinima
		std::vector<int> values;
		std::vector<float3> positions;
		ai->influence->FindLocalMinima(256, values, positions);
		
		// first, find minimum is closest to our base
		// then, find which expansion is closest to this minimum
		// gather there
		found = -1;
		sqdist = FLT_MAX;
		for (int i = 0; i<values.size(); ++i) {
			if (values[i] >= 0)
				continue;
			int uid;
			float tmp = bases->SqDistanceClosestUnit(positions[i], &uid, NULL);
			// less than 0 means not reachable
			if (tmp >= 0 && tmp < sqdist) {
				sqdist = tmp;
				found = i;
			}
		}
		// if not found, bail out
		if (found == -1) {
			groups[currentBattleGroup].rallyPoint = gatherSpot;
		} else {
			// now, find the expansion
			int uid;
			expansions->SqDistanceClosestUnit(positions[found], &uid, NULL);
			// set the rally point
			groups[currentBattleGroup].rallyPoint = random_offset_pos(ai->cb->GetUnitPos(uid), 256, 768);
		}
	}

	// issue retreat goals every 30s or so
	int frameNum = ai->cb->GetCurrentFrame();
	if (lastRetreatTime + 30*GAME_SPEED < frameNum) {
		lastRetreatTime = frameNum;
		RetreatGroup(&groups[currentAssignGroup]);
		if (attackState == AST_GATHER)
			RetreatGroup(&groups[currentBattleGroup]);
	}
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
	if (attackState != AST_ATTACK)
		return;

	std::vector<int> values;
	std::vector<float3> positions;
	ai->influence->FindLocalMinima(256, values, positions);
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
		if (minminidx != maxminidx) {
			groups[currentBattleGroup].MoveTurnTowards(positions[minminidx], positions[maxminidx]);
		} else {
			groups[currentBattleGroup].MoveTurnTowards(positions[minminidx], float3(ai->map.w*0.5f, 0, ai->map.h*0.5f));
		}
	}
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
	for (int i = 0; i<groups.size(); ++i) {
		totalUnits += groups[i].units.size();
	}

	switch (totalUnits > 10 ? randint(0, 2) : 0) {
		case 0: { // bits
			for (int i = 0; i<randint(1, 5); ++i) {
				Command build;
				build.id = -ai->cb->GetUnitDef("bit")->id;
				ai->cb->GiveOrder(baseid, &build);
			}
			break;
		}
		case 1: { // byte
			Command build;
			build.id = -ai->cb->GetUnitDef("byte")->id;
			ai->cb->GiveOrder(baseid, &build);
		}
		case 2: { // pointer
			Command build;
			build.id = -ai->cb->GetUnitDef("pointer")->id;
			ai->cb->GiveOrder(baseid, &build);
		}
		default:
			break;
	}
}

void TopLevelAI::FindPointerTargets()
{
	int enemies[MAX_UNITS];
	int numenemies;

	for (UnitGroupVector::iterator git = groups.begin(); git != groups.end(); ++git) {
		for (UnitGroupAI::UnitAISet::iterator it = git->units.begin(); it != git->units.end(); ++it) {
			int myid = it->first;
			const UnitDef* myud = ai->cb->GetUnitDef(myid);
			if (myud->name != "pointer")
				continue;

			float3 pos = ai->cb->GetUnitPos(it->first);
			// TODO eliminate constant - 1400 is pointer range
			numenemies = ai->cb->GetEnemyUnits(enemies, pos, 1400);
			int smallTargets = 0;
			bool stopMoving = false;
			int foundid = -1;
			for (int i = 0; i<numenemies; ++i) {
				const UnitDef* unitdef = ai->cb->GetUnitDef(enemies[i]);
				std::string name = unitdef->name;
				if (name == "bit" || name == "packet" || name == "exploit" || name == "bug") {
					// target not worthy firing at, but we should stop moving anyway
					++smallTargets;
					continue;
				}
				else {
					foundid = enemies[i];
					break;
				}
			}
			if (foundid != -1) {
				// suspend goal and attack
				assert(ai->GetUnit(myid)->ai);
				ai->GetUnit(myid)->ai->SuspendCurrentGoal();

				Command attack;
				attack.id = CMD_ATTACK;
				attack.AddParam(foundid);
				ai->cb->GiveOrder(myid, &attack);
			} else if (randint(0, smallTargets) > 5) { // FIXME move constant to data
				// if there is a lot of enemies nearby, suspend current goal and stop
				assert(ai->GetUnit(myid)->ai);
				ai->GetUnit(myid)->ai->SuspendCurrentGoal();

				Command stop;
				stop.id = CMD_STOP;
				ai->cb->GiveOrder(myid, &stop);
			} else {
				// continue goal if it was aborted recently
				// FIXME make this work
				// TODO keep account of which goals were suspended here
				assert(ai->GetUnit(myid)->ai);
				ai->GetUnit(myid)->ai->ContinueCurrentGoal();
			}
		}
	}
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
	// FIXME move constant to data file
	goal->timeoutFrame = ai->cb->GetCurrentFrame() + 15*GAME_SPEED;
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
}