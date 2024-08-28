#include "logger.h"

std::mutex Logger::ms_Mutex;
std::ofstream Logger::ms_File;
Logger Logger::ms_Logger(ms_File);

bool Logger::Initialize(char const* const logFileName)
{
	if (logFileName == nullptr)
	{
		return false;
	}

	{
		std::lock_guard const lock(ms_Mutex);

		ms_File.open(logFileName);

		if (not ms_File.is_open())
		{
			// Failed to open the file.
			return false;
		}
	}

	return true;
}

void Logger::Uninitialize()
{
	std::lock_guard const lock(ms_Mutex);

	// Closing the file stream also flushes any remaining data to the file.
	ms_File.close();
}

void Logger::FormatWrite(std::string_view const formatString)
{
	bool escapingCharacter{ false };

	for (char const character : formatString)
	{
		if (escapingCharacter and character == kFormatInterpolationIndicator)
		{
			// Don't have any arguments to write, so write a placeholder.
			Write(kMissingFormatValue);
		}
		else if (character == kFormatEscapeIndicator and not escapingCharacter)
		{
			escapingCharacter = true;
			continue;
		}
		else
		{
			Write(character);
		}
		escapingCharacter = false;
	}
}
