#pragma once

#include <boost/ptr_container/ptr_vector.hpp>

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

	boost::ptr_vector<UnitAI> units;

	float3 rallyPoint;

	bool ProcessGoal(Goal* g);
	void Update();
};
