#pragma once

#include <string>
#include <boost/foreach.hpp>

#include "Goal.h"

class GoalProcessor
{
public:
	GoalProcessor(void) {};
	virtual ~GoalProcessor(void) {};

	enum goal_process_t {
		PROCESS_POP_CONTINUE,
		PROCESS_POP_BREAK,
		PROCESS_CONTINUE,
		PROCESS_BREAK,
	};

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
	
	virtual void ProcessGoalStack(int frameNum)
	{
		BOOST_REVERSE_FOREACH(int gid, goals) {
			Goal* g = Goal::GetGoal(gid);
			if (g) {
				goal_process_t gp = ProcessGoal(g);
				switch (gp) {
					case PROCESS_BREAK:
						goto end;
					case PROCESS_CONTINUE:
						break;
					case PROCESS_POP_BREAK:
						// goal will be aborted later
						Goal::RemoveGoal(g);
						goto end;
					case PROCESS_POP_CONTINUE:
						// goal will be aborted later
						Goal::RemoveGoal(g);
						break;
				}
			}
		}
end:
		CleanupGoals(frameNum);
	}

	virtual void CleanupGoals(int frameNum);
	void DumpGoalStack(std::string str);

	bool HaveGoalType(Type type) {
		for (GoalStack::iterator it = goals.begin(); it != goals.end(); ++it) {
			Goal* g = Goal::GetGoal(*it);
			if (g && g->type == type) {
				return true;
			}
		}
		return false;
	}

	virtual goal_process_t ProcessGoal(Goal *g) = 0;
	virtual void Update() = 0;
};
