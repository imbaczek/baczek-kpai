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
	boost::ptr_vector<UnitGroupAI> groups;
	std::set<int> skippedGoals;

	int builderRetreatGoalId;

	goal_process_t ProcessGoal(Goal* g);
	void Update();

	void ProcessBuildExpansion(Goal* g);
	void ProcessBuildConstructor(Goal* g);

	void AssignUnitToGroup(Unit* unit);
	void FindGoals();

	void FindGoalsExpansion(std::vector<float3>& badSpots);
	void FindGoalsRetreatBuilders(std::vector<float3>& badSpots);
	void FindGoalsBuildConstructors();
	void FindGoalsAttack();

	void HandleExpansionCommands(Unit* expansion);
};
