#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "Log.h"
#include "BaczekKPAI.h"
#include "Unit.h"
#include "UnitGroupAI.h"
#include "Goal.h"
#include "UnitAI.h"

using boost::shared_ptr;

GoalProcessor::goal_process_t UnitGroupAI::ProcessGoal(Goal* goal)
{
	if (!goal || goal->is_finished()) {
		return PROCESS_POP_CONTINUE;
	}

	switch(goal->type) {
		case BUILD_CONSTRUCTOR:
			if (goal->is_executing())
				return PROCESS_CONTINUE;
			BOOST_FOREACH(UnitAISet::value_type& v, units) {
				UnitAIPtr uai = v.second;
				Unit* unit = uai->owner;
				assert(unit);
				if (unit->is_producing)
					continue;
				// FIXME move to a data file
				Goal *g = Goal::GetGoal(Goal::CreateGoal(1, BUILD_CONSTRUCTOR));
				assert(g);
				g->parent = goal->id;

				// behaviour when parent goal changes
				goal->OnAbort(AbortGoal(*g));
				goal->OnComplete(CompleteGoal(*g));
				// behaviour when subgoal changes
				g->OnComplete(CompleteGoal(*goal));

				ailog->info() << "unit " << unit->id << " assigned to producing a constructor";
				uai->AddGoal(g);
				goal->start();
				break;
			}
			return PROCESS_CONTINUE;
		case BUILD_EXPANSION:
			if (goal->is_executing())
				return PROCESS_CONTINUE;

			BOOST_FOREACH(UnitAISet::value_type& v, units) {
				UnitAIPtr uai = v.second;
				Unit* unit = uai->owner;
				assert(unit);
				if (unit->is_producing)
					continue;
				// FIXME move to a data file
				Goal *g = Goal::GetGoal(Goal::CreateGoal(1, BUILD_EXPANSION));
				assert(g);
				g->parent = goal->id;
				g->params.push_back(goal->params[0]);

				// behaviour when parent goal changes
				goal->OnAbort(AbortGoal(*g));
				goal->OnComplete(CompleteGoal(*g));
				// behaviour when subgoal changes
				g->OnComplete(CompleteGoal(*goal));

				ailog->info() << "unit " << unit->id << " assigned to producing a constructor";
				uai->AddGoal(g);
				goal->start();
				break;
			}
			return PROCESS_CONTINUE;
		default:
			return PROCESS_POP_CONTINUE;
	}
	
	return PROCESS_CONTINUE;
}

void UnitGroupAI::Update()
{
	int frameNum = ai->cb->GetCurrentFrame();

	if (frameNum % 30 == 0) {
		ProcessGoalStack();
		CleanupGoals();
	}

	// update units
	BOOST_FOREACH(UnitAISet::value_type& v, units) {
		v.second->Update();
	}
}

void UnitGroupAI::AssignUnit(Unit* unit)
{
	assert(unit);
	if (!unit->ai) {
		unit->ai.reset(new UnitAI(ai, unit));
	}
	UnitAIPtr uai = unit->ai;
	assert(uai);
	units.insert(UnitAISet::value_type(unit->id, uai));
}

void UnitGroupAI::RemoveUnit(Unit* unit)
{
	assert(unit);
	units.erase(unit->id);
}