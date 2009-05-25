// BaczekKPAI.cpp: implementation of the BaczekKPAI class.
//
////////////////////////////////////////////////////////////////////////////////

// system includes
#include <fstream>
#include <stdlib.h>
#include <cassert>
#include <iterator>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/timer.hpp>

// AI interface/Spring includes
#include "AIExport.h"
#include "ExternalAI/IGlobalAICallback.h"
#include "ExternalAI/IAICheats.h"
#include "Sim/MoveTypes/MoveInfo.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Features/FeatureDef.h"
#include "CUtils/Util.h" // we only use the defines

// project includes
#include "BaczekKPAI.h"
#include "Unit.h"
#include "GUI/StatusFrame.h"
#include "Log.h"
#include "InfluenceMap.h"
#include "PythonScripting.h"
#include "RNG.h"


namespace fs = boost::filesystem;

////////////////////////////////////////////////////////////////////////////////
// Construction/Destruction
////////////////////////////////////////////////////////////////////////////////


BaczekKPAI::BaczekKPAI()
{
	ailog = 0;
	influence = 0;
	python = 0;
	toplevel = 0;

	for (int i = 0; i<MAX_UNITS; ++i)
		unitTable[i] = 0;

	debugLines = false;
	debugMsgs = false;
}

BaczekKPAI::~BaczekKPAI()
{
	ailog->info() << "Shutting down." << endl;
	ailog->close();

	// order of deletion matters
	delete toplevel; toplevel = 0; // <- this should delete all child groups

	delete python; python = 0;
	delete influence; influence = 0;

	// global ailog deleted last
	delete ailog; ailog = 0;
}

////////////////////////////////////////////////////////////////////////////////
// AI Event functions
////////////////////////////////////////////////////////////////////////////////

Log* ailog = 0;

void BaczekKPAI::InitAI(IGlobalAICallback* callback, int team)
{
	init_rng();

	this->callback=callback;
	cb = callback->GetAICallback();
	cheatcb = callback->GetCheatInterface();

	cheatcb->EnableCheatEvents(true);

	datadir = aiexport_getDataDir(true, "");
	std::string dd(datadir);
	cb->SendTextMsg(AI_NAME, 0);
	cb->SendTextMsg(AI_VERSION, 0);
	cb->SendTextMsg("AI data directory:", 0);
	cb->SendTextMsg(datadir, 0);

	ailog = new Log(callback);
	ailog->open(aiexport_getDataDir(true, "log.txt"));
	ailog->info() << "Logging initialized.\n";
	ailog->info() << "Baczek KP AI compiled on " __TIMESTAMP__ "\n";
	ailog->info() << AI_NAME << " " << AI_VERSION << std::endl;



	InitializeUnitDefs();

	statusName = aiexport_getDataDir(true, "status.txt");
	map.h = cb->GetMapHeight();
	map.w = cb->GetMapWidth();
	map.squareSize = SQUARE_SIZE;

	float3::maxxpos = map.w * SQUARE_SIZE;
	float3::maxzpos = map.h * SQUARE_SIZE;
	ailog->info() << "Map size: " << float3::maxxpos << "x" << float3::maxzpos << std::endl;

	FindGeovents();

	std::string influence_conf = dd+"influence.json";
	if (!fs::is_regular_file(fs::path(influence_conf))) {
		InfluenceMap::WriteDefaultJSONConfig(influence_conf);
	}
	influence = new InfluenceMap(this, influence_conf);

	PythonScripting::RegisterAI(team, this);
	python = new PythonScripting(team, datadir);

	debugLines = python->GetIntValue("debugDrawLines", false);
	debugMsgs = python->GetIntValue("debugMessages", false);

	toplevel = new TopLevelAI(this);

	assert(random() != random() || random() != random());
	
#ifdef USE_STATUS_WINDOW
	int argc = 1;
	static  char *argv[] = {"SkirmishAI.dll"};

	wxEntry(argc, argv);

	    // Create the main application window
    frame = new MyFrame(_T("Drawing sample"),
                                 wxPoint(50, 50), wxSize(550, 340));

    // Show it and tell the application that it's our main window
    frame->Show(true);
#endif
}

void BaczekKPAI::UnitCreated(int unit)
{
	ailog->info() << "unit created: " << unit << std::endl;
	SendTextMsg("unit created", 0);
	myUnits.insert(unit);

	assert(!unitTable[unit]);
	unitTable[unit] = new Unit(this, unit);

	if (unitTable[unit]->is_expansion)
		toplevel->HandleExpansionCommands(unitTable[unit]);
	else if (unitTable[unit]->is_base)
		toplevel->HandleBaseStartCommands(unitTable[unit]);
}

void BaczekKPAI::UnitFinished(int unit)
{
	ailog->info() << "unit finished: " << unit << std::endl;

	assert(unitTable[unit]);
	unitTable[unit]->complete();
	toplevel->UnitFinished(unitTable[unit]);
	toplevel->AssignUnitToGroup(unitTable[unit]);
}

void BaczekKPAI::UnitDestroyed(int unit,int attacker)
{
	float3 pos = cb->GetUnitPos(unit);
	ailog->info() << "unit destroyed: " << unit << " at " << pos << std::endl;
	myUnits.erase(unit);

	assert(unitTable[unit]);
	Unit* tmp = unitTable[unit];
	unitTable[unit] = 0;
	tmp->destroy(attacker);
	delete tmp;
}

void BaczekKPAI::EnemyEnterLOS(int enemy)
{
	losEnemies.insert(enemy);
}

void BaczekKPAI::EnemyLeaveLOS(int enemy)
{
	losEnemies.erase(enemy);
}

void BaczekKPAI::EnemyEnterRadar(int enemy)
{
}

void BaczekKPAI::EnemyLeaveRadar(int enemy)
{
}

void BaczekKPAI::EnemyDamaged(int damaged, int attacker, float damage,
		float3 dir)
{
}

void BaczekKPAI::EnemyDestroyed(int enemy,int attacker)
{
	SendTextMsg("enemy destroyed", 0);
	losEnemies.erase(enemy);
	allEnemies.erase(find(allEnemies.begin(), allEnemies.end(), enemy));

	Unit* unit = GetUnit(attacker);
	toplevel->EnemyDestroyed(enemy, unit);
}

void BaczekKPAI::UnitIdle(int unit)
{
	Unit* u = GetUnit(unit);
	assert(u);
	ailog->info() << "unit idle " << unit << std::endl;
	u->last_idle_frame = cb->GetCurrentFrame();

	toplevel->UnitIdle(u);
}

void BaczekKPAI::GotChatMsg(const char* msg,int player)
{
}

void BaczekKPAI::UnitDamaged(int damaged,int attacker,float damage,float3 dir)
{
	toplevel->UnitDamaged(GetUnit(damaged), attacker, damage, dir);
}

void BaczekKPAI::UnitMoveFailed(int unit)
{
}

int BaczekKPAI::HandleEvent(int msg,const void* data)
{
	ailog->info() << "event " << msg << std::endl;
	return 0; // signaling: OK
}

void BaczekKPAI::Update()
{
	boost::timer total;
	int frame=cb->GetCurrentFrame();

	int unitids[MAX_UNITS];
	int num = cb->GetFriendlyUnits(unitids);
	friends.clear();
	std::copy(unitids, unitids+num, inserter(friends, friends.begin()));

	num = cheatcb->GetEnemyUnits(unitids);
	oldEnemies.clear();
	oldEnemies.resize(allEnemies.size());
	std::copy(allEnemies.begin(), allEnemies.end(), oldEnemies.begin());
	allEnemies.clear();
	allEnemies.resize(num);
	std::copy(unitids, unitids+num, allEnemies.begin());

	if (frame == 1) {
		// XXX this will fail if used with prespawned units, e.g. missions
		std::copy(unitids, unitids+num, std::inserter(enemyBases, enemyBases.end()));
	}

	if ((frame % 30) == 0) {
		DumpStatus();
	}
	influence->Update(friends, allEnemies);
	python->GameFrame(frame);
	// enable dynamic switching of debug info
	debugLines = python->GetIntValue("debugDrawLines", false);
	debugMsgs = python->GetIntValue("debugMessages", false);


	toplevel->Update();

	ailog->info() << "frame " << frame << " in " << total.elapsed() << std::endl;
}

///////////////////
// helper methods

void BaczekKPAI::FindGeovents()
{
	int features[MAX_UNITS];
	int num = cheatcb->GetFeatures(features, MAX_UNITS);
	ailog->info() << "found " << num << " features" << endl;
	for (int i = 0; i<num; ++i) {
		int featId = features[i];
		const FeatureDef* fd = cb->GetFeatureDef(featId);
		assert(fd);
		ailog->info() << "found feature " << fd->myName << "\n";
		if (fd->myName != "geovent")
			continue;
		float3 fpos = cb->GetFeaturePos(featId);
		ailog->info() << "found geovent at " << fpos.x << " " << fpos.z << "\n";
		// check if there isn't a geovent in close proximity (there are maps
		// with duplicate geovents)
		BOOST_FOREACH(float3 oldpos, geovents) {
			if (oldpos.SqDistance2D(fpos) <= 64*64)
				goto bad_geo;
		}
		ailog->info() << "adding geovent" << endl;
		geovents.push_back(fpos);
bad_geo: ;
	}
}

void BaczekKPAI::DumpStatus()
{
	std::string tmpName = std::string(statusName)+".tmp";
	ofstream statusFile(tmpName.c_str());
	// dump map size, frame number, etc
	int frame = cb->GetCurrentFrame();
	statusFile << "frame " << frame << "\n"
		<< "map " << map.w << " " << map.h << "\n";
	// dump known geovents
	statusFile << "geovents\n";
	BOOST_FOREACH(float3 geo, geovents) {
		statusFile << "\t" << geo.x << " " << geo.z << "\n";
	}
	// dump units
	int unitids[MAX_UNITS];
	// dump known friendly units
	statusFile << "units friendly\n";
	int num = cb->GetFriendlyUnits(unitids);
	std::vector<float3> friends;
	friends.reserve(num);
	for (int i = 0; i<num; ++i) {
		int id = unitids[i];
		float3 pos = cb->GetUnitPos(id);
		const UnitDef* ud = cb->GetUnitDef(id);
		assert(ud);
		// print owner
		char *ownerstr;
		if (cb->GetUnitTeam(id)
				== cb->GetMyTeam()) {
			ownerstr = "mine";
		} else {
			ownerstr = "allied";
		}
		statusFile << "\t" << ud->name << " " << id << " "
			<< pos.x << " " << pos.y << " " << pos.z
			<< " " << ownerstr << "\n";
		friends.push_back(pos);
	}
	// dump known enemy units
	statusFile << "units enemy\n";
	num = cheatcb->GetEnemyUnits(unitids);
	std::vector<float3> enemies;
	enemies.reserve(num);
	for (int i = 0; i<num; ++i) {
		int id = unitids[i];
		float3 pos = cheatcb->GetUnitPos(id);
		const UnitDef* ud = cheatcb->GetUnitDef(id);
		assert(ud);
		statusFile << "\t" << ud->name << " " << id << " " <<
			pos.x << " " << pos.y << " " << pos.z << "\n";
		enemies.push_back(pos);
	}

	// dump influence map
	statusFile << "influence map\n";
	statusFile << influence->mapw << " " << influence->maph << "\n";
	for (int y=0; y<influence->maph; ++y) {
		for (int x=0; x<influence->mapw; ++x) {
			statusFile << influence->map[x][y] << " ";
		}
		statusFile << "\n";
	}
	// dump other stuff
	statusFile.close();
	
	unlink(statusName);
	rename(tmpName.c_str(), statusName);

	//python->DumpStatus(frame, geovents, friends, enemies);
}

///////////////////
// spatial queries


/// union three queries
void BaczekKPAI::GetAllUnitsInRadius(std::vector<int>& vec, float3 pos, float radius)
{
	int friends[MAX_UNITS];
	int friend_cnt;
	int enemies[MAX_UNITS];
	int enemy_cnt;
	int neutrals[MAX_UNITS];
	int neutral_cnt;
	
	vec.clear();

	friend_cnt = cb->GetFriendlyUnits(friends, pos, radius);
	enemy_cnt = cheatcb->GetEnemyUnits(enemies, pos, radius);
	neutral_cnt = cheatcb->GetNeutralUnits(neutrals, pos, radius);

	vec.reserve(friend_cnt + enemy_cnt + neutral_cnt);
	std::copy(friends, friends+friend_cnt, std::back_inserter(vec));
	std::copy(enemies, enemies+enemy_cnt, std::back_inserter(vec));
	std::copy(neutrals, neutrals+neutral_cnt, std::back_inserter(vec));
}


///////////////
// pathfinder
//
// should just use GetPathLength, but it's not implemented yet!


inline float BaczekKPAI::EstimateSqDistancePF(int unitID, const float3& start, const float3& end)
{
	const UnitDef* ud = cb->GetUnitDef(unitID);
	return EstimateSqDistancePF(ud, start, end);
}

float BaczekKPAI::EstimateSqDistancePF(const UnitDef* unitdef, const float3& start, const float3& end)
{
	int pathId = cb->InitPath(start, end, unitdef->movedata->pathType);
	float sqdist = 0;
	float3 prev = start;

	while(true) {
		float3 cur;
		do {
			cur = cb->GetNextWaypoint(pathId);
		} while (cur.y == -2); // y == -2 means "try again"

		if (prev == cur) // end of path
			break;
		
		// y == -1 means no path or end of path
		if (cur.y < 0) {
			if (cur.x < 0 || cur.z < 0) { // error
				cb->FreePath(pathId);
				return -1;
			}
			else {
				// last waypoint reached
				sqdist += cur.SqDistance2D(end);
				// end here!
				break;
			}
		} else {
			sqdist += cur.SqDistance2D(prev);
			prev = cur;
		}
	}

	cb->FreePath(pathId);
	return sqdist;
}



inline float BaczekKPAI::EstimateDistancePF(int unitID, const float3& start, const float3& end)
{
	const UnitDef* ud = cb->GetUnitDef(unitID);
	return EstimateDistancePF(ud, start, end);
}

float BaczekKPAI::EstimateDistancePF(const UnitDef* unitdef, const float3& start, const float3& end)
{
	int pathId = cb->InitPath(start, end, unitdef->movedata->pathType);
	float dist = 0;
	float3 prev = start;

	while(true) {
		float3 cur;
		do {
			cur = cb->GetNextWaypoint(pathId);
		} while (cur.y == -2); // y == -2 means "try again"
		
		if (prev == cur) // end of path
			break;
		
		// y == -1 means no path or end of path
		if (cur.y < 0) {
			if (cur.x < 0 || cur.z < 0) // error
				return -1;
			else {
				// last waypoint reached
				dist += cur.distance2D(end);
				// end here!
				break;
			}
		} else {
			dist += cur.distance2D(prev);
			prev = cur;
		}
	}

	cb->FreePath(pathId);
	return dist;
}


//////////////////////////////////////////////////////////////////

float BaczekKPAI::GetGroundHeight(float x, float y)
{
	int xsquare = int(x) / SQUARE_SIZE;
	int ysquare = int(y) / SQUARE_SIZE;

	if (xsquare < 0)
		xsquare = 0;
	else if (xsquare > map.w - 1)
		xsquare = map.w - 1;
	if (ysquare < 0)
		ysquare = 0;
	else if (ysquare > map.h - 1)
		ysquare = map.h - 1;

	return cb->GetHeightMap()[xsquare + ysquare * map.h];
}

//////////////////////////////////////////////////////////////////

void BaczekKPAI::InitializeUnitDefs()
{
	int num = cb->GetNumUnitDefs();
	const UnitDef** ar = (const UnitDef **)malloc(num*sizeof(void*));
	cb->GetUnitDefList(ar);
	unitDefById.reserve(num);
	std::copy(ar, ar+num, std::back_inserter(unitDefById));
	ailog->info() << "loaded " << num << " unitdefs" << std::endl;
	free(ar);
}

