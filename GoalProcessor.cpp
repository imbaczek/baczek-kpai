#include <boost/foreach.hpp>

#include "GoalProcessor.h"

void GoalProcessor::CleanupGoals(int frame)
{
	GoalStack newgoals;
	newgoals.reserve(goals.size());

	BOOST_FOREACH(int gid, goals) {
		Goal* goal = Goal::GetGoal(gid);
		if (goal) {
			// check for timeout
			if (goal->timeoutFrame >= 0 && goal->timeoutFrame <= frame) {
				Goal::RemoveGoal(goal);
				continue;
			}
			newgoals.push_back(gid);
		}
	}

	goals = newgoals;
}
