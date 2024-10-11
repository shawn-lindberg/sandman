#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <string_view>

namespace Shell::InputWindow { template <typename, std::size_t> class EventfulBuffer; }

/// Fixed sized eventful buffer.
template <typename CharT, std::size_t kMaxStringLengthValue>
class Shell::InputWindow::EventfulBuffer
{
	static_assert(std::is_integral_v<CharT>);

	public:
		static constexpr std::size_t kMaxStringLength{ kMaxStringLengthValue };

		static_assert(kMaxStringLength + 1u != 0u,
						  "The maximum string length causes overflow "
						  "because adding one to it results in zero.");

		// Type of internal data buffer.
		using Data = std::array<CharT, kMaxStringLength + 1u /* add one for the null terminator */>;

		// Can certainly use `CharT` as an alias for `typename Data::value_type`.
		static_assert(std::is_same_v<CharT, typename Data::value_type>);

		using OnStringUpdateListener = void (*)(typename Data::size_type const index, CharT const character);
		using OnClearListener = void (*)();
		using OnDecrementStringLengthListener = void (*)(typename Data::size_type const newStringLength);

	private:
		// Internal data buffer.
		Data m_data{};

		// The last position is reserved for the null character.
		static_assert(kMaxStringLengthValue == std::tuple_size_v<Data> - 1u,
						  "The last position is reserved for the null character.");

		typename Data::size_type m_stringLength{0u};

		OnStringUpdateListener          m_onStringUpdate         ;
		OnClearListener                 m_onClear                ;
		OnDecrementStringLengthListener m_onDecrementStringLength;

	public:
		static_assert(std::tuple_size_v<Data> > 0u,
						  "Assert can subtract from size without underflow.");

		constexpr Data const& GetData() const
		{
			return m_data;
		}

		constexpr typename Data::size_type GetLength() const
		{
			return m_stringLength;
		}

		explicit constexpr EventfulBuffer() = default;

		// Construct a string with events.
		// Pass a null pointer to ignore an event.
		explicit constexpr EventfulBuffer(
			OnStringUpdateListener          const onStringUpdateListener          ,
			OnClearListener                 const onClearListener                 ,
			OnDecrementStringLengthListener const onDecrementStringLengthListener):
			m_onStringUpdate                     (onStringUpdateListener         ),
			m_onClear                            (onClearListener                ),
			m_onDecrementStringLength            (onDecrementStringLengthListener)
		{}

		// Insert a character at any valid index in the string.
		// Can insert a character at the end by inserting at
		// index string length; that is what `PushBack` does.
		//
		// Inserting a character pushes all characters after it one position to the right.
		constexpr bool Insert(typename Data::size_type const insertionIndex, CharT const character)
		{
			// Can insert at any index in the string or at the end if index is string length.
			// Can only insert a character if the string is not at maximum capacity.
			if (not(insertionIndex <= m_stringLength and m_stringLength < kMaxStringLength))
			{
				// Failure.
				return false;
			}

			// Starting from the index of the null terminator which is at
			// index string length, will iterate leftward shifting each character
			// to the right by one position until reached index to insert the new character at.
			for (typename Data::size_type index{ m_stringLength }; index > insertionIndex; --index)
			{
				m_onStringUpdate(index, m_data[index] = m_data[index - 1u]);
			}

			// Use assignment to insert the character at a position.
			// Also call the event listener.
			m_onStringUpdate(insertionIndex, m_data[insertionIndex] = character);

			// Increment the string length and null terminate the string.
			++m_stringLength;
			m_data[m_stringLength] = '\0';

			// Success.
			return true;
		}

		// Put a character at index string length while maintaining that the string
		// is null terminated.
		// Returns `true` on success and `false` otherwise.
		constexpr bool PushBack(CharT const character)
		{
			// Can only push a character if the string is not at max capacity.
			if (m_stringLength < kMaxStringLength)
			{
				// Insert the character at index string length and call the event listener.
				m_onStringUpdate(m_stringLength, m_data[m_stringLength] = character);

				// Update the string length and null terminate the string.
				++m_stringLength;
				m_data[m_stringLength] = '\0';

				// Success.
				return true;
			}

			// Failure.
			return false;
		}

		// Remove character at an index.
		constexpr bool Remove(typename Data::size_type const removalIndex)
		{
			// Can only remove if the index is a valid position in the string; 
			// the index must be strictly less than the string length.
			// The index cannot be equal to the string length because the string length is the
			// the position where the null character currently is,
			// and the null character should not be removed.
			if (removalIndex < m_stringLength)
			{
				// Starting from the index of the character to remove,
				// will iterate rightward shifting each character to the left by one position,
				// until but not including the null character string terminator at the index of
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
				for (typename Data::size_type index{ removalIndex }; index < m_stringLength - 1u; ++index)
				{
					// Replace the character at the current index with the
					// character to the right;
					m_onStringUpdate(index, m_data[index] = m_data[index + 1u]);
				}

				// One character was removed, so decrement the string length by one.
				// Also, null terminate the string.
				--m_stringLength;
				m_data[m_stringLength] = '\0';

				// Call the event listener.
				m_onDecrementStringLength(m_stringLength);

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
		constexpr void Clear()
		{
			// Set the string length to zero and null terminate the string.
			m_data[m_stringLength = 0u] = '\0';

			// Call the event listener.
			m_onClear();
		}

		// Return a string view of the string data.
		// (The returned string view does not get updated when the string object gets updated.)
		constexpr std::basic_string_view<CharT> View() const
		{
			return std::basic_string_view<CharT>(m_data.data(), m_stringLength);
		}
};
