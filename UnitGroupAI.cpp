#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "Sim/MoveTypes/MoveInfo.h"

#include "Log.h"
#include "BaczekKPAI.h"
#include "Unit.h"
#include "UnitGroupAI.h"
#include "Goal.h"
#include "UnitAI.h"
#include "Rng.h"


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

			// may throw
			rallyPoint = boost::get<float3>(goal->params[0]);

			BOOST_FOREACH(UnitAISet::value_type& v, units) {
				UnitAIPtr uai = v.second;
				Unit* unit = uai->owner;
				assert(unit);
				if (usedUnits.find(unit->id) != usedUnits.end())
					continue;
				if (uai->HaveGoalType(RETREAT))
					continue;
				usedUnits.insert(unit->id);

				ailog->info() << "gave " << unit->id << " RETREAT to " << rallyPoint << std::endl;
				Goal* g = CreateRetreatGoal(*uai, goal->timeoutFrame);
				g->parent = goal->id;
				// behaviour when subgoal changes
				// remove marks
				g->OnComplete(RemoveUsedUnit(*this, unit->id));
				// order matters!
				g->OnAbort(RemoveUsedUnit(*this, unit->id));
				g->OnComplete(IfUsedUnitsEmpty(*this, CompleteGoal(*goal)));
				g->OnStart(StartGoal(*goal));

				// behaviour when parent goal changes
				goal->OnAbort(AbortGoal(*g)); // abort subgoal
				goal->OnComplete(CompleteGoal(*g)); // complete subgoal

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


void UnitGroupAI::RetreatUnusedUnits()
{
	if (!rallyPoint.IsInBounds()) {
		ailog->info() << "cannot retreat unit group, rally point not set" << std::endl;
		return;
	}

	for (std::set<int>::iterator it = usedUnits.begin(); it != usedUnits.end(); ++it) {
		ailog->info() << (*it) << " is used" << std::endl;
	}

	for (UnitAISet::iterator it = units.begin(); it != units.end(); ++it) {
		if (usedUnits.find(it->first) == usedUnits.end()	// unit not used
			&& it->second->owner							// and exists
			&& it->second->owner->last_idle_frame + 30 < ai->cb->GetCurrentFrame()	// for a while
			&& rallyPoint.SqDistance2D(ai->cb->GetUnitPos(it->first)) < 10*SQUARE_SIZE*SQUARE_SIZE // and not close to rally point
			&& !it->second->HaveGoalType(RETREAT)) {	 // and doesn't have a retreat goal
			// unit is not used
			ailog->info() << "retreating unused " << it->first << std::endl;
			Goal* newgoal = CreateRetreatGoal(*it->second, 15*GAME_SPEED);
			it->second->AddGoal(newgoal);
		}
	}
}


Goal* UnitGroupAI::CreateRetreatGoal(UnitAI &uai, int timeoutFrame)
{
	Unit* unit = uai.owner;
	Goal *g = Goal::GetGoal(Goal::CreateGoal(1, RETREAT));
	assert(g);
	g->timeoutFrame = timeoutFrame;
	g->params.push_back(random_offset_pos(rallyPoint, 0, SQUARE_SIZE*10));
	return g;
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
		const UnitDef* ud = ai->cb->GetUnitDef(id);
		const float size = std::max(ud->xsize, ud->zsize)*SQUARE_SIZE;
		float3 upos = ai->cb->GetUnitPos(id);
		float3 startpos = random_offset_pos(upos, size*1.5, size*2);
		float tmp = ai->EstimateSqDistancePF(unitdef, startpos, pos);
		if (tmp < min && tmp >=0) {
			min = tmp;
			found_uid = id;
		} else if (tmp < 0) {
			ailog->error() << "can't reach " << pos << " from " << startpos << std::endl;
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
