#include "logger.h"

std::mutex Logger::ms_mutex;
std::ofstream Logger::ms_file;
Logger Logger::ms_logger(ms_file);

bool Logger::Initialize(char const* const logFileName)
{
	if (logFileName == nullptr)
	{
		return false;
	}

	{
		std::lock_guard const lock(ms_mutex);

		ms_file.open(logFileName);

		if (not ms_file.is_open())
		{
			// Failed to open the file.
			return false;
		}
	}

	return true;
}

void Logger::Uninitialize()
{
	std::lock_guard const lock(ms_mutex);

	// Closing the file stream also flushes any remaining data to the file.
	ms_file.close();
}
