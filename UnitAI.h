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

	bool ProcessGoal(Goal* g);
	void Update();
	void OwnerKilled();

	void FindGoals();

	// constructor stuff
	int FindExpansionUnitDefId();
	int FindConstructorUnitDefId();
};
