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

#include <boost/python.hpp>
#include <boost/filesystem.hpp>

#include "Log.h"
#include "PythonScripting.h"

using namespace boost::python;

static bool hasattr(boost::python::object obj, std::string const &attrName) {
     return PyObject_HasAttrString(obj.ptr(), attrName.c_str());
}

BOOST_PYTHON_MODULE(pykpai)
{
}

PythonScripting::PythonScripting(std::string datadir)
{
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

	ailog->info() << "setting sys.stdout..." << std::endl;
	object file_out = file_func(str(datadir+"/pyout.txt"), "w");
	sys.attr("stdout") = file_out;

	ailog->info() << "setting sys.stderr..." << std::endl;
	object file_err = file_func(str(datadir+"/pyerr.txt"), "w");
	sys.attr("stderr") = file_err;

	ailog->info() << "loading ai module..." << std::endl;
	init = import("pykpai");

	ailog->info() << "loading py/init.py..." << std::endl;
	try {
		exec_file(str(datadir+"/py/init.py"), main_namespace, main_namespace);
	} catch (error_already_set &e) {
		PyErr_Print();
	}
}

PythonScripting::~PythonScripting()
{
}


void PythonScripting::GameFrame(int framenum)
{
	if (hasattr(init, "game_frame")) {
		try {
			init.attr("game_frame")(framenum);
		} catch (error_already_set&) {
			PyErr_Print();
		}
	} else {
		ailog->info() << "py: game_frame(int) not defined" << std::endl;
	}
}
