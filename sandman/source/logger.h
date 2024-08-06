#pragma once

#include <cstdarg>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>

#include "shell.h"
#include "common/implicitly.h"

class Logger
{

private:

	std::ostringstream m_Buffer;
	std::ostream& m_OutputStream;
	bool m_ScreenEcho{ false };

public:

	[[nodiscard]] Logger(std::ostream& outputStream): m_Buffer(), m_OutputStream(outputStream) {}

	template <typename T, typename... ParamsT>
	[[gnu::always_inline]] inline void Write(Common::Forward<T> firstArg,
														  Common::Forward<ParamsT>... args);

	static constexpr char kFormatEscapeIndicator{ '%' }, kFormatInterpolationIndicator{ '$' };

	void FormatWrite(std::string_view const formatString);

	template <typename T, typename... ParamsT>
	void FormatWrite(std::string_view formatString, Common::Forward<T> firstArg,
						  Common::Forward<ParamsT>... args);

	template <typename... ObjectsT>
	struct Format
	{
		std::tuple<ObjectsT...> m_Objects;
		std::string_view m_FormatString;

		template <typename... ParamsT>
		[[nodiscard]] explicit Format(std::string_view const formatString,
												Common::Forward<ParamsT>... args)
			: m_Objects(std::forward<ParamsT>(args)...),
			  m_FormatString(formatString) {}
	};

	// Deduction guide: deduce from forwarding reference arguments.
	template <typename... ParamsT>
	Format(std::string_view const, Common::Forward<ParamsT>...) -> Format<Common::Forward<ParamsT>...>;

private:

	static std::mutex ms_Mutex;
	static std::ofstream ms_File;
	static Logger ms_GlobalLogger;

	struct Traits
	{
		template <typename>
		struct IsFormat : Common::Implicitly<false> {};

		template <typename... ObjectsT>
		struct IsFormat<Format<ObjectsT...>> : Common::Implicitly<true> {};
	};

public:

	template <typename T>
	static constexpr bool IsFormat{ ::Logger::Traits::IsFormat<T>{} };

	[[gnu::always_inline]] [[nodiscard]] inline static bool& GetScreenEchoFlag()
	{
		return ms_GlobalLogger.m_ScreenEcho;
	}

	[[nodiscard]] static bool Initialize(char const* const logFileName);

	static void Uninitialize();

	template <typename... ParamsT>
	[[gnu::always_inline]] inline static void WriteLine(Common::Forward<ParamsT>... args);

	template <Shell::ColorIndex kColor = Shell::ColorIndex::None,
				 std::size_t kLogStringBufferCapacity = 2048u>
	[[gnu::format(printf, 1, 0)]] static bool FormatWriteLine(char const* format,
																				 std::va_list argumentList);

	template <Shell::ColorIndex kColor = Shell::ColorIndex::None,
				 std::size_t kLogStringBufferCapacity = 2048u>
	[[gnu::format(printf, 1, 2)]] static bool FormatWriteLine(char const* format, ...);

	template <typename... ParamsT>
	static void InterpolateWriteLine(std::string_view const formatString, Common::Forward<ParamsT>... args);
};

#include "logger.inl"
#include "logger_global.inl"
