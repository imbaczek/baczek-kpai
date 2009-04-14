#include <cassert>

#include "BaczekKPAI.h"
#include "UnitAI.h"
#include "Unit.h"

void Unit::Init()
{
	const UnitDef* ud = global_ai->cb->GetUnitDef(id);
	assert(ud);

	is_constructor = IsConstructor(ud);
	is_base = IsBase(ud);

	last_idle_frame = 0;
}