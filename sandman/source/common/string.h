#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <string_view>

#include "./non_null.h"

namespace Common { template <typename, std::size_t> class String; }

/// Fixed sized eventful string.
/// (`std::string` is dynamically sized.)
template <typename CharT, std::size_t kCapacity>
class Common::String
{
	static_assert(std::is_integral_v<CharT>);

	public:
		// Type of internal data buffer.
		using Data = std::array<CharT, kCapacity>;

		// Can certainly use `CharT` as an alias for `typename Data::value_type`.
		static_assert(std::is_same_v<CharT, typename Data::value_type>);

		using OnStringUpdateListener = void (*)(typename Data::size_type const index, CharT const character);
		using OnClearListener = void (*)();
		using OnDecrementStringLengthListener = void (*)(typename Data::size_type const newStringLength);

	private:
		// Internal data buffer.
		Data m_Data{};
		typename Data::size_type m_StringLength{0u};
		Common::NonNull<OnStringUpdateListener> m_OnStringUpdate;
		Common::NonNull<OnClearListener> m_OnClear;
		Common::NonNull<OnDecrementStringLengthListener> m_OnDecrementStringLength;

	public:
		static_assert(std::tuple_size_v<Data> > 0u,
						  "Assert can subtract from size without underflow.");

		// The last position is reserved for the null character.
		static constexpr typename Data::size_type kMaxStringLength{ std::tuple_size_v<Data> - 1u };

		[[gnu::always_inline]] constexpr Data const& GetData() const
		{
			return m_Data;
		}

		[[gnu::always_inline]] constexpr typename Data::size_type GetLength() const
		{
			return m_StringLength;
		}

		explicit constexpr String() = default;

		// Construct a string with events.
		// Pass a null pointer to ignore an event.
		explicit constexpr String(
			OnStringUpdateListener          const onStringUpdateListener          ,
			OnClearListener                 const onClearListener                 ,
			OnDecrementStringLengthListener const onDecrementStringLengthListener):
			m_OnStringUpdate                     (onStringUpdateListener         ),
			m_OnClear                            (onClearListener                ),
			m_OnDecrementStringLength            (onDecrementStringLengthListener)
		{}

		// Insert a character at any valid index in the string.
		// Can insert a character at the end by inserting at
		// index string length; that is what `String::Push` does.
		//
		// Inserting a character pushes all characters after it one position to the right.
		constexpr bool Insert(typename Data::size_type const index, CharT const character)
		{
			// Can insert at any index in the string or at the end if index is string length.
			// Can only insert a character if the string is not at maximum capacity.
			if (index <= m_StringLength and m_StringLength < kMaxStringLength)
			{
				// Starting from the index of the null terminator which is at
				// index string length, will iterate leftward shifting each character
				// to the right by one position until reached index to insert
				// the the new character at.
				for (typename Data::size_type i{ m_StringLength }; i > index; --i)
				{
					m_OnStringUpdate(i, m_Data[i] = m_Data[i - 1u]);
				}

				// Use assignment to insert the character at a position.
				// Also call the event listener.
				m_OnStringUpdate(index, m_Data[index] = character);

				// Increment the string length and null terminate the string.
				m_Data[++m_StringLength] = '\0';

				// Success.
				return true;
			}

			// Failure.
			return false;
		}

		// Put a character at index string length while maintaining that the string
		// is null terminated.
		// Returns `true` on success and `false` otherwise.
		constexpr bool Push(CharT const character)
		{
			// Can only push a character if the string is not at max capacity.
			if (m_StringLength < kMaxStringLength)
			{
				// Insert the character at index string length and call the event listener.
				m_OnStringUpdate(m_StringLength, m_Data[m_StringLength] = character);

				// Update the string length and null terminate the string.
				m_Data[++m_StringLength] = '\0';

				// Success.
				return true;
			}

			// Failure.
			return false;
		}

		// Remove character at an index.
		constexpr bool Remove(typename Data::size_type const index)
		{
			// Can only remove if the index is a valid position in the string; 
			// the index must be strictly less than the string length.
			// The index cannot be equal to the string length because the string length is the
			// the position where the null character currently is,
			// and the null character should not be removed.
			if (index < m_StringLength)
			{
				// Starting from the index of the character to remove,
				// will iterate rightward shifting each character to the left by one position,
				// until but not including he null character string terminator at the index of
				// string length.
				// 
				// Since started from index of character to remove,
				// the character to remove is overwritten with the character directly
				// to its right, effectively removing it from the string.
				// 
				// It's impossible for the string length to be zero here,
				// because the removal index has to be less than the string length,
				// and the smallest the removal index can be is zero, so
				// string length must be at least one here.
				// So it is okay to subtract one from the string length here.
				for (typename Data::size_type i{ index }; i < m_StringLength - 1u; ++i)
				{
					// Replace the character at the current index with the
					// character to the right;
					m_OnStringUpdate(i, m_Data[i] = m_Data[i + 1u]);
				}

				// One character was removed, so decrement the string length by one.
				// Also, null terminate the string.
				m_Data[--m_StringLength] = '\0';

				// Call the event listener.
				m_OnDecrementStringLength(m_StringLength);

				// Success.
				return true;
			}

			// Failure.
			return false;
		}

		// Clear all characters from the "logical" string.
		// In the "physical" internal string, no characters are set to zero,
		// except for the first character at index zero which is set to the null character
		// to terminate the "empty" string.
		[[gnu::always_inline]] constexpr void Clear()
		{
			// Set the string length to zero and null terminate the string.
			m_Data[m_StringLength = 0u] = '\0';

			// Call the event listener.
			m_OnClear();

			// Success.
			return static_cast<void>(true);
		}

		// Return a string view of the string data.
		// (The returned string view does not get updated when the string object gets updated.)
		[[gnu::always_inline]] constexpr std::basic_string_view<CharT> View() const
		{
			return std::basic_string_view<CharT>(m_Data.data(), m_StringLength);
		}
};
