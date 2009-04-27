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

#include "RStarTree/RStarTree.h"

#include "Log.h"
#include "InfluenceMap.h"
#include "BaczekKPAI.h"

InfluenceMap::InfluenceMap(BaczekKPAI* theai, std::string cfg) :
configName(cfg)
{
	this->ai = theai;
	ReadJSONConfig();

	maph = ai->cb->GetMapHeight()/influence_size_divisor;
	mapw = ai->cb->GetMapWidth()/influence_size_divisor;
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

// TODO this shouldn't be here
// move to another file
typedef RStarTree<int, 2, 32, 64> RTree;
typedef RTree::BoundingBox BoundingBox;

BoundingBox bounds(int x, int y, int w, int h)
{
	BoundingBox bb;
	
	bb.edges[0].first  = x;
	bb.edges[0].second = x + w;
	
	bb.edges[1].first  = y;
	bb.edges[1].second = y + h;
	
	return bb;
} 


struct Visitor {
	int current;
	float sqradius;
	std::set<int>& list;
	const std::vector<float3>& positions;
	bool ContinueVisiting;
	Visitor(int c, float sqrad, std::set<int>& l, const std::vector<float3>& pos):
			current(c), sqradius(sqrad), list(l), positions(pos), ContinueVisiting(true) {}
	void operator()(const RTree::Leaf * const leaf) {
		if (leaf->leaf == current)
			return;
		if (list.find(leaf->leaf) != list.end())
			return;
		float sqdist = positions[current].SqDistance2D(positions[leaf->leaf]);
		if (sqdist < sqradius)
			list.insert(leaf->leaf);
	}
};

void InfluenceMap::FindLocalMinima(float radius, std::vector<int> &values, std::vector<float3> &positions)
{
	float r = radius*radius*scalex*scaley;
	values.clear();
	positions.clear();
	RTree rtree;

	// find points such that
	// a b c
	// d X f
	// g h i
	// X is min(a, b, c, d, f, g, h, i, X)
	for (int x = 0; x<mapw; ++x) {
		for (int y = 0; y<maph; ++y) {
			int v = map[x][y];
			bool found = true;
			for (int x1 = std::max(0, x-1); x1 < std::min(mapw, x+2); ++x1) {
				for (int y1 = std::max(0, y-1); y1 < std::min(maph, y+2); ++y1) {
					if (x == x1 && y == y1)
						continue;
					if (map[x1][y1] < v) {
						goto not_found;
					}
				}
			}
			if (found) {
				values.push_back(map[x][y]);
				positions.push_back(float3(x/scalex, 0, y/scaley));
				rtree.Insert(positions.size()-1, bounds(x/scalex, x/scaley, 0, 0));
			}
not_found:  ;
		}
	}

	// remove points which are too close to each other
	// according to the provided radius
	if (radius <= 0)
		return;
	std::set<int> toDel;

	for (int i = 0; i<positions.size(); ++i) {
		if (toDel.find(i) == toDel.end()) { // only if not marked for deletion already
			rtree.Query(RTree::AcceptEnclosing(bounds(positions[i].x - radius, positions[i].z - radius, 2*radius, 2*radius)),
				Visitor(i, radius*radius, toDel, positions));
		}
	}

	for (std::set<int>::reverse_iterator it = toDel.rbegin(); it != toDel.rend(); ++it) {
		values.erase(values.begin() + *it);
		positions.erase(positions.begin() + *it);
	}
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
	const UnitDef *ud = ai->cheatcb->GetUnitDef(uid);
	assert(ud);
	unit_value_map_t::iterator it = unit_map.find(ud->name);

	if (it == unit_map.end()) {
		// unit not found in influence map
		ailog->error() << "unit data for influence map not found for "
			<< ud->name << std::endl;
		float3 pos = ai->cheatcb->GetUnitPos(uid);
		int x = (int)(pos.x * scalex);
		int y = (int)(pos.z * scaley);
		map[x][y] += 1;
	} else {
		// unit found, add a value to influence map in given
		// UnitData.radius, with min_value at the max distance
		// and max_value at the center
		const UnitData& data = it->second;
		float3 pos = ai->cheatcb->GetUnitPos(uid);
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
				map[px][py] += (int)((1-k)*data.max_value + k*data.min_value)*sign;
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
	ud.max_value = 25;
	PUSH_UNIT("socket", ud);
	PUSH_UNIT("terminal", ud);
	PUSH_UNIT("window", ud);
	PUSH_UNIT("obelisk", ud);
	PUSH_UNIT("port", ud);
	PUSH_UNIT("firewall", ud);
	// spam units
	ud.radius = 512;
	ud.max_value = 5;
	PUSH_UNIT("bit", ud);
	PUSH_UNIT("bug", ud);
	PUSH_UNIT("exploit", ud);
	PUSH_UNIT("packet", ud);
	// heavy units
	ud.radius = 768;
	ud.max_value = 100;
	PUSH_UNIT("byte", ud);
	PUSH_UNIT("worm", ud);
	PUSH_UNIT("connection", ud);
	// arty units
	ud.radius = 1024;
	ud.max_value = 30;
	PUSH_UNIT("pointer", ud);
	PUSH_UNIT("dos", ud);
	PUSH_UNIT("flow", ud);

	std::ofstream os(configName.c_str());
	json_spirit::write_formatted(root, os);
}