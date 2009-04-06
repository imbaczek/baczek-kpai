#include "BaczekKPAI.h"
#include "Unit.h"
#include "UnitAI.h"
#include "Goal.h"

UnitAI::UnitAI(BaczekKPAI* ai, Unit* owner): owner(owner), ai(ai)
{

}

UnitAI::~UnitAI(void)
{
}

bool UnitAI::ProcessGoal(Goal* goal)
{
	if (goal->is_executing()) {
		return true;
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
	default:
		break;
	}
	return true;
}

void UnitAI::Update()
{
}

void UnitAI::OwnerKilled()
{
}


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