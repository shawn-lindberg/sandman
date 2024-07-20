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

[[gnu::format(printf, 1, 2)]] bool Logger::FormatWriteLine(char const* p_Format, ...)
{
	static constexpr std::size_t s_LogStringBufferCapacity{ 2048u };
	char l_LogStringBuffer[s_LogStringBufferCapacity];

	std::va_list l_Arguments;
	va_start(l_Arguments, p_Format);
	std::vsnprintf(l_LogStringBuffer, s_LogStringBufferCapacity, p_Format, l_Arguments);
	va_end(l_Arguments);

	WriteLine(l_LogStringBuffer);

	return true;
}
