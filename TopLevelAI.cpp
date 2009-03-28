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
}

TopLevelAI::~TopLevelAI(void)
{
}

void TopLevelAI::Update()
{
	int frameNum = ai->cb->GetCurrentFrame();

	if (frameNum % 300 == 1) {
		FindGoals();
	}
	if (frameNum % 30 == 0) {
		// pop from goals while ProcessGoal returns false
		// and goals are not empty
		while(true) {
			Goal* g = GetTopGoal();
			if (g) {
				if (ProcessGoal(g)) {
					break;
				} else {
					goals.pop();
				}
			} else {
				break;
			}
		}
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
	}
	return false;
}

void TopLevelAI::FindGoals()
{
	ailog->info() << "FindGoal()" << std::endl;
	BOOST_FOREACH(float3 geo, ai->geovents) {
		// TODO priority should be a function of distance and risk
		Goal *g = Goal::GetGoal(Goal::CreateGoal(1, BUILD_EXPANSION));
		g->params.push_back(geo);
		AddGoal(g);
	}
}