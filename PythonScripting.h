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

#include <boost/python.hpp>

namespace bp = boost::python;

class PythonScripting
{
private:
	boost::python::object init;
public:
	PythonScripting(std::string datadir);
	~PythonScripting();

	void GameFrame(int framenum);
};
