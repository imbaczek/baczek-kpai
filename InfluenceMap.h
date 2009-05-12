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

	// partial updates
	int alliedProgress, enemyProgress;
	bool updateInProgress;
	bool enemiesDone;
	std::vector<int> friends, enemies;


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

	typedef std::vector<std::vector<int> > map_t;

	map_t map;
	map_t workMap;


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


	void UpdateAll(const std::vector<int>& friends,
				const std::vector<int>& enemies);

	void Update(const std::vector<int>& friends,
						  const std::vector<int>& enemies);
	void StartPartialUpdate(const std::vector<int>& friends,
						  const std::vector<int>& enemies);
	void FinishPartialUpdate();
	bool IsUpdateInProgress() { return updateInProgress; }
	bool UpdatePartial(bool allied, const std::vector<int>& uids);

	void UpdateSingleUnit(int uid, int sign, map_t& themap);

	void FindLocalMinima(float radius, std::vector<int>& values, std::vector<float3>& positions);
};
