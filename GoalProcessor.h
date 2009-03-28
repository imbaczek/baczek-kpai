#pragma once

#include "Goal.h"

class GoalProcessor
{
public:
	GoalProcessor(void) {};
	virtual ~GoalProcessor(void) {};

	GoalQueue goals;

	void AddGoal(Goal* g) { goals.push(g->id); }
	Goal* GetTopGoal() { if (goals.empty()) return 0; else return Goal::GetGoal(goals.top()); }
	virtual bool ProcessGoal(Goal *g) = 0;
	virtual void Update() = 0;
};
