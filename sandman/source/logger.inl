#include "logger.h"

template <typename T, typename... ParamsT>
[[gnu::always_inline]] inline void ::Logger::Write(T const& p_FirstArg, ParamsT const&... p_Args)
{
	if constexpr (NCurses::IsColor<T>)
	{
		std::apply(
			[this](auto const&... args)
			{
				this->Write(T::On, args..., T::Off);
			},
			p_FirstArg.objects);
	}
	else if constexpr (std::is_same_v<T, NCurses::Attr>)
	{
		std::string const l_String(m_Buffer.str());
		if (m_ScreenEcho)
		{
			NCurses::LoggingWindow::Write(l_String);
		}
		m_outputStream << l_String;

		if (m_ScreenEcho)
		{
			NCurses::LoggingWindow::Write(p_FirstArg);
		}

		// Clear buffer.
		m_Buffer.str("");
	}
	else
	{
		m_Buffer << p_FirstArg;
	}

	if constexpr (sizeof...(p_Args) > 0u)
	{
		Write(p_Args...);
	}
	else
	{
		auto const string(m_Buffer.str());

		if (m_ScreenEcho)
		{
			NCurses::LoggingWindow::Print(string);
		}

		m_outputStream << string;
		m_Buffer.str("");
	}
}

template <char t_InterpolationIndicator>
void ::Logger::InterpolateWrite(std::string_view const p_FormatString)
{
	using namespace std::string_view_literals;

	bool l_EscapingCharacter{ false };
	for (char const c : p_FormatString)
	{
		switch (c)
		{
			case t_InterpolationIndicator:
				if (l_EscapingCharacter)
				{
					l_EscapingCharacter = false, Write(c);
				}
				else
				{
					Write("`null`"sv);
				}
				break;
			case '\0':
				l_EscapingCharacter = true;
				break;
			default:
				Write(c);
				break;
		}
	}
}

template <char t_InterpolationIndicator, typename T, typename... ParamsT>
void ::Logger::InterpolateWrite(std::string_view p_FormatString, T const& p_FirstArg,
										  ParamsT const&... p_Args)
{
	bool l_EscapingCharacter{ false };
	for (std::string_view::size_type l_Index{ 0u }; l_Index < p_FormatString.size(); ++l_Index)
	{
		char const c{ p_FormatString[l_Index] };
		switch (c)
		{
			case t_InterpolationIndicator:
				if (l_EscapingCharacter)
				{
					l_EscapingCharacter = false;
					Write(c);
				}
				else
				{
					Write(p_FirstArg);
					p_FormatString.remove_prefix(++l_Index);
					return InterpolateWrite<t_InterpolationIndicator>(p_FormatString, p_Args...);
				}
				break;
			case '\0':
				l_EscapingCharacter = true;
				break;
			default:
				Write(c);
				break;
		}
	}
}
