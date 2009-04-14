#include <string>
#include <sstream>
#include <boost/foreach.hpp>

#include "Log.h"
#include "GoalProcessor.h"


void GoalProcessor::CleanupGoals(int frame)
{
	GoalStack newgoals;
	newgoals.reserve(goals.size());

	BOOST_FOREACH(int gid, goals) {
		Goal* goal = Goal::GetGoal(gid);
		if (goal) {
			// check for timeout
			if ((goal->timeoutFrame >= 0 && goal->timeoutFrame <= frame)
				|| goal->is_finished()) {
				Goal::RemoveGoal(goal);
				continue;
			}
			newgoals.push_back(gid);
		}
	}

	goals = newgoals;
}

void GoalProcessor::DumpGoalStack(std::string str)
{
	std::stringstream ss(str);

	ss << str << ":";
	BOOST_FOREACH(int gid, goals) {
		Goal* goal = Goal::GetGoal(gid);
		if (goal) {
			ss << "\ngoal id: " << gid << " type: " << goal->type
				<< " flags " << std::hex << goal->flags << std::dec;
			ss << " params: ";
			BOOST_FOREACH(Goal::param_variant& param, goal->params) {
				ss << param << ", ";
			}
			ss << " priority: " << goal->priority;
		}
	}

	ailog->info() << ss.str() << std::endl; 
}
