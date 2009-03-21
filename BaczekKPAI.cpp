// BaczekKPAI.cpp: implementation of the BaczekKPAI class.
//
////////////////////////////////////////////////////////////////////////////////

// system includes
#include <fstream>
#include <stdlib.h>
#include <cassert>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

// AI interface/Spring includes
#include "AIExport.h"
#include "ExternalAI/IGlobalAICallback.h"
#include "ExternalAI/IAICheats.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Features/FeatureDef.h"
#include "CUtils/Util.h" // we only use the defines

// project includes
#include "BaczekKPAI.h"
#include "GUI/StatusFrame.h"
#include "Log.h"
#include "InfluenceMap.h"
#include "PythonScripting.h"


namespace fs = boost::filesystem;

////////////////////////////////////////////////////////////////////////////////
// Construction/Destruction
////////////////////////////////////////////////////////////////////////////////


BaczekKPAI::BaczekKPAI()
{
	ailog = 0;
	influence = 0;
	python = 0;
}

BaczekKPAI::~BaczekKPAI()
{
	ailog->info() << "Shutting down." << endl;
	ailog->close();

	// order of deletion matters
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
	this->callback=callback;
	callback->GetCheatInterface()->EnableCheatEvents(true);

	datadir = aiexport_getDataDir(true, "");
	std::string dd(datadir);
	callback->GetAICallback()->SendTextMsg("AI data directory:", 0);
	callback->GetAICallback()->SendTextMsg(datadir, 0);

	ailog = new Log(callback);
	ailog->open(aiexport_getDataDir(true, "log.txt"));
	ailog->info() << "Logging initialized.\n";
	ailog->info() << "Baczek KP AI compiled on " __TIMESTAMP__ "\n";

	statusName = aiexport_getDataDir(true, "status.txt");
	map.h = callback->GetAICallback()->GetMapHeight();
	map.w = callback->GetAICallback()->GetMapWidth();
	map.squareSize = SQUARE_SIZE;

	FindGeovents();

	std::string influence_conf = dd+"influence.json";
	if (!fs::is_regular_file(fs::path(influence_conf))) {
		InfluenceMap::WriteDefaultJSONConfig(influence_conf);
	}
	influence = new InfluenceMap(influence_conf);

	PythonScripting::RegisterAI(team, this);
	python = new PythonScripting(team, datadir);

	
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
	myUnits.insert(unit);
}

void BaczekKPAI::UnitFinished(int unit)
{
}

void BaczekKPAI::UnitDestroyed(int unit,int attacker)
{
	myUnits.erase(unit);
}

void BaczekKPAI::EnemyEnterLOS(int enemy)
{
	enemies.insert(enemy);
}

void BaczekKPAI::EnemyLeaveLOS(int enemy)
{
	enemies.erase(enemy);
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
	enemies.erase(enemy);
}

void BaczekKPAI::UnitIdle(int unit)
{
	const UnitDef* ud=callback->GetAICallback()->GetUnitDef(unit);

	static char c[200];
	SNPRINTF(c, 200, "Idle unit %s", ud->humanName.c_str());
	callback->GetAICallback()->SendTextMsg(c, 0);
}

void BaczekKPAI::GotChatMsg(const char* msg,int player)
{
}

void BaczekKPAI::UnitDamaged(int damaged,int attacker,float damage,float3 dir)
{
}

void BaczekKPAI::UnitMoveFailed(int unit)
{
}

int BaczekKPAI::HandleEvent(int msg,const void* data)
{
	return 0; // signaling: OK
}

void BaczekKPAI::Update()
{
	int frame=callback->GetAICallback()->GetCurrentFrame();

	if ((frame % 5) == 0) {
		DumpStatus();
	}
	python->GameFrame(frame);
}

///////////////////
// helper methods

void BaczekKPAI::FindGeovents()
{
	int features[MAX_UNITS];
	int num = callback->GetCheatInterface()->GetFeatures(features, MAX_UNITS);
	ailog->info() << "found " << num << " features" << endl;
	for (int i = 0; i<num; ++i) {
		int featId = features[i];
		const FeatureDef* fd = callback->GetAICallback()->GetFeatureDef(featId);
		assert(fd);
		ailog->info() << "found feature " << fd->myName << "\n";
		if (fd->myName != "geovent")
			continue;
		float3 fpos = callback->GetAICallback()->GetFeaturePos(featId);
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
	ofstream statusFile(statusName);
	// dump map size, frame number, etc
	int frame = callback->GetAICallback()->GetCurrentFrame();
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
	int num = callback->GetAICallback()->GetFriendlyUnits(unitids);
	std::vector<float3> friends;
	friends.reserve(num);
	for (int i = 0; i<num; ++i) {
		int id = unitids[i];
		float3 pos = callback->GetAICallback()->GetUnitPos(id);
		const UnitDef* ud = callback->GetAICallback()->GetUnitDef(id);
		assert(ud);
		statusFile << "\t" << ud->name << " " << id << " " <<
			pos.x << " " << pos.y << " " << pos.z << "\n";
		friends.push_back(pos);
	}
	// dump known enemy units
	statusFile << "units enemy\n";
	num = callback->GetCheatInterface()->GetEnemyUnits(unitids);
	std::vector<float3> enemies;
	enemies.reserve(num);
	for (int i = 0; i<num; ++i) {
		int id = unitids[i];
		float3 pos = callback->GetCheatInterface()->GetUnitPos(id);
		const UnitDef* ud = callback->GetCheatInterface()->GetUnitDef(id);
		assert(ud);
		statusFile << "\t" << ud->name << " " << id << " " <<
			pos.x << " " << pos.y << " " << pos.z << "\n";
		enemies.push_back(pos);
	}

	// dump influence map
	statusFile << "influence map\n";
	// dump other stuff
	statusFile.close();

	python->DumpStatus(frame, geovents, friends, enemies);
}
