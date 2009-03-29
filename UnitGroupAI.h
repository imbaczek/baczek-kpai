#pragma once

#include "GoalProcessor.h"

class BaczekKPAI;

class UnitGroupAI : public GoalProcessor
{
public:
	UnitGroupAI(BaczekKPAI *theai) : ai(theai) {};
	~UnitGroupAI() {};

	BaczekKPAI* ai;

	bool ProcessGoal(Goal* g);
	void Update();
};
