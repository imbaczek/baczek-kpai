// GroupAI.h: interface for the CGroupAI class.
// Dont modify this file
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_GroupAI_H__10718E36_5CDF_4CD4_8D90_F41311DD2694__INCLUDED_)
#define AFX_GroupAI_H__10718E36_5CDF_4CD4_8D90_F41311DD2694__INCLUDED_

#pragma once

#include <map>
#include <set>
#include <vector>

#include "ExternalAI/IGlobalAI.h"
#include "ExternalAI/IAICallback.h"
#include "ExternalAI/IAICheats.h"


#include "GUI/StatusFrame.h"
#include "InfluenceMap.h"
#include "PythonScripting.h"
#include "TopLevelAI.h"


const char AI_NAME[]="Baczek KP AI";

using namespace std;

class Log;
class Unit;

class BaczekKPAI : public IGlobalAI  
{
public:
	BaczekKPAI();
	virtual ~BaczekKPAI();

	void InitAI(IGlobalAICallback* callback, int team);

	void UnitCreated(int unit);									//called when a new unit is created on ai team
	void UnitFinished(int unit);								//called when an unit has finished building
	void UnitDestroyed(int unit,int attacker);								//called when a unit is destroyed

	void EnemyEnterLOS(int enemy);
	void EnemyLeaveLOS(int enemy);

	void EnemyEnterRadar(int enemy);				
	void EnemyLeaveRadar(int enemy);				

	void EnemyDamaged(int damaged,int attacker,float damage,float3 dir);	//called when an enemy inside los or radar is damaged
	void EnemyDestroyed(int enemy,int attacker);							//will be called if an enemy inside los or radar dies (note that leave los etc will not be called then)

	void UnitIdle(int unit);										//called when a unit go idle and is not assigned to any group

	void GotChatMsg(const char* msg,int player);					//called when someone writes a chat msg

	void UnitDamaged(int damaged,int attacker,float damage,float3 dir);					//called when one of your units are damaged

	void UnitMoveFailed(int unit);
	int HandleEvent (int msg,const void *data);

	//called every frame
	void Update();

	void DumpStatus();
	void FindGeovents();

	IGlobalAICallback* callback;
	IAICallback* cb;
	IAICheats* cheatcb;

	set<int> myUnits;
	set<int> losEnemies;

	// oldEnemies is used to find units that changed
	vector<int> oldEnemies;
	vector<int> allEnemies;
	vector<int> friends;

	// units
	Unit* unitTable[MAX_UNITS];

	virtual void Load(IGlobalAICallback* callback,std::ifstream *ifs){};
	virtual void Save(std::ifstream *ifs){};

	const char *datadir;
	const char *statusName;

	struct MapInfo {
		int w, h;
		int squareSize;
	};
	MapInfo map;

	vector<float3> geovents;

	InfluenceMap *influence;
	PythonScripting *python;

	TopLevelAI* toplevel;

#ifdef USE_STATUS_WINDOW
	MyFrame *frame;
#endif
};

#endif // !defined(AFX_GroupAI_H__10718E36_5CDF_4CD4_8D90_F41311DD2694__INCLUDED_)
