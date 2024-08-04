#include "logger.h"

template <typename T, typename... ParamsT>
[[gnu::always_inline]] inline void ::Logger::Write(Common::Forward<T> firstArg,
																	Common::Forward<ParamsT>... args)
{
	if constexpr (Logger::IsFormat<std::decay_t<T>>)
	{
		std::apply(
			[this, formatString=firstArg.m_FormatString](Common::Forward<auto>... args)
			{
				this->InterpolateWrite(formatString, std::forward<decltype(args)>(args)...);
			},
			firstArg.m_Objects);
	}
	else if constexpr (Shell::IsColor<std::decay_t<T>>)
	{
		std::apply(
			[this](Common::Forward<auto>... args)
			{
				this->Write(std::decay_t<T>::kOn, std::forward<decltype(args)>(args)...,
								std::decay_t<T>::kOff);
			},
			firstArg.m_Objects);
	}
	else if constexpr (std::is_same_v<std::decay_t<T>, Shell::CharacterAttribute>)
	{
		std::string const string(m_Buffer.str());
		if (m_ScreenEcho)
		{
			Shell::LoggingWindow::Write(string);
		}
		m_OutputStream << string;

		if (m_ScreenEcho)
		{
			Shell::LoggingWindow::Write(std::forward<T>(firstArg));
		}

		// Clear buffer.
		m_Buffer.str("");
	}
	else
	{
		m_Buffer << std::forward<T>(firstArg);
	}

	if constexpr (sizeof...(args) > 0u)
	{
		Write(std::forward<ParamsT>(args)...);
	}
	else
	{
		auto const string(m_Buffer.str());

		if (m_ScreenEcho)
		{
			Shell::LoggingWindow::Print(string);
		}

		m_OutputStream << string;

		// Clear buffer.
		m_Buffer.str("");
	}
}

template <typename T, typename... ParamsT>
void ::Logger::InterpolateWrite(std::string_view formatString, Common::Forward<T> firstArg,
										  Common::Forward<ParamsT>... args)
{
	bool escapingCharacter{ false };
	for (std::string_view::size_type index{ 0u }; index < formatString.size(); ++index)
	{
		char const c{ formatString[index] };
		switch (c)
		{
			case kInterpolationIndicator:
				if (escapingCharacter)
				{
					Write(c);
					escapingCharacter = false;
				}
				else
				{
					Write(std::forward<T>(firstArg));
					formatString.remove_prefix(++index);
					return InterpolateWrite(formatString, std::forward<ParamsT>(args)...);
				}
				break;
			case kEscapeIndicator:
				if (escapingCharacter)
				{
					Write(c);
					escapingCharacter = false;
				}
				else
				{
					escapingCharacter = true;
				}
				break;
			default:
				Write(c);
				break;
		}
	}
}
