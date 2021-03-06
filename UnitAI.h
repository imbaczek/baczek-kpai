#pragma once

#include <boost/signal.hpp>

#include "GoalProcessor.h"

class BaczekKPAI;
class Unit;

class UnitAI :
	public GoalProcessor
{
public:
	UnitAI(BaczekKPAI* ai, Unit* owner);
	~UnitAI();

	typedef boost::signal<void (UnitAI&)> on_killed_sig;
	typedef boost::signals::connection connection;

	Unit* owner;
	BaczekKPAI* ai;
	int currentGoalId;

	int stuckInBaseCnt;

	on_killed_sig onKilled;

	goal_process_t ProcessGoal(Goal* g);
	void Update();
	void OwnerKilled();

	connection OnKilled(on_killed_sig::slot_function_type f)
	{
		return onKilled.connect(f);
	}

	void FindGoals();

	// constructor stuff
	int FindExpansionUnitDefId();
	int FindConstructorUnitDefId();
	int FindSpamUnitDefId();

	void CompleteCurrentGoal();
	void SuspendCurrentGoal();
	void ContinueCurrentGoal();

	void CheckContinueGoal();

	void CheckBuildValid();
	void CheckSpamTargets();
	void CheckStandingInBase();
	bool CheckPosInBase(float3 pos);
};

bool operator<(const UnitAI& a, const UnitAI& b);
bool operator==(const UnitAI& a, const UnitAI& b);

