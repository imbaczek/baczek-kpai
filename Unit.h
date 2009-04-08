#pragma once

// a thin wrapper on friendly unitids

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
	UnitAI *ai;

	bool complete;
	bool killed;

	bool is_constructor;
	bool is_base;

	Unit(BaczekKPAI* g_ai, int id) : global_ai(g_ai), id(id), complete(false), killed(false) { Init(); }
	~Unit() {}

	void OnComplete() { complete = true; }
	void OnDestroy(int attacker) { killed = true; }
	
	static bool IsConstructor(const UnitDef* ud) { return ud->name == "assembler" || ud->name == "gateway" || ud->name == "trojan"; }
	static bool IsBase(const UnitDef* ud) { return ud->name == "kernel" || ud->name == "hole" || ud->name == "carrier"; }
};
