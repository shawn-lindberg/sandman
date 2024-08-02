#include "logger.h"

template <typename T, typename... ParamsT>
[[gnu::always_inline]] inline void ::Logger::Write(T const& firstArg, ParamsT const&... args)
{
	if constexpr (NCurses::IsColor<T>)
	{
		std::apply(
			[this](auto const&... args)
			{
				this->Write(T::kOn, args..., T::kOff);
			},
			firstArg.m_Objects);
	}
	else if constexpr (std::is_same_v<T, NCurses::CharacterAttribute>)
	{
		std::string const string(m_Buffer.str());
		if (m_ScreenEcho)
		{
			NCurses::LoggingWindow::Write(string);
		}
		m_OutputStream << string;

		if (m_ScreenEcho)
		{
			NCurses::LoggingWindow::Write(firstArg);
		}

		// Clear buffer.
		m_Buffer.str("");
	}
	else
	{
		m_Buffer << firstArg;
	}

	if constexpr (sizeof...(args) > 0u)
	{
		Write(args...);
	}
	else
	{
		auto const string(m_Buffer.str());

		if (m_ScreenEcho)
		{
			NCurses::LoggingWindow::Print(string);
		}

		m_OutputStream << string;
		m_Buffer.str("");
	}
}

template <typename T, typename... ParamsT>
void ::Logger::InterpolateWrite(std::string_view formatString, T const& firstArg,
										  ParamsT const&... args)
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
					Write(firstArg);
					formatString.remove_prefix(++index);
					return InterpolateWrite(formatString, args...);
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
