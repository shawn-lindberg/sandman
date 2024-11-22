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

class Logger
{

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
		: m_buffer(),
		  m_outputStream(outputStream)
	{
	}

	[[nodiscard]] constexpr std::ostringstream const& GetBuffer() const
	{
		return m_buffer;
	}

	[[nodiscard]] constexpr std::ostream const& GetOutputStream() const
	{
		return m_outputStream;
	}

	[[nodiscard]] constexpr bool HasScreenEchoEnabled() const
	{
		return m_screenEcho;
	}

	[[nodiscard]] inline static bool GetEchoToScreen()
	{
		std::lock_guard const lock(ms_mutex);
		return ms_logger.m_screenEcho;
	}

	/// @brief Toggle whether the logger, in addition to writting to the log file,
	/// also prints to the screen.
	/// @warning This does not initialize or uninitialize the shell graphics system.
	inline static void SetEchoToScreen(bool const value)
	{
		std::lock_guard const lock(ms_mutex);
		ms_logger.m_screenEcho = value;
	}

	/// @brief Initializes the global logger such that it can write
	/// to the file denoted by the passed-in file name. If the
	/// file doesn't exist, then it is automatically created.
	///
	/// @warning This does not initialize the shell graphics system.
	///
	/// @returns `true` on success, `false` otherwise.
	[[nodiscard]] static bool Initialize(char const* const logFileName);

	[[nodiscard]] static bool Initialize(std::string const& logFileName)
	{
		return Initialize(logFileName.c_str());
	}

	/// Close the file associated with the global logger.
	static void Uninitialize();

	// "Higher-level" write function.
	//
	// Using the global logger's `Write` method,
	// writes a string composed of a timestamp followed by
	// the arguments of this function followed by a newline character `'\n'`.
	template <typename... ParametersT>
	inline static void WriteLine(ParametersT&&... args)
	{
		std::lock_guard const loggerLock(ms_mutex);

		// Lock Shell functionality only if screen echo is enabled.
		std::optional<Shell::Lock> const shellLock(
			ms_logger.HasScreenEchoEnabled() ? std::make_optional<Shell::Lock>() : std::nullopt);

		std::tm const* const localTime{ Common::GetLocalTime() };

		// Write the timestamp.
		if (localTime != nullptr)
		{
			ms_logger.Write(Shell::Cyan(std::put_time(localTime, "%Y/%m/%d %H:%M:%S %Z | ")));
		}
		else
		{
			using namespace std::string_view_literals;
			ms_logger.Write(Shell::Cyan("(missing local time) | "sv));
		}

		// Write the arguments with a trailing newline character.
		ms_logger.Write(std::forward<ParametersT>(args)..., '\n');

		if (ms_logger.HasScreenEchoEnabled())
		{
			Shell::LoggingWindow::ClearAllAttributes();
			Shell::LoggingWindow::Refresh();
		}
	}

protected:

	// "Lower-level" write function.
	// This simply writes data to the internal buffer,
	// then flushes the data to the output destinations.
	//
	// The internal buffer should be empty after every call to this function.
	template <typename FirstT, typename... ParametersT>
	inline void Write(FirstT&& firstArg, ParametersT&&... args);

private:

	// String data to be written to output.
	//
	// String data is first stored here before being sent to output
	// in order to transform the data into string form which both
	// the shell graphics system and the output stream understand.
	std::ostringstream m_buffer;

	// Main output destination of the logger to write data to.
	std::ostream& m_outputStream;

	// Flag governing whether this logger should write to
	// shell graphics. This is `false` by default.
	//
	// Only the global logger should be able to have this set to true
	// through a global accessor method. Other loggers should not
	// be able to set this variable with their member public interface.
	bool m_screenEcho{ false };

	// Global logger.
	static Logger ms_logger;

	// Mutex for global logger.
	static std::mutex ms_mutex;

	// The file that the global logger writes to.
	static std::ofstream ms_file;
};

#include "logger.inl"
