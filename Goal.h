#pragma once

#include <cassert>
#include <vector>
#include <string>
#include <queue>
#include <iostream>
#include <boost/ptr_container/ptr_unordered_map.hpp>
#include <boost/signal.hpp>
#include <boost/variant.hpp>

#include "float3.h"

#include "Log.h"

enum Type {
	ATTACK_UNIT,
	ATTACK_AREA,
	DEFEND_AREA,
	BUILD_CONSTRUCTOR,
	BUILD_UNIT,
	BUILD_EXPANSION,
	BUILD_WEAPON,
	MOVE,
	RETREAT,
	NO_TYPE,
};


// needed for boost::variant here
inline std::ostream &operator <<(std::ostream& os, float3 f)
{
	os << f.x << " " << f.y << " " << f.z;
	return os;
}



class Goal
{
public:
	Goal(void) {
		id = global_id;
		flags = 0;
		priority = 0;
		type = NO_TYPE;
		parent = -1;
	}

	Goal(int priority, Type type)
	{
		id = global_id;
		flags = 0;
		this->priority = priority;
		this->type = type;
		parent = -1;
	}

	~Goal() {};

	typedef boost::variant<int, float3, std::string> param_variant;
	typedef std::vector<param_variant> param_vector;
	
	typedef boost::signal<void (Goal&)> on_complete_sig;
	typedef boost::signal<void (Goal&)> on_abort_sig;
	typedef boost::signal<void (Goal&)> on_start_sig;
	typedef boost::signal<void (Goal&)> on_suspend_sig;
	typedef boost::signal<void (Goal&)> on_continue_sig;
	typedef boost::signals::connection connection;

	static const int FINISHED = 0x0001;
	static const int COMPLETED = 0x0002;
	static const int ABORTED = 0x0004;
	static const int EXECUTING = 0x0008;
	static const int SUSPENDED = 0x0010;

	static int global_id;

	int id;
	int priority;
	int flags;
	int parent;
	Type type;
	param_vector params;
	std::vector<int> nextGoals;

	on_complete_sig onComplete;
	on_abort_sig onAbort;
	on_start_sig onStart;
	on_suspend_sig onSuspend;
	on_continue_sig onContinue;

	bool operator<(const Goal& o) { return id < o.id; }
	bool operator==(const Goal& o) { return id == o.id; }

	connection OnComplete(on_complete_sig::slot_function_type f)
	{
		return onComplete.connect(f);
	}

	connection OnAbort(on_abort_sig::slot_function_type f)
	{
		return onAbort.connect(f);
	}

	connection OnStart(on_start_sig::slot_function_type f)
	{
		return onStart.connect(f);
	}

	connection OnContinue(on_continue_sig::slot_function_type f)
	{
		return onContinue.connect(f);
	}

	connection OnSuspend(on_suspend_sig::slot_function_type f)
	{
		return onSuspend.connect(f);
	}

	bool is_finished() { return (bool)(flags & FINISHED); }
	bool is_executing() { return (bool)(flags & EXECUTING); }

	void start() {
		assert(!is_finished());
		flags = EXECUTING;
		ailog->info() << "starting goal " << id << std::endl;
		onStart(*this);
	}
	void suspend() {
		assert(!is_finished());
		flags = SUSPENDED;
		ailog->info() << "suspending goal " << id << std::endl;
		onSuspend(*this);
	}
	void continue_() {
		assert(!is_finished());
		flags = EXECUTING;
		ailog->info() << "continuing goal " << id << std::endl;
		onContinue(*this);
	}
	void complete() {
		assert(!is_finished());
		flags = FINISHED | COMPLETED;
		ailog->info() << "completing goal " << id << std::endl;
		onComplete(*this);
	}
	void abort() {
		assert(!is_finished());
		flags = FINISHED | ABORTED;
		ailog->info() << "aborting goal " << id << std::endl;
		onAbort(*this);
	}

	static int CreateGoal(int priority, Type type);

	static Goal* GetGoal(int id);
	static void RemoveGoal(Goal* g);
};

typedef boost::ptr_unordered_map<int, Goal> GoalSet;

extern GoalSet g_goals;

class goal_priority_less : std::binary_function<int, int, bool> {
public:
	bool operator()(int a, int b) const
	{
		GoalSet::iterator it1 = g_goals.find(a);
		if (it1 == g_goals.end())
			return false;
		GoalSet::iterator it2 = g_goals.find(b);
		if (it2 == g_goals.end())
			return true;
		const Goal* aa = it1->second;
		const Goal* bb = it2->second;
		return aa->priority < bb->priority;
	}
};


typedef std::priority_queue<int, std::vector<int>, goal_priority_less> GoalQueue;
typedef std::vector<int> GoalStack;

inline Goal* Goal::GetGoal(int id)
{
	GoalSet::iterator it = g_goals.find(id);
	if (it == g_goals.end())
		return 0;
	return it->second;
}

inline void Goal::RemoveGoal(Goal* g)
{
	assert(g);
	if (!g->is_finished())
		g->abort();
	GoalSet::iterator it = g_goals.find(g->id);
	if (it != g_goals.end()) {
		g_goals.release(it);
	}
}


/////////////////////////////////////
// goal utilities, functors, etc


struct AbortGoal : public std::unary_function<Goal&, void> {
	Goal& self;
	AbortGoal(Goal& s):self(s) {}
	void operator()(Goal& other) { ailog->info() << "AbortGoal(" << self.id << ")" << std::endl; if (!self.is_finished()) self.abort(); }
};

struct CompleteGoal : public std::unary_function<Goal&, void> {
	Goal& self;
	CompleteGoal(Goal& s):self(s) {}
	void operator()(Goal& other) { ailog->info() << "CompleteGoal(" << self.id << ")" << std::endl; if (!self.is_finished()) self.complete(); }
};

struct StartGoal : public std::unary_function<Goal&, void> {
	Goal& self;
	StartGoal(Goal& s):self(s) {}
	void operator()(Goal& other) { ailog->info() << "StartGoal(" << self.id << ")" << std::endl; if (!self.is_executing()) self.start(); }
};
