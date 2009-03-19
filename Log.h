#pragma once

#include <fstream>

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

extern Log* ailog;
