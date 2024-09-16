#pragma once

#include <cstdarg>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <iomanip>

#include "shell.h"
#include "common/time_util.h"
#include "common/implicitly.h"

class Logger
{

private:

	// String data to be written to output.
	//
	// String data is first stored here before being sent to output
	// in order to transform the data into string form which both
	// the shell graphics system and the output stream understand.
	std::ostringstream m_Buffer;

	// Main output destination of the logger to write data to.
	std::ostream& m_OutputStream;

	// Flag governing whether this logger should write to
	// shell graphics. This is `false` by default.
	//
	// Only the global logger should be able to have this set to true
	// through a global accessor method. Other loggers should not
	// be able to set this variable with their member public interface.
	bool m_ScreenEcho{ false };

public:

	// Construct a logger with a reference to an output stream.
	//
	// The output stream is stored internally by reference, and can be written to over the
	// course of the lifetime of the constructed logger object.
	//
	// Since the constructed logger will store the output stream by reference,
	// it is important to ensure that the output stream is not
	// destroyed, lest the reference to the output stream that the logger stores be invalidated.
	[[nodiscard]] explicit Logger(std::ostream& outputStream)
		: m_Buffer(), m_OutputStream(outputStream) {}

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

	// "Lower-level" write function.
	// This simply writes data to the internal buffer,
	// then flushes the data to the output destinations.
	//
	// The internal buffer should be empty after every call to this function.
	template <typename FirstT, typename... ParametersT>
	[[gnu::always_inline]] inline void Write(FirstT&& firstArg, ParametersT&&... args);

	static constexpr char kFormatEscapeIndicator{ '%' };
	static constexpr char kFormatSubstitutionIndicator{ '$' };

	static constexpr std::string_view kMissingFormatValue = "`null`";

	// "Lower-level" format write function.
	// 
	// Base case for template recursion. 
	void FormatWrite(std::string_view const formatString);

	// "Lower-level" format write function.
	//
	// Substitutes arguments into the format string
	// as it writes all characters in the format string to
	// the buffer.
	//
	// `%$` substitutes an argument into the format string.
	// `%%` writes a literal percent sign `%` character.
	//
	// If substitution is denoted with `%$` but there are no more arguments left
	// to substitute, `::Logger::kMissingFormatValue` is written instead.
	//
	// The arguments are substituted into the string in the order
	// that they are passed into this function from left to right.
	//
	// If there are more arguments than there are substitution indicators `%$`
	// in the format string, the extra arguments are ignored.
	template <typename FirstT, typename... ParametersT>
	void FormatWrite(std::string_view formatString, FirstT&& firstArg, ParametersT&&... args);

	// Object wrapper for formatting.
	//
	// Instances of this class can be passed to `Logger::WriteLine`.
	// If so, the arguments will be substituted into the
	// format string and written to the buffer as if
	// `Logger::FormatWrite` was called with the arguments
	// that the `Format` instance was constructed with.
	template <typename... ObjectsT>
	struct Format
	{
		std::tuple<ObjectsT...> m_Objects;
		std::string_view m_FormatString;

		// Constructor.
		//
		// `%$` substitutes an argument into the format string.
		// `%%` writes a literal percent sign `%` character.
		//
		// If substitution is denoted with `%$` but there are no more arguments
		// to substitute, `::Logger::kMissingFormatValue` is written instead.
		//
		// The arguments are substituted into the string in the order
		// that they are passed into this function from left to right.
		//
		// If there are more arguments than there are substitution indicators `%$`
		// in the format string, the extra arguments are ignored.
		template <typename... ParametersT>
		[[nodiscard]] explicit Format(std::string_view const formatString, ParametersT&&... args)
			: m_Objects(std::forward<ParametersT>(args)...),
			  m_FormatString(formatString) {}

		// Convenience method.
		void constexpr WriteLine() const &
		{
			return Logger::WriteLine(*this);
		}

		// Convenience method.
		void constexpr WriteLine() &&
		{
			return Logger::WriteLine(std::move(*this));
		}
	};

	// Deduction guide.
	// 
	// Deduce the type of a `Format` object from the arguments passed to its constructor.
	template <typename... ParametersT>
	Format(std::string_view const, ParametersT&&...) -> Format<ParametersT&&...>;

private:

	// Global logger.
	static Logger ms_Logger;

	// Mutex for global logger.
	static std::mutex ms_Mutex;

	// The file that the global logger writes to.
	static std::ofstream ms_File;

	// This is implementation of some traits for compile-time conditionals.
	struct Traits
	{
		template <typename>
		struct IsFormat : Common::Implicitly<false> {};

		template <typename... ObjectsT>
		struct IsFormat<Format<ObjectsT...>> : Common::Implicitly<true> {};
	};

public:

	// Is `true` if `T` is a type that is a variant of the `Format` template class;
	// is `false` otherwise.
	//
	// This can be used for compile-time conditional branching.
	template <typename T>
	// NOLINTNEXTLINE(readability-identifier-naming)
	static constexpr bool IsFormat{ Logger::Traits::IsFormat<T>{} };

	[[gnu::always_inline]] [[nodiscard]] inline static bool GetEchoToScreen()
	{
		std::lock_guard const lock(ms_Mutex);
		return ms_Logger.m_ScreenEcho;
	}

	/// @brief Toggle whether the logger, in addition to writting to the log file,
	/// also prints to the screen.
	/// @warning This does not initialize or uninitialize the shell graphics system.
	[[gnu::always_inline]] inline static void SetEchoToScreen(bool const value)
	{
		std::lock_guard const lock(ms_Mutex);
		ms_Logger.m_ScreenEcho = value;
	}

	/// @brief Initializes the global logger such that it can write
	/// to the file denoted by the passed in file name. If the
	/// file doesn't exist, then it is automatically created.
	///
	/// @warning This does not initialize the shell graphics system.
	///
	/// @returns `true` on success, `false` otherwise.
	[[nodiscard]] static bool Initialize(char const* const logFileName);

	/// Close the file associated with the global logger.
	static void Uninitialize();

	// "Higher-level" write function.
	//
	// Using the global logger's `Write` method,
	// writes a string composed of a timestamp followed by
	// the arguments of this function followed by a newline character `'\n'`.
	template <typename... ParametersT>
	[[gnu::always_inline]] inline static void WriteLine(ParametersT&&... args)
	{
		std::lock_guard const loggerLock(ms_Mutex);

		// Lock Shell functionality only if screen echo is enabled.
		std::optional<Shell::Lock> const shellLock(
			ms_Logger.HasScreenEchoEnabled() ? std::make_optional<Shell::Lock>() : std::nullopt
		);

		std::tm const* const localTime{ Common::GetLocalTime() };

		// Write the timestamp.
		if (localTime != nullptr)
		{
			ms_Logger.Write(Shell::Cyan(std::put_time(localTime, "%Y/%m/%d %H:%M:%S %Z | ")));
		}
		else
		{
			using namespace std::string_view_literals;
			ms_Logger.Write(Shell::Cyan("(missing local time) | "sv));
		}

		// Write the arguments with a trailing newline character.
		ms_Logger.Write(std::forward<ParametersT>(args)..., '\n');

		if (ms_Logger.HasScreenEchoEnabled())
		{
			Shell::LoggingWindow::ClearAllAttributes();
			Shell::LoggingWindow::Refresh();
		}
	}

	static constexpr std::size_t kDefaultFormatBufferCapacity{ 1u << 11u };

	// `std::vprintf` style write function.
	//
	// The function takes arguments that are interpreted exactly how
	// `std::vprintf` would interpret them after the attributes parameter, and writes the data using
	// the global logger with a trailing newline character `'\n'`.
	// 
	// Pass `nullptr` as the attributes parameter to not use any attributes.
	//
	// Returns `true` on success, `false` otherwise.
	template <std::size_t kCapacity = kDefaultFormatBufferCapacity, typename AttributesT>
	[[gnu::format(printf, 2, 0)]] static std::enable_if_t<
		std::is_null_pointer_v<std::decay_t<AttributesT>> or
		std::is_class_v<std::decay_t<AttributesT>>,
	bool> WriteFormattedVarArgsListLine(AttributesT const attributes, char const* const format,
													std::va_list argumentList)
	{
		char logStringBuffer[kCapacity];

		// `std::vsnprintf` returns a negative value on error.
		if (std::vsnprintf(logStringBuffer, kCapacity, format, argumentList) < 0)
		{
			return false;
		}

		if constexpr (std::is_null_pointer_v<std::decay_t<AttributesT>>)
		{
			WriteLine(logStringBuffer);
		}
		else
		{
			WriteLine(attributes(logStringBuffer));
		}

		return true;
	}

	// `std::printf` style write function.
	//
	// This function takes arguments that are interpreted exactly how
	// `std::printf` would interpret them after the attributes parameter, and writes the data using
	// the global logger with a trailing newline character `'\n'`.
	//
	// Returns `true` on success, `false` otherwise.
	template <std::size_t kCapacity = kDefaultFormatBufferCapacity, typename AttributesT>
	[[gnu::format(printf, 2, 3)]]
	static std::enable_if_t<std::is_class_v<std::decay_t<AttributesT>>, bool>
		WriteFormattedLine(AttributesT const attributes, char const* const format, ...)
	{
		std::va_list argumentList;
		va_start(argumentList, format);
		bool const result{WriteFormattedVarArgsListLine<kCapacity>(attributes, format, argumentList)};
		va_end(argumentList);
		return result;
	}

	template <std::size_t = kDefaultFormatBufferCapacity, typename AttributesT>
	static bool WriteFormattedLine(AttributesT, char const* const, std::va_list) = delete;

	// `std::printf` style write function.
	//
	// This function takes arguments that are interpreted exactly how
	// `std::printf` would interpret them, and writes the data using the global logger with a
	// trailing newline character `'\n'`.
	//
	// Returns `true` on success, `false` otherwise.
	template <std::size_t kCapacity = kDefaultFormatBufferCapacity>
	[[gnu::format(printf, 1, 2)]] static bool WriteFormattedLine(char const* const format, ...)
	{
		std::va_list argumentList;
		va_start(argumentList, format);
		bool const result{WriteFormattedVarArgsListLine<kCapacity>(nullptr, format, argumentList)};
		va_end(argumentList);
		return result;
	}

	template <std::size_t = kDefaultFormatBufferCapacity>
	static bool WriteFormattedLine(char const* const, std::va_list) = delete;
};

#include "logger.inl"
