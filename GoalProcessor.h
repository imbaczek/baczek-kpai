#pragma once

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
	
	virtual void ProcessGoalStack()
	{
		GoalStack todel;
		todel.reserve(goals.size());

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
						if (!g->is_finished())
							g->abort();
						todel.push_back(gid);
						goto end;
					case PROCESS_POP_CONTINUE:
						if (!g->is_finished())
							g->abort();
						todel.push_back(gid);
						break;
				}
			}
		}
end:
		// slow?
		BOOST_FOREACH(int gid, todel) {
			GoalStack::iterator it = std::find(goals.begin(), goals.end(), gid);
			if (it != goals.end()) {
				Goal* g = Goal::GetGoal(*it);
				assert(g);
				g->abort();
				goals.erase(it);
			}
		}
	}

	virtual void CleanupGoals();

	virtual goal_process_t ProcessGoal(Goal *g) = 0;
	virtual void Update() = 0;
};
