#include "logger.h"


bool Logger::g_ScreenEcho{ false };

std::mutex Logger::Self::s_Mutex;
std::ofstream Logger::Self::s_File;
std::ostringstream Logger::Self::s_Buffer;

bool ::Logger::Initialize(char const* const p_LogFileName)
{
	if (p_LogFileName == nullptr) return false;
	std::lock_guard const l_Lock(Self::s_Mutex);
	Self::s_File = std::ofstream(p_LogFileName);
	return true;
}

void Logger::Uninitialize()
{
	std::lock_guard const l_Lock(Self::s_Mutex);
	Self::s_File.close();
}
