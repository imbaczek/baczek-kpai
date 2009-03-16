#include <fstream>
#include <algorithm>
#include <boost/foreach.hpp>

#include "json_spirit/json_spirit.h"

#include "InfluenceMap.h"

InfluenceMap::InfluenceMap(std::string cfg) :
configName(cfg)
{
	ReadJSONConfig();
}

InfluenceMap::~InfluenceMap()
{
}

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