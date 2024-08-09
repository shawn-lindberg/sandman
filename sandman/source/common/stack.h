#pragma once

#include <array>
#include <optional>
#include <utility>

namespace Common { template <typename, std::size_t> class Stack; }

/// Fixed size stack data structure.
/// `std::stack` is dynamically sized.
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

	// Return pointer to value at index. Returns `nullptr` on out of bounds.
	[[nodiscard]] constexpr T const* operator[](SizeType const index) const
	{
		return index < m_Size ? &m_Data[index] : nullptr;
	}

	// Return pointer to value at index. Returns `nullptr` on out of bounds.
	[[nodiscard]] constexpr T* operator[](SizeType const index)
	{
		return const_cast<T*>(std::as_const(*this)[index]);
	}

	// Return pointer to value at index 0. Returns `nullptr` on out of bounds.
	[[nodiscard]] constexpr T const* GetBottom() const
	{
		return (*this)[0u];
	}

	// Return pointer to value at index 0. Returns `nullptr` on out of bounds.
	[[nodiscard]] constexpr T* GetBottom()
	{
		return const_cast<T*>(std::as_const(*this).GetBottom());
	}

	// Return pointer to value at index size minus one. Returns `nullptr` on out of bounds.
	[[nodiscard]] constexpr T const* GetTop() const
	{
		return (*this)[m_Size - 1u];
	}

	// Return pointer to value at index size minus one. Returns `nullptr` on out of bounds.
	[[nodiscard]] constexpr T* GetTop()
	{
		return const_cast<T*>(std::as_const(*this).GetTop());
	}

	// Push an object to the stack at index size. Returns `true` on success, `false` otherwise.
	template <typename ObjectT>
	constexpr bool Push(ObjectT&& object)
	{
		return m_Size < kMaxSize ? (m_Data[m_Size++] = std::forward<ObjectT>(object), true) : false;
	}

	// Removes an object from the stack from index size minus one.
	// Returns optional with the popped object on success, `std::nullopt` otherwise.
	constexpr std::optional<T> Pop()
	{
		return m_Size > 0u ? std::make_optional(std::move(m_Data[--m_Size])) : std::nullopt;
	}

	// Clears the stack of all objects. Size is zero after call to this function.
	// Internally, this only sets size to zero and does not destroy any objects.
	constexpr void Clear()
	{
		m_Size = 0u;
	}
};
