#pragma once

#if defined(_DEBUG)
// fix some _DEBUG mismatches
#	include <cstdio>

#	undef _DEBUG
#	include "Python.h"
#	define _DEBUG
#else
#	include "Python.h"
#endif

#include <map>
#include <boost/python.hpp>

namespace bp = boost::python;

class BaczekKPAI;

class PythonScripting
{
protected:
	boost::python::object init;
	int teamId;

	typedef std::map<int, BaczekKPAI*> ai_map_t;
	static ai_map_t ai_map;

public:
	PythonScripting(int teamId, std::string datadir);
	~PythonScripting();

	static void RegisterAI(int teamId, BaczekKPAI *);
	static void UnregisterAI(int teamId);
	static BaczekKPAI* GetAIForTeam(int teamId);

	void GameFrame(int framenum);
	void DumpStatus(int framenum, const std::vector<float3>& geos,
					const std::vector<float3>& friendlies,
					const std::vector<float3>& enemies);

	int GetBuilderRetreatTimeout(int frameNum);
	int GetWantedConstructors(int geospots, int mapwidth, int mapheight);
	int GetBuildSpotPriority(float distance, int influence, int mapwidth, int mapheight, int def);

	template<typename T> T extract_default(bp::object obj, T def)
	{
		try {
			return extract<T>(obj);
		} catch (bp::error_already_set&) {
			PyErr_Print();
			return def;
		}
	}

	template<typename T1, typename T2, typename Ret> Ret extract_default(bp::object obj, Ret def)
	{
		try {
			return Ret(extract<T1>(obj));
		} catch (bp::error_already_set&) {
		}
		try {
			return Ret(extract<T2>(obj));
		} catch (bp::error_already_set&) {
			PyErr_Print();
			return def;
		}
	}


	int GetIntValue(const char* name, int def);
	float GetFloatValue(const char* name, float def);
	std::string GetStringValue(const char* name, const std::string& def);
};
