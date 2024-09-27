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

void Logger::FormatWrite(std::string_view const formatString)
{
	bool escapingCharacter{ false };

	for (char const character : formatString)
	{
		if (escapingCharacter and character == kFormatSubstitutionIndicator)
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
