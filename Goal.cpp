#include <iostream>
#include <boost/thread.hpp>

#include "Goal.h"

GoalSet g_goals;
int Goal::global_id = 0;


int Goal::CreateGoal(int priority, Type type)
{
	static boost::mutex mutex;
	{
		boost::mutex::scoped_lock(mutex);
		++global_id;
	}
	Goal* g = new Goal(priority, type);
	g_goals.insert(g->id, g);
	return g->id;
}
