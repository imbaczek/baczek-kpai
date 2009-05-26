#include <string>

#if defined(_DEBUG)
// fix some _DEBUG mismatches
#	include <cstdio>

#	undef _DEBUG
#	include "Python.h"
#	define _DEBUG
#else
#	include "Python.h"
#endif

#include <strstream>

#include <boost/python.hpp>
#include <boost/filesystem.hpp>

#include "BaczekKPAI.h"
#include "Log.h"
#include "PythonScripting.h"

using namespace boost::python;

/////////////////////////////////////
// static members

PythonScripting::ai_map_t PythonScripting::ai_map;

void PythonScripting::RegisterAI(int teamId, BaczekKPAI *ai)
{
	ai_map.insert(PythonScripting::ai_map_t::value_type(teamId, ai));
}

void PythonScripting::UnregisterAI(int teamId)
{
	ai_map.erase(teamId);
}

BaczekKPAI* PythonScripting::GetAIForTeam(int teamId)
{
	PythonScripting::ai_map_t::iterator it = ai_map.find(teamId);
	if (it != ai_map.end()) {
		return it->second;
	} else {
		return 0;
	}
}

/////////////////////////////////////
// helpers

static bool hasattr(boost::python::object obj, std::string const &attrName) {
     return PyObject_HasAttrString(obj.ptr(), attrName.c_str());
}

/////////////////////////////////////
// module

namespace PythonFunctions
{
	void SendTextMessage(int teamId, std::string s)
	{
		BaczekKPAI* ai = PythonScripting::GetAIForTeam(teamId);
		if (!ai)
			return;
		ai->cb->SendTextMsg(s.c_str(), 0);
	}
};

static void IndexError() { PyErr_SetString(PyExc_IndexError, "Index out of range"); }

template<typename T, typename V>
struct std_item
{
    static V& get(T & x, int i)
    {
        if( i<0 ) i+=x.size();
        if( i>=0 && i<x.size() ) return x[i];
        IndexError();
    }
    static void set(T & x, int i, V const& v)
    {
        if( i<0 ) i+=x.size();
        if( i>=0 && i<x.size() ) x[i]=v;
        else IndexError();
    }
    static void del(T & x, int i)
    {
        if( i<0 ) i+=x.size();
        if( i>=0 && i<x.size() ) x.erase(x.begin()+i);
        else IndexError();
    }
    static void add(T const& x, V const& v)
    {
        x.push_back(v);
    }
};

static std::string float3_repr(const float3& f3)
{
	std::ostringstream os;
	os << "<float3 " << f3.x << ", " << f3.y << ", " << f3.z << ">";
	return os.str();
}

BOOST_PYTHON_MODULE(pykpai)
{
	class_<float3>("float3")
		.def_readwrite("x", &float3::x)
		.def_readwrite("y", &float3::y)
		.def_readwrite("z", &float3::z)
		.def("__repr__", &float3_repr)
		.def("sq_distance2d", &float3::SqDistance2D)
		;
	class_<std::vector<float3> >("vector_float3")
		.def("__len__", &std::vector<float3>::size)
		.def("__getitem__", &std_item<std::vector<float3>, float3 >::get,
			return_value_policy<copy_non_const_reference>())
		.def("__setitem__", &std_item<std::vector<float3>, float3 >::set,
			 with_custodian_and_ward<1,2>()) // to let container keep value
		.def("__delitem__", &std_item<std::vector<float3>, float3>::del)
		;
	def("SendTextMessage", PythonFunctions::SendTextMessage);
	
	// export constants
	scope().attr("GAME_SPEED") = GAME_SPEED;
	scope().attr("MAX_UNITS") = MAX_UNITS;
	scope().attr("SQUARE_SIZE") = SQUARE_SIZE;
}


/////////////////////////////////////
// methods

PythonScripting::PythonScripting(int teamId, std::string datadir)
{
	this->teamId = teamId;

	PyImport_AppendInittab( "pykpai", &initpykpai );
	Py_Initialize();

	object main_module = import("__main__");
	object main_namespace = main_module.attr("__dict__");

	object sys = import("sys");
	std::string version = extract<std::string>(sys.attr("version"));
	ailog->info() << "Python loaded.\n";
	ailog->info() << version << std::endl;

	dict main_dict = extract<dict>(main_namespace);
	object file_func = main_dict["__builtins__"].attr("file");

	ailog->info() << "py: setting sys.stdout..." << std::endl;
	object file_out = file_func(str(datadir+"/pyout.txt"), "w");
	sys.attr("stdout") = file_out;

	ailog->info() << "py: setting sys.stderr..." << std::endl;
	object file_err = file_func(str(datadir+"/pyerr.txt"), "w");
	sys.attr("stderr") = file_err;

	sys.attr("path") = boost::python::list();
	sys.attr("path").attr("append")(datadir+"/py/library.zip");
	sys.attr("path").attr("append")(datadir+"/py");
	try {
		init = import("init");
	} catch (error_already_set &) {
		PyErr_Print();
	}
}

PythonScripting::~PythonScripting()
{
}

#define PY_FUNC_SKELETON(NAME, ...) \
	object ret; \
	if (hasattr(init, NAME)) { \
		try { \
			ret = init.attr(NAME) (__VA_ARGS__); \
		} catch (error_already_set&) { \
			PyErr_Print(); \
		} \
	} else { \
		ailog->info() << "py: " NAME "(" #__VA_ARGS__ ") not defined" << std::endl; \
	}



void PythonScripting::GameFrame(int framenum)
{
	PY_FUNC_SKELETON("game_frame", teamId, framenum);
}

void PythonScripting::DumpStatus(int framenum, const std::vector<float3>& geos,
								 const std::vector<float3>& friendlies,
								 const std::vector<float3>& enemies)
{
	PY_FUNC_SKELETON("dump_status", teamId, framenum, geos, friendlies, enemies);
}


int PythonScripting::GetBuilderRetreatTimeout(int frameNum)
{
	PY_FUNC_SKELETON("get_builder_retreat_timeout", frameNum);
	return extract_default<int, double, int>(ret, frameNum+10*GAME_SPEED);
}


int PythonScripting::GetWantedConstructors(int geospots, int width, int height)
{
	PY_FUNC_SKELETON("get_wanted_constructors", geospots, width, height);
	return extract_default<int, double, int>(ret, 4);
}


int PythonScripting::GetBuildSpotPriority(float distance, int influence, int width, int height, int def)
{
	PY_FUNC_SKELETON("get_build_spot_priority", distance, influence, width, height);
	return extract_default<int, double, int>(ret, def);
}

///////////////////////////////////////////////////////////////
// generic config values

int PythonScripting::GetIntValue(const char* name, int def)
{
	PY_FUNC_SKELETON("get_config_value", name);
	return extract_default<int, double, int>(ret, def);
}


float PythonScripting::GetFloatValue(const char* name, float def)
{
	PY_FUNC_SKELETON("get_config_value", name);
	// try to extract as float first, if fails, extract int, if fails again, return default
	return extract_default<double, int, float>(ret, def);
}


std::string PythonScripting::GetStringValue(const char* name, const std::string& def)
{
	PY_FUNC_SKELETON("get_config_value", name);
	return extract_default<std::string>(ret, def);
}
