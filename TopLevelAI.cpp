#include <algorithm>
#include <strstream>
#include <boost/foreach.hpp>

#include "ExternalAI/IGlobalAICallback.h"
#include "ExternalAI/IAICheats.h"
#include "float3.h"

#include "Log.h"
#include "Goal.h"
#include "TopLevelAI.h"
#include "BaczekKPAI.h"
#include "Unit.h"


TopLevelAI::TopLevelAI(BaczekKPAI* theai)
{
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

	if (frameNum % 300 == 1) {
		FindGoals();
	}
	if (frameNum % 30 == 0) {
		ProcessGoalStack();
		CleanupGoals();
	}

	// update unit groups
	builders->Update();
	BOOST_FOREACH(UnitGroupAI& it, groups) {
		it.Update();
	}
}

GoalProcessor::goal_process_t TopLevelAI::ProcessGoal(Goal* g)
{
	if (!g)
		return PROCESS_POP_CONTINUE;

	switch (g->type) {
		case BUILD_EXPANSION:
			ailog->info() << "goal: BUILD_EXPANSION (" << g->params[0] << ")" << std::endl;
			if (!g->is_executing()) {
				// add goal for builder group
				Goal *newgoal = Goal::GetGoal(Goal::CreateGoal(g->priority, BUILD_EXPANSION));
				newgoal->params.push_back(g->params[0]);
				builders->AddGoal(g);
				// execute
				g->start();
			}
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

void TopLevelAI::FindGoals()
{
	// find free geo spots to build expansions on
	ailog->info() << "FindGoal() expansions" << std::endl;
	BOOST_FOREACH(float3 geo, ai->geovents) {
		// TODO priority should be a function of distance and risk
		int priority = ai->cb->GetCurrentFrame();
		// check if there already is a goal with this position
		// or if somebody built an expansion there
		BOOST_FOREACH(int gid, goals) {
			Goal* goal = Goal::GetGoal(gid);
			if (!goal) {
				ailog->info() << "Goal " << gid << " doesn't exist" << endl;
				continue;
			}
			if (goal->type != BUILD_EXPANSION)
				continue;
			if (goal->priority == priority)
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
			if (param->SqDistance2D(geo) < 1) {
				ailog->info() << "aborting old BUILD_EXPANSION goal at " << param << endl;
				Goal::RemoveGoal(goal);
			}
		}
		// add the goal
		Goal *g = Goal::GetGoal(Goal::CreateGoal(priority, BUILD_EXPANSION));
		g->params.push_back(geo);
		AddGoal(g);
	}

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
	if (goalcnt + bldcnt < 3) {
		ailog->info() << "adding BUILD_CONSTRUCTOR goal" << std::endl;
		Goal* g = Goal::GetGoal(Goal::CreateGoal(1, BUILD_CONSTRUCTOR));
		assert(g);
		AddGoal(g);
	}
}

