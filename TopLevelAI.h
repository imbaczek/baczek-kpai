#pragma once

#include "GoalProcessor.h"

class BaczekKPAI;

class TopLevelAI : public GoalProcessor
{
public:
	TopLevelAI(BaczekKPAI*);
	~TopLevelAI();

	BaczekKPAI* ai;

	bool ProcessGoal(Goal* g);
	void Update();

	void FindGoals();
};
