#include <boost/foreach.hpp>

#include "GoalProcessor.h"

void GoalProcessor::CleanupGoals()
{
	GoalStack newgoals;
	newgoals.reserve(goals.size());

	BOOST_FOREACH(int gid, goals) {
		Goal* goal = Goal::GetGoal(gid);
		if (goal)
			newgoals.push_back(gid);
	}

	goals = newgoals;
}
