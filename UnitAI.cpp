#include "BaczekKPAI.h"
#include "Log.h"
#include "Unit.h"
#include "UnitAI.h"
#include "Goal.h"

UnitAI::UnitAI(BaczekKPAI* ai, Unit* owner): owner(owner), ai(ai)
{

}

UnitAI::~UnitAI(void)
{
}


////////////////////////////////////////////////////////////////////////////////
// operators

bool operator<(const UnitAI& a, const UnitAI& b)
{
	assert(a.owner);
	assert(b.owner);
	return a.owner->id < b.owner->id;
}

bool operator==(const UnitAI& a, const UnitAI& b)
{
	assert(a.owner);
	assert(b.owner);
	return a.owner->id == b.owner->id;
}

////////////////////////////////////////////////////////////////////////////////
// overloads

GoalProcessor::goal_process_t UnitAI::ProcessGoal(Goal* goal)
{
	if (goal->is_finished()) {
		return PROCESS_POP_CONTINUE;
	}

	if (goal->is_executing()) {
		return PROCESS_BREAK;
	}

	switch (goal->type) {
		case BUILD_EXPANSION: {
			Command c;
			c.id = -FindExpansionUnitDefId();
			assert(c.id);
			assert(!goal->params.empty());
			const float3& param = boost::get<float3>(goal->params[0]);
			c.params.push_back(param.x);
			c.params.push_back(param.y);
			c.params.push_back(param.z);
			ai->cb->GiveOrder(owner->id, &c);
			break;
		}
		case BUILD_CONSTRUCTOR: {
			if (!owner->is_base) {
				ailog->error() << "BUILD_CONSTRUCTOR issued to non-base unit" << std::endl;
				return PROCESS_POP_CONTINUE;
			}
			if (goal->is_executing()) {
				return PROCESS_BREAK;
			}
			Command c;
			c.id = -FindConstructorUnitDefId();
			ai->cb->GiveOrder(owner->id, &c);
			// FIXME XXX no way to track goal progress!
			goal->complete();
			return PROCESS_BREAK;
		}
	default:
		return PROCESS_POP_CONTINUE;
	}
	return PROCESS_BREAK;
}


void UnitAI::Update()
{
	int frameNum = ai->cb->GetCurrentFrame();

	if (frameNum % 30 == 0) {
		ProcessGoalStack();
		CleanupGoals();
	}
}


void UnitAI::OwnerKilled()
{
}


////////////////////////////////////////////////////////////////////////////////////////////////
// utils

// TODO move to a data file
int UnitAI::FindExpansionUnitDefId()
{
	assert(owner);
	const UnitDef *ud = ai->cb->GetUnitDef(owner->id);
	assert(ud);
	
	if (ud->name == "assembler") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("socket");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "trojan") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("window");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "gateway") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("port");
		assert(tobuild);
		return tobuild->id;
	}
	return 0;
}

// TODO move to a data file
int UnitAI::FindConstructorUnitDefId()
{
	assert(owner);
	const UnitDef *ud = ai->cb->GetUnitDef(owner->id);
	assert(ud);
	
	if (ud->name == "kernel") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("assembler");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "hole") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("trojan");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "carrier") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("gateway");
		assert(tobuild);
		return tobuild->id;
	}
	return 0;
}

