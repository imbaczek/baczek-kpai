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
				Goal *g = Goal::GetGoal(Goal::CreateGoal(goal->priority, BUILD_CONSTRUCTOR));
				assert(g);
				g->parent = goal->id;

				// behaviour when parent goal changes
				goal->OnAbort(AbortGoal(*g));
				goal->OnComplete(CompleteGoal(*g));
				// behaviour when subgoal changes
				g->OnComplete(CompleteGoal(*goal));

				ailog->info() << "unit " << unit->id << " assigned to producing a constructor" << std::endl;
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
				if (!unit->is_constructor)
					continue;
				if (usedUnits.find(unit->id) != usedUnits.end())
					continue;
				if (usedGoals.find(goal->id) != usedGoals.end())
					continue;
				// FIXME move to a data file
				Goal *g = Goal::GetGoal(Goal::CreateGoal(1, BUILD_EXPANSION));
				assert(g);
				assert(goal->params.size() >= 1);
				g->parent = goal->id;
				g->params.push_back(goal->params[0]);

				// behaviour when parent goal changes
				goal->OnAbort(AbortGoal(*g)); // abort subgoal
				goal->OnComplete(CompleteGoal(*g)); // complete subgoal

				// behaviour when subgoal changes
				// mark parent as complete
				g->OnComplete(CompleteGoal(*goal));
				// remove marks
				g->OnComplete(RemoveUsedUnit(*this, unit->id));
				g->OnComplete(RemoveUsedGoal(*this, goal->id));
				g->OnAbort(RemoveUsedUnit(*this, unit->id));
				g->OnAbort(RemoveUsedGoal(*this, goal->id));

				ailog->info() << "unit " << unit->id << " assigned to building an expansion (goal id " << goal->id
					<< " " << goal->params[0] << ")" << std::endl;
				uai->AddGoal(g);
				goal->start();
				usedUnits.insert(unit->id);
				usedGoals.insert(goal->id);
				break;
			}
			return PROCESS_CONTINUE;

		case RETREAT:
			if (goal->params.empty())
				return PROCESS_POP_CONTINUE;

			BOOST_FOREACH(UnitAISet::value_type& v, units) {
				UnitAIPtr uai = v.second;
				Unit* unit = uai->owner;
				assert(unit);
				if (usedUnits.find(unit->id) != usedUnits.end())
					continue;
				usedUnits.insert(unit->id);

				Goal *g = Goal::GetGoal(Goal::CreateGoal(1, RETREAT));
				assert(g);
				g->parent = goal->id;
				g->timeoutFrame = goal->timeoutFrame;
				g->params.push_back(goal->params[0]);

				// behaviour when parent goal changes
				goal->OnAbort(AbortGoal(*g)); // abort subgoal
				goal->OnComplete(CompleteGoal(*g)); // complete subgoal

				// behaviour when subgoal changes
				// remove marks
				g->OnComplete(RemoveUsedUnit(*this, unit->id));
				// order matters!
				g->OnComplete(IfUsedUnitsEmpty(*this, CompleteGoal(*goal)));
				g->OnAbort(RemoveUsedUnit(*this, unit->id));
				g->OnStart(StartGoal(*goal));
				uai->AddGoal(g);
			}
			return PROCESS_BREAK;

		default:
			return PROCESS_POP_CONTINUE;
	}
	
	return PROCESS_CONTINUE;
}

void UnitGroupAI::Update()
{
	int frameNum = ai->cb->GetCurrentFrame();

	if (frameNum % 30 == 0) {
		usedUnits.clear();
		std::sort(goals.begin(), goals.end(), goal_priority_less());
		DumpGoalStack("UnitGroupAI");
		ProcessGoalStack(frameNum);
	}

	// update units
	BOOST_FOREACH(UnitAISet::value_type& v, units) {
		v.second->Update();
	}
}


////////////////////////////////////////////////////////////////////
// unit stuff

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
	usedUnits.erase(unit->id);
}

////////////////////////////////////////////////////////////////////
// utils

/// if unitdef is NULL, "assembler" is used
float UnitGroupAI::SqDistanceClosestUnit(const float3& pos, int* unit, const UnitDef* unitdef)
{
	float min = 1e30;
	int found_uid = -1;

	if (!unitdef) {
		unitdef = ai->cb->GetUnitDef("assembler");
		if (!unitdef) {
			ailog->error() << "default unitdef \"assembler\" not found in SqDistanceClosestUnit" << std::endl;
			return -1;
		}
	}

	BOOST_FOREACH(const UnitAISet::value_type& v, units) {
		int id = v.first;
		float3 upos = ai->cb->GetUnitPos(id);
		float tmp = ai->EstimateSqDistancePF(unitdef, upos, pos);
		if (tmp < min && tmp >=0) {
			min = tmp;
			found_uid = id;
		}
	}
	
	if (unit)
		*unit = found_uid;
	if (found_uid == -1)
		min = -1;
	return min;
}


float3 UnitGroupAI::GetGroupMidPos()
{
	float3 pos(0, 0, 0);
	for (UnitAISet::iterator it = units.begin(); it != units.end(); ++it) {
		pos += ai->cb->GetUnitPos(it->first);
	}
	pos /= (float)units.size();
	return pos;
}
