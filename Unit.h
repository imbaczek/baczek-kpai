#pragma once

// a thin wrapper on friendly unitids

#include "UnitAI.h"

class Unit
{
public:
	int id;

	bool complete;
	bool killed;

	Unit(int id) : id(id), complete(false), killed(false) {};
	~Unit() {};

	void OnComplete() { complete = true; }
	void OnDestroy(int attacker) { killed = true; }
};
