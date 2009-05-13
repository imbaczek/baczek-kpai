#pragma once

// a thin wrapper on friendly unitids

#include <boost/shared_ptr.hpp>

#include "Sim/Units/UnitDef.h"

#include "UnitAI.h"

class BaczekKPAI;

class Unit
{
protected:
	void Init();

public:
	int id;

	BaczekKPAI *global_ai;
	boost::shared_ptr<UnitAI> ai;

	// unit status flags
	bool is_complete;
	bool is_killed;

	// unit type flags
	bool is_constructor;
	bool is_base;
	bool is_expansion;

	// unit state flags
	bool is_producing;

	// last idle state
	int last_idle_frame;

	Unit(BaczekKPAI* g_ai, int id) : global_ai(g_ai), id(id), is_complete(false), is_killed(false),
		is_producing(false)
	{ Init(); }
	~Unit() {}

	void complete() { is_complete = true; }
	void destroy(int attacker) { is_killed = true; if (ai) ai->OwnerKilled(); }
	
	// FIXME move to data file
	static bool IsConstructor(const UnitDef* ud) { return ud->name == "assembler" || ud->name == "gateway" || ud->name == "trojan"; }
	static bool IsBase(const UnitDef* ud) { return ud->name == "kernel" || ud->name == "hole" || ud->name == "carrier"; }
	static bool IsExpansion(const UnitDef* ud) { return ud->name == "window" || ud->name == "socket" || ud->name == "port"; }
	static bool IsSuperWeapon(const UnitDef* ud) { return ud->name == "terminal" || ud->name == "firewall" || ud->name == "obelisk"; }
};
