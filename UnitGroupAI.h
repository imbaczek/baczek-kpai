#pragma once

#include <map>
#include <set>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include "float3.h"

#include "Goal.h"
#include "GoalProcessor.h"
#include "UnitAI.h"
#include "Log.h"


class BaczekKPAI;

class UnitGroupAI : public GoalProcessor
{
public:
	UnitGroupAI(BaczekKPAI *theai) : ai(theai), rallyPoint(-1, -1, -1),
			dir(1, 0, 0), rightdir(0, 0, 1)
		{};
	~UnitGroupAI() {};

	BaczekKPAI* ai;

	typedef boost::shared_ptr<UnitAI> UnitAIPtr;
	typedef std::map<int, UnitAIPtr> UnitAISet;

	UnitAISet units;
	std::map<int, int> usedUnits;
	std::set<int> usedGoals;
	std::map<int, int> unit2goal;
	std::map<int, int> goal2unit;

	struct RemoveUsedUnit : std::unary_function<Goal&, void> {
		UnitGroupAI& self;
		int unitId;
		RemoveUsedUnit(UnitGroupAI& s, int uid) : self(s), unitId(uid) {}
		void operator()(Goal& g) {
			ailog->info() << "removing used unit " << unitId << std::endl;
			self.usedUnits.erase(unitId);
			self.unit2goal.erase(unitId);
		}
	};

	struct RemoveUsedGoal : std::unary_function<Goal&, void> {
		UnitGroupAI& self;
		int goalId;
		RemoveUsedGoal(UnitGroupAI& s, int gid):self(s), goalId(gid) {}
		void operator()(Goal& g) {
			ailog->info() << "removing used goal " << goalId << std::endl;
			self.usedGoals.erase(goalId);
			self.goal2unit.erase(goalId);
		}
	};

	struct IfUsedUnitsEmpty : std::unary_function<Goal&, void> {
		UnitGroupAI& self;
		boost::function<void (Goal&)> func;
		IfUsedUnitsEmpty(UnitGroupAI& s, const boost::function<void (Goal&)>& f) : self(s), func(f) {}
		void operator()(Goal& other) {
			if (self.usedUnits.empty()) {
				func(other);
			}
		}
	};

	float3 rallyPoint;

	float3 dir;
	float3 rightdir;
	int perRow; //<? units per row in formation

	goal_process_t ProcessGoal(Goal* g);
	void Update();

	void ProcessBuildExpansion(Goal* g);
	void ProcessBuildConstructor(Goal* g);
	void ProcessRetreatMove(Goal* g);
	void ProcessAttack(Goal* g);

	void AssignUnit(Unit* unit);
	void RemoveUnit(Unit* unit);
	void RemoveUnitAI(UnitAI& unitai);

	// pos - point to which compute closest unit
	// unit - returns unit id
	// unitdef - unitdef for pathfinder purposes
	float SqDistanceClosestUnit(const float3& pos, int* unit, const UnitDef* unitdef);
	float DistanceClosestUnit(const float3& pos, int* unit, const UnitDef* unitdef);

	float3 GetGroupMidPos();
	int GetGroupHealth();

	void RetreatUnusedUnits();
	Goal* CreateRetreatGoal(UnitAI& uai, int timeoutFrame);
	bool CheckUnit2Goal();

	void TurnTowards(float3 point);
	void MoveTurnTowards(float3 dest, float3 point);
	void SetupFormation(float3 point);
	void AttackMoveToSpot(float3 dest);
	void MoveToSpot(float3 dest);
};
