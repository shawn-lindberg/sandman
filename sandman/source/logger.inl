#include "logger.h"

template <typename FirstT, typename... ParametersT>
[[gnu::always_inline]] inline void Logger::Write(FirstT&& first, ParametersT&&... arguments)
{
	// Decayed type of the first argument.

	static_assert(not std::is_same_v<std::decay_t<FirstT>, Shell::AttributeBundle>,
					  "Do not pass in attributes directly; instead use an attribute object wrapper.");

	if constexpr (Logger::IsFormatter<std::decay_t<FirstT>>{})
	{
		// If the first argument is a `Logger::Format` object,
		// then forward the arguments to a call to `Logger::FormatWrite`.
		std::apply(
			[this, formatString=first.m_FormatString](auto&&... objects) -> void
			{
				return this->FormatWrite(formatString, std::forward<decltype(objects)>(objects)...);
			},
			first.m_Objects
		);
	}
	else if constexpr (Shell::IsObjectBundle<std::decay_t<FirstT>>)
	{
		// Callable to be passed into `std::apply`.
		auto const writeArgs = [this](auto&&... objects) -> void
		{
			return this->Write(std::forward<decltype(objects)>(objects)...);
		};

		if (m_ScreenEcho)
		{
			// Get the current data from the buffer.
			std::string const string(m_Buffer.str());

			// Dump the current data to the output destinations.
			bool const didPushAttributes{
				[attributes=first.m_Attributes, stringView=std::string_view(string)]() -> bool
				{
					Shell::LoggingWindow::Write(stringView);
					return Shell::LoggingWindow::PushAttributes(attributes);
				}()
			};
			m_OutputStream << string;

			// Clear the buffer.
			m_Buffer.str("");

			// Recursively write the objects in the object wrapper.
			std::apply(writeArgs, first.m_Objects);

			// Pop the attributes object to remove its effect.
			if (didPushAttributes)
			{
				Shell::LoggingWindow::PopAttributes();
			}
		}
		else
		{
			// If screen echo is not enabled, simply recursively write the objects in the
			// object wrapper.
			std::apply(writeArgs, first.m_Objects);
		}
	}
	else
	{
		// If the first argument is not a special parameter,
		// simply write it to the buffer.
		m_Buffer << std::forward<FirstT>(first);
	}

	if constexpr (sizeof...(arguments) > 0u)
	{
		// Recursively write the remaining arguments.
		return Write(std::forward<ParametersT>(arguments)...);
	}
	else
	{
		// When there are no more arguments to write,
		// flush the current data to the output destinations,
		// and clear the buffer.

		// Get the current data from the buffer.
		std::string const string(m_Buffer.str());

		// Dump current data to output destinations.
		if (m_ScreenEcho)
		{
			Shell::LoggingWindow::Write(std::string_view(string));
		}
		m_OutputStream << string;

		// Clear buffer.
		m_Buffer.str("");
	}
}

template <typename FirstT, typename... ParametersT>
void Logger::FormatWrite(std::string_view formatString, FirstT&& first, ParametersT&&... arguments)
{
	bool escapingCharacter{false};

	for (std::string_view::size_type index{0u}; index < formatString.size(); ++index)
	{
		char const character{ formatString[index] };

		if (escapingCharacter and character == kFormatSubstitutionIndicator)
		{
			// Write the first argument, then
			// substitute the remaining arguments
			// into the rest of the string recursively.

			Write(std::forward<FirstT>(first));
			++index;
			formatString.remove_prefix(index);

			// Recursion.
			return FormatWrite(formatString, std::forward<ParametersT>(arguments)...);
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
