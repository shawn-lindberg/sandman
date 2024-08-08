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

	[[nodiscard]] constexpr std::ostringstream const& GetBuffer() const
	{
		return m_Buffer;
	}

	[[nodiscard]] constexpr std::ostream const& GetOutputStream() const
	{
		return m_OutputStream;
	}

	[[nodiscard]] constexpr bool HasScreenEchoEnabled() const
	{
		return m_ScreenEcho;
	}

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

	// Deduction guide.
	template <typename... ParamsT>
	Format(std::string_view const,
			 Common::Forward<ParamsT>...) -> Format<Common::Forward<ParamsT>...>;

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

	[[nodiscard]] static constexpr Logger const& Get()
	{
		return ms_GlobalLogger;
	}

	template <typename T>
	// NOLINTNEXTLINE(readability-identifier-naming)
	static constexpr bool IsFormat{ ::Logger::Traits::IsFormat<T>{} };

	[[gnu::always_inline]] [[nodiscard]] inline static bool& GetScreenEchoFlag()
	{
		return ms_GlobalLogger.m_ScreenEcho;
	}

	[[nodiscard]] static bool Initialize(char const* const logFileName);

	static void Uninitialize();

	template <typename... ParamsT>
	[[gnu::always_inline]] inline static void WriteLine(Common::Forward<ParamsT>... args);

	static constexpr Shell::Attr::Value kDefaultColorPair
	{
		Shell::GetColorPair(Shell::Fg(Shell::ColorIndex::White),
								  Shell::Bg(Shell::ColorIndex::Black)).m_Value
	};

	template <Shell::Attr::Value = kDefaultColorPair, std::size_t kStringBufferCapacity = 1u << 11u>
	[[gnu::format(printf, 1, 0)]] static bool FormatWriteLine(char const* format,
																				 std::va_list argumentList);

	template <Shell::Attr::Value = kDefaultColorPair, std::size_t kStringBufferCapacity = 1u << 11u>
	[[gnu::format(printf, 1, 2)]] static bool FormatWriteLine(char const* format, ...);


};

#include "logger_local.inl"
#include "logger_global.inl"
