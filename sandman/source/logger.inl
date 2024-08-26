#include "logger.h"

template <typename T, typename... ParamsT>
[[gnu::always_inline]] inline void ::Logger::Write(T&& firstArg, ParamsT&&... args)
{
	static_assert(not std::is_same_v<std::decay_t<T>, Shell::AttributeBundle>,
					  "Do not pass in attributes directly; instead use an attribute object wrapper.");

	if constexpr (Logger::IsFormat<std::decay_t<T>>)
	{
		// If the first argument is a `Logger::Format` object,
		// then forward the arguments to a call to `Logger::FormatWrite`.
		std::apply(
			[this, formatString=firstArg.m_FormatString](auto&&... objects) -> void
			{
				return this->FormatWrite(formatString, std::forward<decltype(objects)>(objects)...);
			},
			firstArg.m_Objects
		);
	}
	else if constexpr (Shell::IsAttrWrapper<std::decay_t<T>>)
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
			bool const didPushAttributes
			{
				[attributes=firstArg.m_Attributes, stringView=std::string_view(string)]() -> bool
				{
					::Shell::LoggingWindow::Write(stringView);
					return ::Shell::LoggingWindow::PushAttributes(attributes);
				}()
			};
			m_OutputStream << string;

			// Clear the buffer.
			m_Buffer.str("");

			// Recursively write the objects in the object wrapper.
			std::apply(writeArgs, firstArg.m_Objects);

			// Pop the attributes object to remove its effect.
			if (didPushAttributes)
			{
				::Shell::LoggingWindow::PopAttributes();
			}
		}
		else
		{
			// If screen echo is not enabled, simply recursively write the objects in the
			// object wrapper.
			std::apply(writeArgs, firstArg.m_Objects);
		}
	}
	else
	{
		// If the first argument is not a special parameter,
		// simply write it to the buffer.
		m_Buffer << std::forward<T>(firstArg);
	}

	if constexpr (sizeof...(args) > 0u)
	{
		// Recursively write the remaining arguments.
		return Write(std::forward<ParamsT>(args)...);
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
			::Shell::LoggingWindow::Write(std::string_view(string));
		}
		m_OutputStream << string;

		// Clear buffer.
		m_Buffer.str("");
	}
}

template <typename T, typename... ParamsT>
void ::Logger::FormatWrite(std::string_view formatString, T&& firstArg, ParamsT&&... args)
{
	for (
		auto [index, escapingCharacter] = std::tuple{std::string_view::size_type{ 0u }, false};
		index < formatString.size();
		++index
	)
	{
		char const c{ formatString[index] };

		if (escapingCharacter)
		{
			switch (c)
			{
				case kFormatInterpolationIndicator:
					// Write the first argument, then
					// interpolate the remaining arguments
					// into the rest of the string recursively.

					Write(std::forward<T>(firstArg));
					formatString.remove_prefix(++index);

					// Recursion.
					return FormatWrite(formatString, std::forward<ParamsT>(args)...);

				default:
					// An escaped character that isn't special is simply written.
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
