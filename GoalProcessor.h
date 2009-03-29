#pragma once

#include "Goal.h"

class GoalProcessor
{
public:
	GoalProcessor(void) {};
	virtual ~GoalProcessor(void) {};

	GoalStack goals;

	void AddGoal(Goal* g) { goals.push_back(g->id); }

	Goal* GetTopGoal()
	{
		if (goals.empty())
			return 0;
		return Goal::GetGoal(
			*std::max_element(goals.begin(), goals.end(), goal_priority_less()));
	}

	Goal* PopTopGoal()
	{
		if (goals.empty())
			return 0;
		GoalStack::iterator it = std::max_element(goals.begin(), goals.end(), goal_priority_less());
		int id = *it;
		goals.erase(it);
		return Goal::GetGoal(id);
	}
	
	virtual void ProcessGoalStack()
	{
		// pop from goals while ProcessGoal returns false
		// and goals are not empty
		while(true) {
			Goal* g = GetTopGoal();
			if (g) {
				if (ProcessGoal(g)) {
					break;
				} else {
					PopTopGoal();
				}
			} else {
				break;
			}
		}
	}
	virtual bool ProcessGoal(Goal *g) = 0;
	virtual void Update() = 0;
};
