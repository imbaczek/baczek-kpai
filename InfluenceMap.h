#pragma once

#include <map>
#include <string>
#include <vector>

#include "ExternalAI/IGlobalAICallback.h"

class InfluenceMap
{
protected:
	IGlobalAICallback* callback;

public:
	InfluenceMap(IGlobalAICallback*, std::string);
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

	void Update(const std::vector<int>& friends,
				const std::vector<int>& enemies);
	void UpdateSingleUnit(int uid, int sign);
};
