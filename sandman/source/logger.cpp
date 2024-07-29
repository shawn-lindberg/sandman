#include "logger.h"

std::mutex Logger::s_Mutex;
std::ofstream Logger::s_File;
Logger Logger::s_GlobalLogger(s_File);

bool ::Logger::Initialize(char const* const p_LogFileName)
{
	if (p_LogFileName == nullptr)
	{
		return false;
	}

	{
		std::lock_guard const l_Lock(s_Mutex);

		s_File.open(p_LogFileName);

		if (not s_File)
		{
			return false;
		}
	}

	return true;
}

void ::Logger::Uninitialize()
{
	std::lock_guard const l_Lock(s_Mutex);
	s_File.close();
}
