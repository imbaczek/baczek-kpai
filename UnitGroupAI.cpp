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
			ProcessBuildConstructor(goal);
			return PROCESS_CONTINUE;

		case BUILD_EXPANSION:
			if (goal->is_executing())
				return PROCESS_CONTINUE;
			ProcessBuildExpansion(goal);
			return PROCESS_CONTINUE;

		case RETREAT:
			if (goal->params.empty())
				return PROCESS_POP_CONTINUE;
			ProcessRetreatMove(goal);
			return PROCESS_BREAK;

		case ATTACK:
			if (goal->params.empty())
				return PROCESS_POP_CONTINUE;
			ProcessAttack(goal);
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
		CheckUnit2Goal();
		std::sort(goals.begin(), goals.end(), goal_priority_less());
		DumpGoalStack("UnitGroupAI");
		ProcessGoalStack(frameNum);
	}

	// update units
	BOOST_FOREACH(UnitAISet::value_type& v, units) {
		v.second->Update();
	}
}


///////////////////////////////////////////////////////////////////////////
// goal processing

void UnitGroupAI::ProcessBuildConstructor(Goal* goal)
{
	assert(goal);
	assert(goal->type == BUILD_CONSTRUCTOR);
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
		// unit found, exit loop
		break;
	}
}


void UnitGroupAI::ProcessBuildExpansion(Goal* goal)
{
	assert(goal);
	assert(goal->type == BUILD_EXPANSION);
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
		// TODO FIXME used goals aren't freed when units assigned to them die
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
		// do not suspend - risk matrix may have changed
		g->OnAbort(AbortGoal(*goal));
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
		unit2goal[unit->id] = goal->id;
		goal2unit[goal->id] = unit->id;
		// unit found, exit loop
		break;
	}
}



void UnitGroupAI::ProcessRetreatMove(Goal* goal)
{
	assert(goal);
	assert(goal->type == MOVE || goal->type == RETREAT);

	// may throw
	rallyPoint = boost::get<float3>(goal->params[0]);

	int i = 0;

	BOOST_FOREACH(UnitAISet::value_type& v, units) {
		UnitAIPtr uai = v.second;
		Unit* unit = uai->owner;
		assert(unit);
		if (usedUnits.find(unit->id) != usedUnits.end())
			continue;
		if (uai->HaveGoalType(RETREAT, goal->priority))
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
}


void UnitGroupAI::ProcessAttack(Goal* goal)
{
	assert(goal);
	assert(goal->type == ATTACK);

	// may throw
	rallyPoint = boost::get<float3>(goal->params[0]);

	int i = 0;

	BOOST_FOREACH(UnitAISet::value_type& v, units) {
		UnitAIPtr uai = v.second;
		Unit* unit = uai->owner;
		assert(unit);
		if (usedUnits.find(unit->id) != usedUnits.end())
			continue;
		if (uai->HaveGoalType(ATTACK, goal->priority))
			continue;
		usedUnits.insert(unit->id);

		ailog->info() << "gave " << unit->id << " ATTACK to " << rallyPoint << std::endl;
		Goal* g = Goal::GetGoal(Goal::CreateGoal(goal->priority, ATTACK));
		g->parent = goal->id;
		g->timeoutFrame = goal->timeoutFrame;
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
}



////////////////////////////////////////////////////////////////////
// unit stuff

struct OnKilledHandler : std::unary_function<UnitAI&, void> {
	UnitGroupAI& self;
	OnKilledHandler(UnitGroupAI& s):self(s) {}
	void operator()(UnitAI& uai) { self.RemoveUnitAI(uai); }
};

void UnitGroupAI::AssignUnit(Unit* unit)
{
	assert(unit);
	if (!unit->ai) {
		unit->ai.reset(new UnitAI(ai, unit));
	}
	UnitAIPtr uai = unit->ai;
	assert(uai);
	units.insert(UnitAISet::value_type(unit->id, uai));
	//uai->OnKilled(boost::bind(&UnitGroupAI::RemoveUnitAI, this)); // crashes msvc9 lol
	uai->OnKilled(OnKilledHandler(*this));
}

void UnitGroupAI::RemoveUnit(Unit* unit)
{
	assert(unit);
	units.erase(unit->id);
	usedUnits.erase(unit->id);
}

void UnitGroupAI::RemoveUnitAI(UnitAI& unitAi)
{
	assert(unitAi.owner);
	ailog->info() << "removing unit " << unitAi.owner->id << " from group" << std::endl;
	RemoveUnit(unitAi.owner);
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
			&& it->second->owner->last_idle_frame + 30 < ai->cb->GetCurrentFrame()	// and is idle for a while
			&& rallyPoint.SqDistance2D(ai->cb->GetUnitPos(it->first)) > 20*20*SQUARE_SIZE*SQUARE_SIZE // and not close to rally point
			&& !it->second->HaveGoalType(RETREAT)) {	 // and doesn't have a retreat goal
			// retreat
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
	g->params.push_back(random_offset_pos(rallyPoint, SQUARE_SIZE*4, SQUARE_SIZE*4*sqrt((float)units.size())));
	return g;
}

bool UnitGroupAI::CheckUnit2Goal()
{
#ifdef _DEBUG
	// check unit2goal
	for (std::map<int, int>::iterator it = unit2goal.begin(); it != unit2goal.end(); ++it) {
		Unit* unit = ai->GetUnit(it->first);
		assert(unit);
		assert(!unit->is_killed);
		assert(it->first == goal2unit[it->second]);
		Goal* unitgoal = Goal::GetGoal(unit->ai->currentGoalId);
		assert(unitgoal);
		assert(unitgoal->parent == it->second);
	}
	// check goal2unit
	for (std::map<int, int>::iterator it = goal2unit.begin(); it != goal2unit.end(); ++it) {
		Goal* goal = Goal::GetGoal(it->first);
		assert(goal);
		assert(it->first == unit2goal[it->second]);
		Unit* unit = ai->GetUnit(it->second);
		assert(unit);
	}
#endif
	return true;
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
	if (units.empty)
		return pos;

	for (UnitAISet::iterator it = units.begin(); it != units.end(); ++it) {
		pos += ai->cb->GetUnitPos(it->first);
	}
	pos /= (float)units.size();
	return pos;
}

int UnitGroupAI::GetGroupHealth()
{
	int health = 0;
	for (UnitAISet::iterator it = units.begin(); it != units.end(); ++it) {
		health += ai->cb->GetUnitHealth(it->first);
	}
	return health;
}


void UnitGroupAI::TurnTowards(float3 point)
{
	float3 midpos = GetGroupMidPos();
	float3 diff = midpos - point;
	dir = diff;
	dir.y = 0;
	dir.ANormalize();
	rightdir.x = -dir.z;
	rightdir.z = dir.x;
	// TODO move units to the new locations
	SetupFormation(point);
}

void UnitGroupAI::MoveTurnTowards(float3 dest, float3 point)
{
	float3 diff = dest - point;
	dir = diff;
	dir.y = 0;
	dir.ANormalize();
	rightdir.x = -dir.z;
	rightdir.z = dir.x;
	// TODO move units to the new locations
	SetupFormation(dest);
}

void UnitGroupAI::SetupFormation(float3 point)
{
	// TODO move to data file
	const static float aspectRatio = 4.f;
	const static int spacing = 48;

	int friends[MAX_UNITS];
	int friend_cnt;

	// perRow ** 2 / aspect ratio = total units
	perRow = std::ceil(std::sqrt(units.size()*aspectRatio));
	ailog->info() << "SetupFormation: perRow = " << perRow << std::endl;

	// put units like this
	//   front
	// 3 1 0 2 4
	// 8 6 5 7 9
	// if there are units on chosen spots, skip that spot and just move units to the destination
	int i = 0;
	for (UnitAISet::iterator it = units.begin(); it != units.end(); ++i, ++it) {
		if (it->second->currentGoalId >= 0) {
			continue;
		}
		int rowPos = i%perRow;
		int x;
		if (rowPos & 1) { // odd variant
			x = -(rowPos % perRow) / 2 - 1;
		} else { // even
			x = (rowPos % perRow) / 2; 
		}
		int y = i/perRow;
		float3 dest = dir*y*-spacing + rightdir*x*spacing + point;
		dest.y = ai->GetGroundHeight(dest.x, dest.z);

		// don't issue a move order if there already is a unit on the destination
		friend_cnt = ai->cb->GetFriendlyUnits(friends, dest, spacing);
		if (friend_cnt > 0) {
			continue;
		}

		Goal* g = Goal::GetGoal(Goal::CreateGoal(10, MOVE));
		assert(g);

		g->params.push_back(dest);
		it->second->AddGoal(g);
	}
}


void UnitGroupAI::AttackMoveToSpot(float3 dest)
{
	Goal* g = Goal::GetGoal(Goal::CreateGoal(10, ATTACK));
	assert(g);

	g->params.push_back(dest);
	g->timeoutFrame = ai->cb->GetCurrentFrame() + 30*GAME_SPEED;
	AddGoal(g);
}

void UnitGroupAI::MoveToSpot(float3 dest)
{
	Goal* g = Goal::GetGoal(Goal::CreateGoal(10, MOVE));
	assert(g);

	g->params.push_back(dest);
	g->timeoutFrame = ai->cb->GetCurrentFrame() + 30*GAME_SPEED;
	AddGoal(g);
}
