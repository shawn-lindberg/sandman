#pragma once

#include <array>
#include <optional>
#include <utility>

#include "./forward_alias.h"

namespace Common { template <typename, std::size_t> class Stack; }

template <typename T, std::size_t kCapacity>
class Common::Stack
{

	static_assert(std::is_default_constructible_v<T>, "`T` should be default constructible.");

private:

	std::array<T, kCapacity> m_Data;

public:

	using SizeType = typename decltype(m_Data)::size_type;

	static constexpr SizeType kMaxSize{ kCapacity };

private:

	SizeType m_Size{ 0u };

public:

	[[nodiscard]] constexpr auto GetSize() const { return m_Size; }

	[[nodiscard]] constexpr T const* operator[](SizeType const index) const
	{
		return index < m_Size ? &m_Data[index] : nullptr;
	}

	[[nodiscard]] constexpr T* operator[](SizeType const index)
	{
		return std::as_const(*this)[index];
	}

	[[nodiscard]] constexpr T const* GetBottom() const { return (*this)[0u]; }

	[[nodiscard]] constexpr T* GetBottom()
	{
		return std::as_const(*this).GetBottom();
	}

	[[nodiscard]] constexpr T const* GetTop() const { return (*this)[m_Size - 1u]; }

	[[nodiscard]] constexpr T* GetTop()
	{
		return std::as_const(*this).GetTop();
	}

	template <typename ObjectT>
	constexpr bool Push(Common::Forward<ObjectT> object)
	{
		return m_Size < kMaxSize ? (m_Data[m_Size++] = std::forward<ObjectT>(object), true) : false;
	}

	constexpr std::optional<T> Pop()
	{
		return m_Size > 0u ? std::move(m_Data[--m_Size]) : std::nullopt;
	}

};
