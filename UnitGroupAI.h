#pragma once

#include <map>
#include <boost/shared_ptr.hpp>

#include "float3.h"

#include "GoalProcessor.h"
#include "UnitAI.h"


class BaczekKPAI;

class UnitGroupAI : public GoalProcessor
{
public:
	UnitGroupAI(BaczekKPAI *theai) : ai(theai) {};
	~UnitGroupAI() {};

	BaczekKPAI* ai;

	typedef boost::shared_ptr<UnitAI> UnitAIPtr;
	typedef std::map<int, UnitAIPtr> UnitAISet;

	UnitAISet units;

	float3 rallyPoint;

	goal_process_t ProcessGoal(Goal* g);
	void Update();

	void AssignUnit(Unit* unit);
	void RemoveUnit(Unit* unit);
};
