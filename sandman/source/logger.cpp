#include "logger.h"

std::mutex Logger::ms_Mutex;
std::ofstream Logger::ms_File;
Logger Logger::ms_Logger(ms_File);

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

void ::Logger::FormatWrite(std::string_view const formatString)
{
	using namespace std::string_view_literals;

	bool escapingCharacter{ false };

	for (char const c : formatString)
	{
		if (escapingCharacter)
		{
			switch (c)
			{
				case kFormatInterpolationIndicator:
					Write("`null`"sv);
					break;
				default:
					Write(c);
					break;
			}
			escapingCharacter = false;
		}
		else
		{
			switch (c)
			{
				case kFormatEscapeIndicator:
					escapingCharacter = true;
					break;
				default:
					Write(c);
					break;
			}
		}
	}
}
