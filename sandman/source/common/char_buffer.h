#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <string_view>

#include "common/non_null.h"

namespace Common { template <typename, std::size_t> class CharBuffer; }

template <typename CharT, std::size_t kCapacity>
class Common::CharBuffer
{
	static_assert(std::is_integral_v<CharT>);

	public:
		using Data = std::array<CharT, kCapacity>;

		// Can certainly use `CharT` as an alias for `typename Data::value_type`.
		static_assert(std::is_same_v<CharT, typename Data::value_type>);

		using OnStringUpdateListener = void (*)(typename Data::size_type const index, CharT const character);
		using OnClearListener = void (*)();
		using OnDecrementStringLengthListener = void (*)(typename Data::size_type const newStringLength);

	private:
		Data m_Data{};
		typename Data::size_type m_StringLength{0u};
		Common::NonNull<OnStringUpdateListener> m_OnStringUpdate;
		Common::NonNull<OnClearListener> m_OnClear;
		Common::NonNull<OnDecrementStringLengthListener> m_OnDecrementStringLength;

	public:
		static_assert(std::tuple_size_v<decltype(m_Data)> > 0u, "Assert can subtract from size without underflow.");

		// The last position is reserved for the null character.
		static constexpr typename Data::size_type kMaxStringLength{ std::tuple_size_v<decltype(m_Data)> - 1u };

		[[gnu::always_inline]] constexpr Data const& GetData() const
		{
			return m_Data;
		}

		[[gnu::always_inline]] constexpr typename Data::size_type GetStringLength() const
		{
			return m_StringLength;
		}

		constexpr CharBuffer() = default;

		explicit constexpr CharBuffer(
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

		constexpr bool Push(CharT const character)
		{
			if (m_StringLength < kMaxStringLength)
			{
				m_OnStringUpdate(m_StringLength, m_Data[m_StringLength] = character);

				m_Data[++m_StringLength] = '\0';

				return true;
			}

			return false;
		}

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
				// will iterate rightward shifting each character to the left by one position.
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

				m_OnDecrementStringLength(m_StringLength);

				return true;
			}

			return false;
		}

		[[gnu::always_inline]] constexpr void Clear()
		{
			m_Data[m_StringLength = 0u] = '\0';
			m_OnClear();

			return static_cast<void>(true);
		}

		[[gnu::always_inline]] constexpr std::string_view View() const
		{
			return std::string_view(m_Data.data(), m_StringLength);
		}
};
