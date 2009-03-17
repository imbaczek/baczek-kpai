#pragma once

#include <map>
#include <string>

class InfluenceMap
{
public:
	InfluenceMap(std::string);
	~InfluenceMap();

	std::string configName;

	struct UnitData {
		std::string name;
		int max_value;
		int min_value;
		int radius;
	};

	typedef std::map<std::string, UnitData> unit_value_map_t;
	unit_value_map_t unit_map;


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
};
