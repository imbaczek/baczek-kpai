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
	bool complete;
	bool killed;

	// unit type flags
	bool is_constructor;
	bool is_base;

	// unit state flags
	bool is_producing;

	Unit(BaczekKPAI* g_ai, int id) : global_ai(g_ai), id(id), complete(false), killed(false),
		is_producing(false)
	{ Init(); }
	~Unit() {}

	void OnComplete() { complete = true; }
	void OnDestroy(int attacker) { killed = true; }
	
	static bool IsConstructor(const UnitDef* ud) { return ud->name == "assembler" || ud->name == "gateway" || ud->name == "trojan"; }
	static bool IsBase(const UnitDef* ud) { return ud->name == "kernel" || ud->name == "hole" || ud->name == "carrier"; }
};
