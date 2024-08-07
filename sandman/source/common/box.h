#pragma once


namespace Common
{
	template<typename ValueT>
	struct Box
	{
		ValueT m_Value;

		[[nodiscard]] constexpr operator ValueT() const
		{
			return m_Value;
		}
	};
}
