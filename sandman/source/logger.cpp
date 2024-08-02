#include "logger.h"

std::mutex Logger::ms_Mutex;
std::ofstream Logger::ms_File;
Logger Logger::ms_GlobalLogger(ms_File);

bool ::Logger::Initialize(char const* const logFileName)
{
	if (logFileName == nullptr)
	{
		return false;
	}

	{
		std::lock_guard const lock(ms_Mutex);

		ms_File.open(logFileName);

		if (not ms_File)
		{
			return false;
		}
	}

	return true;
}

void ::Logger::Uninitialize()
{
	std::lock_guard const lock(ms_Mutex);
	ms_File.close();
}
