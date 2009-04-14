#pragma once

#include <map>
#include <set>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include "float3.h"

#include "Goal.h"
#include "GoalProcessor.h"
#include "UnitAI.h"


class BaczekKPAI;

class UnitGroupAI : public GoalProcessor
{
public:
	UnitGroupAI(BaczekKPAI *theai) : ai(theai), rallyPoint(-1, -1, -1) {};
	~UnitGroupAI() {};

	BaczekKPAI* ai;

	typedef boost::shared_ptr<UnitAI> UnitAIPtr;
	typedef std::map<int, UnitAIPtr> UnitAISet;

	UnitAISet units;
	std::set<int> usedUnits;
	std::set<int> usedGoals;

	struct RemoveUsedUnit : std::unary_function<Goal&, void> {
		UnitGroupAI& self;
		int unitId;
		RemoveUsedUnit(UnitGroupAI& s, int uid) : self(s), unitId(uid) {}
		void operator()(Goal& g) { self.usedUnits.erase(unitId); }
	};

	struct RemoveUsedGoal : std::unary_function<Goal&, void> {
		UnitGroupAI& self;
		int goalId;
		RemoveUsedGoal(UnitGroupAI& s, int gid):self(s), goalId(gid) {}
		void operator()(Goal& g) { self.usedGoals.erase(goalId); }
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

	goal_process_t ProcessGoal(Goal* g);
	void Update();

	void AssignUnit(Unit* unit);
	void RemoveUnit(Unit* unit);

	float SqDistanceClosestUnit(const float3& pos, int* unit, const UnitDef* unitdef);

	float3 GetGroupMidPos();

	void RetreatUnusedUnits();
	Goal* CreateRetreatGoal(UnitAI& uai, int timeoutFrame);
};
