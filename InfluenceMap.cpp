#include <fstream>
#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include "json_spirit/json_spirit.h"

#include "ExternalAI/IGlobalAI.h"
#include "ExternalAI/IAICheats.h"
#include "ExternalAI/IAICallback.h"
#include "ExternalAI/IGlobalAICallback.h"
#include "Sim/Units/UnitDef.h"

#include "Log.h"
#include "InfluenceMap.h"

InfluenceMap::InfluenceMap(IGlobalAICallback* cb, std::string cfg) :
configName(cfg)
{
	this->callback = cb;
	ReadJSONConfig();

	maph = cb->GetAICallback()->GetMapHeight()/influence_size_divisor;
	mapw = cb->GetAICallback()->GetMapWidth()/influence_size_divisor;
	scalex = scaley = 1./SQUARE_SIZE/influence_size_divisor;

	map.resize(mapw);
	BOOST_FOREACH(std::vector<int>& r, map) {
		r.resize(maph);
	}
}

InfluenceMap::~InfluenceMap()
{
}

/////////////////////////////////////////
// queries

int InfluenceMap::GetAtXY(int x, int y)
{
	x = x*scalex;
	y = y*scaley;
	return map[x][y];
}


/////////////////////////////////////////
// influence map updating

void InfluenceMap::Update(const std::vector<int>& friends,
						  const std::vector<int>& enemies)
{
	for (int x=0; x<mapw; ++x) {
		for (int y=0; y<maph; ++y) {
			map[x][y] = 0;
		}
	}

	BOOST_FOREACH(int uid, friends) {
		// add friends to influence map
		UpdateSingleUnit(uid, 1);
	}

	BOOST_FOREACH(int uid, enemies) {
		// add enemies to influence map
		UpdateSingleUnit(uid, -1);
	}
}

void InfluenceMap::UpdateSingleUnit(int uid, int sign)
{
	// find customized data from JSON file
	const UnitDef *ud = callback->GetCheatInterface()->GetUnitDef(uid);
	assert(ud);
	unit_value_map_t::iterator it = unit_map.find(ud->name);

	if (it == unit_map.end()) {
		// unit not found in influence map
		ailog->error() << "unit data for influence map not found for "
			<< ud->name << std::endl;
		float3 pos = callback->GetCheatInterface()->GetUnitPos(uid);
		int x = (int)(pos.x * scalex);
		int y = (int)(pos.z * scaley);
		map[x][y] += 1;
	} else {
		// unit found, add a value to influence map in given
		// UnitData.radius, with min_value at the max distance
		// and max_value at the center
		const UnitData& data = it->second;
		float3 pos = callback->GetCheatInterface()->GetUnitPos(uid);
		int x = (int)(pos.x * scalex);
		int y = (int)(pos.z * scaley);
		int minx = std::max(0, x-data.radius);
		int miny = std::max(0, y-data.radius);
		int maxx = std::min(mapw-1, x+data.radius);
		int maxy = std::min(maph-1, y+data.radius);
		int rsq = (int)(data.radius*data.radius * scalex * scaley);

		for (int px = minx; px<=maxx; ++px) {
			for (int py = miny; py<=maxy; ++py) {
				int distsq = (x-px)*(x-px) + (y-py)*(y-py);
				if (distsq > rsq)
					continue;
				float k = (float)distsq/rsq;
				map[px][py] = (int)((1-k)*data.max_value + k*data.min_value)*sign;
			}
		}
	}
}


/////////////////////////////////////////
// JSON parsing

static InfluenceMap::UnitData read_unit_data(const std::string& name, const json_spirit::Object& obj)
{
	InfluenceMap::UnitData ud;
	ud.name = name;
	ud.max_value = 0;
	ud.min_value = 0;
	ud.radius = 0;

	BOOST_FOREACH(json_spirit::Pair p, obj) {
		if (p.name_ == "max")
			ud.max_value = p.value_.get_int();
		else if (p.name_ == "min")
			ud.min_value = p.value_.get_int();
		else if (p.name_ == "radius")
			ud.radius = p.value_.get_int();
	}

	return ud;
}

bool InfluenceMap::ReadJSONConfig()
{
	if (!boost::filesystem::is_regular_file(boost::filesystem::path(configName))) {
		return false;
	}
	std::ifstream is(configName.c_str());

	json_spirit::Value value;
	if (!json_spirit::read(is, value)) {
		return false;
	}

	const json_spirit::Object& o = value.get_obj();
	BOOST_FOREACH(json_spirit::Pair p, o) {
		UnitData ud = read_unit_data(p.name_, p.value_.get_obj());
		unit_map.insert(unit_value_map_t::value_type(p.name_, ud));
	}

	return true;
}

static json_spirit::Object make_json_unit(const InfluenceMap::UnitData& ud)
{
	json_spirit::Object unit;
	json_spirit::Value maxval(ud.max_value), minval(ud.min_value),
		radius(ud.radius);
	unit.push_back(json_spirit::Pair("max", maxval));
	unit.push_back(json_spirit::Pair("min", minval));
	unit.push_back(json_spirit::Pair("radius", radius));
	return unit;
}


#define PUSH_UNIT(N, U) \
	root.push_back(json_spirit::Pair((N), make_json_unit(U)))

void InfluenceMap::WriteDefaultJSONConfig(std::string configName) {
	json_spirit::Object root;
	UnitData ud;
	// home bases
	ud.radius = 1024;
	ud.max_value = 100;
	ud.min_value = 0;
	PUSH_UNIT("kernel", ud);
	PUSH_UNIT("hole", ud);
	PUSH_UNIT("carrier", ud);
	// support bases
	ud.radius = 768;
	ud.max_value = 75;
	PUSH_UNIT("socket", ud);
	PUSH_UNIT("terminal", ud);
	PUSH_UNIT("window", ud);
	PUSH_UNIT("obelisk", ud);
	PUSH_UNIT("port", ud);
	PUSH_UNIT("firewall", ud);
	// spam units
	ud.radius = 256;
	ud.max_value = 5;
	PUSH_UNIT("bit", ud);
	PUSH_UNIT("bug", ud);
	PUSH_UNIT("exploit", ud);
	PUSH_UNIT("packet", ud);
	// heavy units
	ud.radius = 384;
	ud.max_value = 40;
	PUSH_UNIT("byte", ud);
	PUSH_UNIT("worm", ud);
	PUSH_UNIT("connection", ud);
	// arty units
	ud.radius = 512;
	ud.max_value = 20;
	PUSH_UNIT("pointer", ud);
	PUSH_UNIT("dos", ud);
	PUSH_UNIT("flow", ud);

	std::ofstream os(configName.c_str());
	json_spirit::write_formatted(root, os);
}