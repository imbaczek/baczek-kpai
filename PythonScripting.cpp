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

	ailog->info() << "py: setting sys.stdout..." << std::endl;
	object file_out = file_func(str(datadir+"/pyout.txt"), "w");
	sys.attr("stdout") = file_out;

	ailog->info() << "py: setting sys.stderr..." << std::endl;
	object file_err = file_func(str(datadir+"/pyerr.txt"), "w");
	sys.attr("stderr") = file_err;

	init = import("pykpai");

	std::string init_py = datadir+"/py/init.py";
	boost::filesystem::path init_path(init_py);
	try {
		if (boost::filesystem::is_regular(init_path))
			exec_file(str(), main_namespace, main_namespace);
		else
			ailog->error() << init_py << " doesn't exist" << std::endl;
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
