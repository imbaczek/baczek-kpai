#include "BaczekKPAI.h"
#include "Log.h"
#include "Unit.h"
#include "UnitAI.h"
#include "Goal.h"
#include "Rng.h"

UnitAI::UnitAI(BaczekKPAI* ai, Unit* owner): owner(owner), ai(ai), currentGoalId(-1)
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

struct on_complete_clean_current_goal : std::unary_function<Goal&, void> {
	UnitAI* ai;
	on_complete_clean_current_goal(UnitAI* uai):ai(uai) {}
	void operator()(Goal& goal) { ai->currentGoalId = -1; ailog->info() << "cleaning currentGoal on " << ai->owner->id << std::endl; }
};

struct on_complete_clean_producing : std::unary_function<Goal&, void> {
	Unit* unit;
	on_complete_clean_producing(Unit* u):unit(u) {}
	void operator()(Goal& goal) { unit->is_producing = false; ailog->info() << "cleaning is_producing on " << unit->id << std::endl; }
};


GoalProcessor::goal_process_t UnitAI::ProcessGoal(Goal* goal)
{
	if (!goal) {
		return PROCESS_POP_CONTINUE;
	}

	if (goal->is_finished()) {
		return PROCESS_POP_CONTINUE;
	}

	if (currentGoalId >= 0) {
		Goal* current = Goal::GetGoal(currentGoalId);
		if (current) {
			if (current->is_executing() && current->priority >= goal->priority) {
				return PROCESS_BREAK;
			}
		}
	}

	if (goal->is_executing()) {
		return PROCESS_BREAK;
	} else if (goal->is_restarted()) {
		goal->do_continue();
	} else if (goal->is_suspended()) {
		return PROCESS_BREAK;
	}

	ailog->info() << "EXECUTE GOAL: Unit " << owner->id << " executing goal " << goal->id << " type " << goal->type << std::endl;

	switch (goal->type) {
		case BUILD_EXPANSION: {
			if (!owner->is_constructor) {
				ailog->error() << "BUILD_EXPANSION issued to non-constructor unit" << std::endl;
				return PROCESS_POP_CONTINUE;
			}
			Command c;
			c.id = -FindExpansionUnitDefId();
			assert(c.id);
			assert(!goal->params.empty());
			const float3& param = boost::get<float3>(goal->params[0]);
			c.params.push_back(param.x);
			c.params.push_back(param.y);
			c.params.push_back(param.z);
			ai->cb->GiveOrder(owner->id, &c);
			goal->OnComplete(on_complete_clean_current_goal(this));
			goal->OnAbort(on_complete_clean_current_goal(this));
			goal->OnComplete(on_complete_clean_producing(this->owner));
			goal->OnAbort(on_complete_clean_producing(this->owner));
			owner->is_producing = true;
			goal->start();
			currentGoalId = goal->id;
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

		case MOVE:
		case RETREAT: {
			if (goal->params.empty()) {
				ailog->error() << "no params on RETREAT or MOVE goal" << std::endl;
				return PROCESS_POP_CONTINUE;
			}
			float3* param = boost::get<float3>(&goal->params[0]);
			if (!param) {
				ailog->error() << "invalid param on RETREAT or MOVE goal" << std::endl;
				return PROCESS_POP_CONTINUE;
			}
			Command c;
			c.id = CMD_MOVE;
			// TODO formation offset
			c.AddParam(param->x);
			c.AddParam(param->y);
			c.AddParam(param->z);
			ai->cb->GiveOrder(owner->id, &c);
			goal->OnComplete(on_complete_clean_current_goal(this));
			goal->OnAbort(on_complete_clean_current_goal(this));
			goal->start();
			currentGoalId = goal->id;
			
			return PROCESS_BREAK;
		}

		case ATTACK: {
			if (goal->params.empty()) {
				ailog->error() << "no params on ATTACK goal" << std::endl;
				return PROCESS_POP_CONTINUE;
			}
			float3* paramf = boost::get<float3>(&goal->params[0]);
			int* parami = boost::get<int>(&goal->params[0]);
			if (!paramf && !parami) {
				ailog->error() << "invalid param on ATTACK goal" << std::endl;
				return PROCESS_POP_CONTINUE;
			}
			Command c;
			if (paramf) { // attack move
				c.id = CMD_FIGHT;
				c.AddParam(paramf->x);
				c.AddParam(paramf->y);
				c.AddParam(paramf->z);
				// it's goot to have some units move up close
				// FIXME move constant to config file
				if (random() < 0.1)
					c.id = CMD_MOVE;
			} else { // attack unit
				c.id = CMD_ATTACK;
				c.AddParam(*parami);
			}
			ai->cb->GiveOrder(owner->id, &c);
			currentGoalId = goal->id;
			goal->OnComplete(on_complete_clean_current_goal(this));
			goal->OnAbort(on_complete_clean_current_goal(this));
			goal->start();

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

	int phase = frameNum % GAME_SPEED;

	if (phase == (owner ? owner->id%GAME_SPEED : 0)) {
		std::sort(goals.begin(), goals.end(), goal_priority_less());
		//DumpGoalStack("Unit");
		CheckContinueGoal();
		ProcessGoalStack(frameNum);
	} else if (phase == 1) {
		CheckBuildValid();
	}

	if (frameNum % GAME_SPEED == owner->id % GAME_SPEED) {
		CheckStandingInBase();
		if (owner->is_spam)
			CheckSpamTargets();
	}
}


void UnitAI::CheckContinueGoal()
{
	if (currentGoalId < 0) {
		return;
	}

	Goal* current = Goal::GetGoal(currentGoalId);
	if (!current) {
		currentGoalId = -1;
		return;
	}

	if (current->is_restarted()) {
		ailog->info() << "restarting goal " << current->id << " in CheckContinueGoal" << std::endl;
		ProcessGoal(current);
	}
}


void UnitAI::OwnerKilled()
{
	// signal listeners
	onKilled(*this);

	currentGoalId = -1;
	BOOST_FOREACH(int gid, goals) {
		Goal* g = Goal::GetGoal(gid);
		if (g)
			Goal::RemoveGoal(g);
	}
	owner = 0;
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

// TODO move to a data file
int UnitAI::FindSpamUnitDefId()
{
	assert(owner);
	const UnitDef *ud = ai->cb->GetUnitDef(owner->id);
	assert(ud);
	
	if (ud->name == "kernel" || ud->name == "socket") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("bit");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "hole" || ud->name == "window") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("bug");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "carrier" || ud->name == "port") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("packet");
		assert(tobuild);
		return tobuild->id;
	}
	return 0;
}


void UnitAI::CompleteCurrentGoal()
{
	if (currentGoalId >= 0) {
		Goal* currentGoal = Goal::GetGoal(currentGoalId);
		if (currentGoal && !currentGoal->is_finished()) {
			currentGoal->complete();
		}
	}
	currentGoalId = -1;
	owner->is_producing = false;
}

void UnitAI::SuspendCurrentGoal()
{
	if (currentGoalId >= 0) {
		Goal* currentGoal = Goal::GetGoal(currentGoalId);
		if (currentGoal && !currentGoal->is_finished()) {
			currentGoal->suspend();
		}
	}
}

void UnitAI::ContinueCurrentGoal()
{
	if (currentGoalId >= 0) {
		Goal* currentGoal = Goal::GetGoal(currentGoalId);
		if (currentGoal && currentGoal->is_suspended()) {
			currentGoal->continue_();
		}
	}
}


void UnitAI::CheckBuildValid()
{
	assert(owner);
	const CCommandQueue* q = ai->cb->GetCurrentUnitCommands(owner->id);
	if (q->empty())
		return;

	// is the first command a "build at pos" command
	Command c = *q->begin();
	if (c.id >= 0)
		return;
	if (c.params.size() != 3)
		return;

	float3 pos(c.params[0], c.params[1], c.params[2]);
	const UnitDef* ud = ai->GetUnitDefById(-c.id);
	assert(ud);

	std::vector<int> enemies;
	ai->GetEnemiesInRadius(pos, 96, enemies);

	if (!enemies.empty()) {
		// we shouldn't be building here, abort
		// unless of course it wasn't our goal...
		Goal* goal = Goal::GetGoal(currentGoalId);
		if (goal && goal->params.size() >= 1 && goal->type == BUILD_EXPANSION) {
			float3& param = boost::get<float3>(goal->params[0]);
			if (param.SqDistance2D(pos) < 8*8) {
				ailog->info() << "aborting construction goal at " << pos << " for builder "
					<< owner->id << " (goal id " << goal->id << ")" << std::endl;
				goal->abort();
				Command stop;
				stop.id = CMD_STOP;
				ai->cb->GiveOrder(owner->id, &stop);

				for (std::vector<int>::iterator it = enemies.begin(); it != enemies.end(); ++it) {
					ailog->info() << "  enemy at " << ai->cheatcb->GetUnitPos(*it) << std::endl;
					ai->cb->CreateLineFigure(pos+float3(0, 100, 0), ai->cheatcb->GetUnitPos(*it)+float3(0, 100, 0), 5, 20, 900, 0);
				}
			}
		}
	}
}

// check if there are important targets for spam units around
void UnitAI::CheckSpamTargets()
{
	assert(owner);
	const CCommandQueue* q = ai->cb->GetCurrentUnitCommands(owner->id);
	if (!q->empty()) {
		Command c = *q->begin();
		if (c.id != CMD_FIGHT && c.id != CMD_MOVE)
			return;
	}

	int num;
	int enemies[MAX_UNITS];
	float radius = ai->python->GetFloatValue("spam_radius", 384);
	float3 pos = ai->cb->GetUnitPos(owner->id);
	num = ai->cheatcb->GetEnemyUnits(enemies, pos, radius);
	int found = -1;

	for (int i = 0; i<num; ++i) {
		const UnitDef* ud = ai->cheatcb->GetUnitDef(enemies[i]);
		if (!ud)
			continue;
		if (Unit::IsConstructor(ud) || ud->name == "pointer" || ud->name == "dos" || ud->name == "flow") {
			found = enemies[i];
			break;
		}
	}
	if (found) {
		Command c;
		c.id = CMD_INSERT;
		c.options = ALT_KEY;
		c.AddParam(0);
		c.AddParam(CMD_ATTACK);
		c.AddParam(0);
		c.AddParam(found);
		ai->cb->GiveOrder(owner->id, &c);
	}
}

// do not stand in base and block construction
void UnitAI::CheckStandingInBase()
{
	assert(owner);

	if (owner->is_base)
		return;

	const CCommandQueue* q = ai->cb->GetCurrentUnitCommands(owner->id);
	if (q->empty()) {
		return;
	}
	else {
		Command c = *q->begin();
		if (c.id == CMD_MOVE)
			return;
	}

	int friends[MAX_UNITS];
	int num;
	float3 pos = ai->cb->GetUnitPos(owner->id);

	num = ai->cb->GetFriendlyUnits(friends, pos, 24);
	for (int i = 0; i<num; ++i) {
		Unit* u = ai->GetUnit(friends[i]);
		if (u && u->is_base) {
			float3 newpos = random_offset_pos(pos, 24, 48);
			Command c;
			c.id = CMD_INSERT;
			c.options = ALT_KEY;
			c.AddParam(0); // position in queue
			c.AddParam(CMD_MOVE);
			c.AddParam(0); // options
			c.AddParam(newpos.x);
			c.AddParam(newpos.y);
			c.AddParam(newpos.z);
			ai->cb->GiveOrder(owner->id, &c);
			break;
		}
	}
}
