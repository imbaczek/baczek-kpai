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
}

TopLevelAI::~TopLevelAI(void)
{
	delete builders; builders = 0;
	delete bases; bases = 0;
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
	if (goalcnt + bldcnt < 3) {
		ailog->info() << "adding BUILD_CONSTRUCTOR goal" << std::endl;
		Goal* g = Goal::GetGoal(Goal::CreateGoal(1, BUILD_CONSTRUCTOR));
		assert(g);
		AddGoal(g);
	}

	std::sort(goals.begin(), goals.end(), goal_priority_less());
}


//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

void TopLevelAI::AssignUnitToGroup(Unit* unit)
{
	if (unit->is_base)
		bases->AssignUnit(unit);
	else if (unit->is_constructor)
		builders->AssignUnit(unit);
}