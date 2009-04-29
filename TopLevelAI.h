#pragma once

#include <set>
#include <boost/ptr_container/ptr_vector.hpp>

#include "GoalProcessor.h"
#include "UnitGroupAI.h"

class BaczekKPAI;

class TopLevelAI : public GoalProcessor
{
public:
	TopLevelAI(BaczekKPAI*);
	~TopLevelAI();

	BaczekKPAI* ai;

	UnitGroupAI* builders;
	UnitGroupAI* bases;
	UnitGroupAI* expansions;

	typedef boost::ptr_vector<UnitGroupAI> UnitGroupVector;
	boost::ptr_vector<UnitGroupAI> groups;
	std::set<int> skippedGoals;

	int currentBattleGroup;
	int currentAssignGroup;
	int lastRetreatTime;
	int lastSwapTime;
	int lastStateChangeTime;
	int attackStartHealth;

	enum AttackState { AST_ATTACK, AST_GATHER };
	AttackState attackState;

	int builderRetreatGoalId;
	int queuedConstructors;

	goal_process_t ProcessGoal(Goal* g);
	void Update();

	void ProcessBuildExpansion(Goal* g);
	void ProcessBuildConstructor(Goal* g);

	void AssignUnitToGroup(Unit* unit);
	void InitBattleGroups();

	void FindGoals();

	void FindGoalsExpansion(std::vector<float3>& badSpots);
	void FindGoalsRetreatBuilders(std::vector<float3>& badSpots);
	void FindGoalsBuildConstructors();
	
	void FindBaseBuildGoals();

	void FindBattleGroupGoals();
	void FindGoalsGather();
	void FindGoalsAttack();
	void FindPointerTargets();

	void SwapBattleGroups();
	void SetAttackState(AttackState state);

	void RetreatGroup(UnitGroupAI* group, const float3& dest);
	void RetreatGroup(UnitGroupAI* group);

	void HandleExpansionCommands(Unit* expansion);
	void HandleBaseStartCommands(Unit* base);

	void UnitIdle(Unit* unit);
	void UnitFinished(Unit* unit);
};
