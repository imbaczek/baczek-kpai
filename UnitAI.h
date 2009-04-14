#pragma once
#include "GoalProcessor.h"

class BaczekKPAI;
class Unit;

class UnitAI :
	public GoalProcessor
{
public:
	UnitAI(BaczekKPAI* ai, Unit* owner);
	~UnitAI();

	Unit* owner;
	BaczekKPAI* ai;
	int currentGoalId;

	goal_process_t ProcessGoal(Goal* g);
	void Update();
	void OwnerKilled();

	void FindGoals();

	// constructor stuff
	int FindExpansionUnitDefId();
	int FindConstructorUnitDefId();

	void CompleteCurrentGoal();
};

bool operator<(const UnitAI& a, const UnitAI& b);
bool operator==(const UnitAI& a, const UnitAI& b);

