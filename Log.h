#pragma once

#include <fstream>
#include <boost/shared_ptr.hpp>

#ifdef _MSC_VER
#pragma warning (disable: 4996) // secure iterators
#pragma warning (disable: 4244) // int->float conversions
#pragma warning (disable: 4800) // ...->bool conversions (performance)
#endif


#include "AIExport.h"
#include "ExternalAI/IGlobalAICallback.h"


class Log
{
	std::ofstream logfile;
	IGlobalAICallback* callback;
public:

	Log(IGlobalAICallback* cb):callback(cb)
	{
	}

	~Log()
	{
		logfile.close();
	}

	void flush() { logfile.flush(); }
	void open(const char* name) { logfile.open(name); }
	void close() { logfile.close(); }

	std::ofstream& error() { flush(); logfile << "ERROR:"; return logfile; }
	std::ofstream& info() { flush(); logfile << "INFO:"; return logfile; }
};

extern boost::shared_ptr<Log> ailog;
