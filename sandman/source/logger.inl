#include "logger.h"

template <typename FirstT, typename... ParametersT>
inline void Logger::Write(FirstT&& first, ParametersT&&... arguments)
{
	static_assert(not std::is_same_v<std::decay_t<FirstT>, Shell::AttributeBundle>,
					  "Do not pass in attributes directly; instead use an attribute object wrapper.");

	if constexpr (Shell::IsObjectBundle<std::decay_t<FirstT>>)
	{
		// Callable to be passed into `std::apply`.
		auto const writeArgs = [this](auto&&... objects) -> void
		{
			return this->Write(std::forward<decltype(objects)>(objects)...);
		};

		if (m_screenEcho)
		{
			// Get the current data from the buffer.
			std::string const string(m_buffer.str());

			// Dump the current data to the output destinations.
			bool const didPushAttributes{
				[attributes=first.m_attributes, stringView=std::string_view(string)]() -> bool
				{
					Shell::LoggingWindow::Write(stringView);
					return Shell::LoggingWindow::PushAttributes(attributes);
				}()
			};
			m_outputStream << string;

			// Clear the buffer.
			m_buffer.str("");

			// Recursively write the objects in the object wrapper.
			std::apply(writeArgs, first.m_objects);

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
			std::apply(writeArgs, first.m_objects);
		}
	}
	else
	{
		// If the first argument is not a special parameter,
		// simply write it to the buffer.
		m_buffer << std::forward<FirstT>(first);
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
		std::string const string(m_buffer.str());

		// Dump current data to output destinations.
		if (m_screenEcho)
		{
			Shell::LoggingWindow::Write(std::string_view(string));
		}
		m_outputStream << string;

		// Clear buffer.
		m_buffer.str("");
	}
}
