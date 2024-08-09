#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <string_view>

#include "./non_null.h"

namespace Common { template <typename, std::size_t> class String; }

/// Fixed sized eventful string.
/// `std::string` is dynamically sized.
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

		constexpr String() = default;

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

		constexpr bool Insert(typename Data::size_type const index, CharT const character)
		{
			if (index <= m_StringLength and m_StringLength < kMaxStringLength)
			{
				for (typename Data::size_type i{ m_StringLength }; i > index; --i)
				{
					m_OnStringUpdate(i, m_Data[i] = m_Data[i - 1u]);
				}

				m_OnStringUpdate(index, m_Data[index] = character);

				m_Data[++m_StringLength] = '\0';

				return true;
			}

			return false;
		}

		// Put a character at index string length while maintaining that the string
		// is null terminated.
		// Returns `true` on success and `false` otherwise.
		constexpr bool Push(CharT const character)
		{
			if (m_StringLength < kMaxStringLength)
			{
				// insert the character at index string length and call the event listener.
				m_OnStringUpdate(m_StringLength, m_Data[m_StringLength] = character);

				// Update the string length and null terminate the string.
				m_Data[++m_StringLength] = '\0';

				return true;
			}

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

				return true;
			}

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

			return static_cast<void>(true);
		}

		// Return a string view of the string data.
		// (The returned string view does not get updated when the string object gets updated.)
		[[gnu::always_inline]] constexpr std::basic_string_view<CharT> View() const
		{
			return std::basic_string_view<CharT>(m_Data.data(), m_StringLength);
		}
};
