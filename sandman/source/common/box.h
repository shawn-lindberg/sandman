#pragma once

namespace Common
{
	template <typename T>
	struct Box
	{
		T m_Value;

		[[nodiscard]] explicit constexpr Box(T const& value) : m_Value(value) {}

		[[nodiscard]] explicit constexpr Box(T&& value) : m_Value(std::move(value)) {}
	};
}
