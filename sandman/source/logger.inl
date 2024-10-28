#include "logger.h"

template <typename FirstT, typename... ParametersT>
inline void Logger::Write(FirstT&& first, ParametersT&&... arguments)
{
	// Assert that something like `Shell::Red` on it's own is not passed in.
	static_assert(not std::disjunction_v<
						  std::is_same<std::decay_t<FirstT>, Shell::AttributeBundle>,
						  std::is_same<std::decay_t<FirstT>, Shell::AttributeBundle::ForegroundColor>,
						  std::is_same<std::decay_t<FirstT>, Shell::AttributeBundle::BackgroundColor>,
						  std::is_same<std::decay_t<FirstT>, Shell::AttributeBundle::ColorPair>>,
					  "Do not pass in attributes directly; instead use an attribute object wrapper.");

	/*
		How this algorithm works:
		1. Process the first argument.
			a. If the first argument is a bundle of objects, process all those objects recursively.
			b. Otherwise, if the first argument is a single object, just process that single object.
		2. Process the remaining arguments recursively, if any.
			a. If there are more arguments to process, recurse.
			b. Otherwise, if there are no more arguments,
				dump the contents and clear the buffer because we are done.
	*/

	// 1. Process the first argument.
	if constexpr (Shell::kIsObjectBundle<std::decay_t<FirstT>>)
	{
		// 1a. Need to process all the objects in the object bundle.

		// Callable to be passed into `std::apply`. This is just a wrapper around this function.
		auto const writeArgs = [this](auto&&... objects) -> void
		{
			return this->Write(std::forward<decltype(objects)>(objects)...);
		};

		if (m_screenEcho)
		{
			/*
				If screen echo is enabled, will take care to process the shell attributes
				correctly. The shell's attributes should be pushed and then popped once
				all the objects in the bundle have been processed.
			*/

			// Get the current data from the buffer.
			std::string const string(m_buffer.str());

			// Dump the current data to the output destinations.
			Shell::LoggingWindow::Write(std::string_view(string));
			m_outputStream << string;

			// Clear the buffer.
			m_buffer.str("");

			bool const didPushAttributes{ Shell::LoggingWindow::PushAttributes(first.m_attributes) };

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
			/*
				If screen echo is not enabled,
				simply recursively write the objects in the object wrapper.
				We don't need to worry about shell attributes here.
			*/
			std::apply(writeArgs, first.m_objects);
		}
	}
	else
	{
		// 1b. If the first argument is not a special parameter, simply write it to the buffer.
		m_buffer << std::forward<FirstT>(first);
	}

	// 2. Process the remaining arguments recursively, if any.
	if constexpr (sizeof...(arguments) > 0u)
	{
		// 2a. Recursively write the remaining arguments.
		return Write(std::forward<ParametersT>(arguments)...);
	}
	else
	{
		/*
			2b. When there are no more arguments to write,
			flush the current data to the output destinations,
			and clear the buffer.
		*/

		// Get the current data from the buffer.
		std::string const string(m_buffer.str());

		// Dump current data to output destinations.
		if (m_screenEcho)
		{
			Shell::LoggingWindow::Write(std::string_view(string));
		}
		m_outputStream << string;
		m_outputStream.flush();

		// Clear buffer.
		m_buffer.str("");
	}
}
