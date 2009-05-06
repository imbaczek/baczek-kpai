#pragma once

#include <map>
#include <string>
#include <vector>

#include "float3.h"

class BaczekKPAI;

class InfluenceMap
{
protected:
	BaczekKPAI* ai;

	int lastMinimaFrame; //<? for caching purposes
	std::vector<int> minimaCachedValues;
	std::vector<float3> minimaCachedPositions;

public:
	InfluenceMap(BaczekKPAI* ai, std::string);
	~InfluenceMap();

	static const int influence_size_divisor = 8;

	std::string configName;

	struct UnitData {
		std::string name;
		int max_value;
		int min_value;
		int radius;
	};

	typedef std::map<std::string, UnitData> unit_value_map_t;
	unit_value_map_t unit_map;

	int mapw, maph;
	float scalex, scaley;

	std::vector<std::vector<int> > map;


	/* reads a config file in a format like

	{
		"kernel": {
			"max": 5,
			"min": 0,
			"radius": 10
		}
	}
	*/
	bool ReadJSONConfig();
	static void WriteDefaultJSONConfig(std::string configName);

	int GetAtXY(int x, int y);


	void Update(const std::vector<int>& friends,
				const std::vector<int>& enemies);
	void UpdateSingleUnit(int uid, int sign);

	void FindLocalMinima(float radius, std::vector<int>& values, std::vector<float3>& positions);
};
