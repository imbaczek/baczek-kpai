#include <strstream>
#include <boost/foreach.hpp>

#include "ExternalAI/IGlobalAICallback.h"
#include "ExternalAI/IAICheats.h"
#include "float3.h"

#include "Log.h"
#include "Goal.h"
#include "TopLevelAI.h"
#include "BaczekKPAI.h"


TopLevelAI::TopLevelAI(BaczekKPAI* theai)
{
	ai = theai;
	builders = new UnitGroupAI(theai);
}

TopLevelAI::~TopLevelAI(void)
{
	delete builders; builders = 0;
}

void TopLevelAI::Update()
{
	int frameNum = ai->cb->GetCurrentFrame();

	if (frameNum % 300 == 1) {
		FindGoals();
	}
	if (frameNum % 30 == 0) {
		ProcessGoalStack();
	}

	// update unit groups
	builders->Update();
	BOOST_FOREACH(UnitGroupAI& it, groups) {
		it.Update();
	}
}

bool TopLevelAI::ProcessGoal(Goal* g)
{
	if (!g)
		return false;

	switch (g->type) {
		default:
			ailog->info() << "unknown goal type: " << g->type << " params "
				<< g->params.size() << std::endl;
			std::stringstream ss;
			BOOST_FOREACH(Goal::param_variant p, g->params) {
				ss << p << ", ";
			}
			ailog->info() << "params: " << ss.str() << endl;
	}
	return false;
}

void TopLevelAI::FindGoals()
{
	ailog->info() << "FindGoal()" << std::endl;
	BOOST_FOREACH(float3 geo, ai->geovents) {
		// TODO priority should be a function of distance and risk
		int priority = ai->cb->GetCurrentFrame();
		// check if there already is a goal with this position
		// or if somebody built an expansion there
		BOOST_FOREACH(int gid, goals) {
			Goal* goal = Goal::GetGoal(gid);
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
}

