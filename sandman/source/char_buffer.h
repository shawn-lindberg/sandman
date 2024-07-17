#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <string_view>

template <typename T>
struct Dummy;

template <typename ReturnT, typename... ParamsT>
struct Dummy<ReturnT (*)(ParamsT...)> { static ReturnT Function(ParamsT...) { return ReturnT(); } };

template <typename FunctionPointerT>
constexpr std::enable_if_t<std::is_pointer_v<FunctionPointerT>, FunctionPointerT> NonNull(FunctionPointerT const pointer=nullptr)
{
	if (pointer != nullptr) return pointer; else return Dummy<FunctionPointerT>::Function;
}

template <typename CharT, std::size_t t_Capacity>
class CharBuffer
{
	static_assert(std::is_integral_v<CharT>);

	public:
		using Data = std::array<CharT, t_Capacity>;

		// Can certainly use `CharT` as an alias for `typename Data::value_type`.
		static_assert(std::is_same_v<CharT, typename Data::value_type>);
		
		using OnCharWriteListener = void (*)(typename Data::size_type const p_Index, CharT const p_Character);

		using OnClearListener = void (*)();

	private:
		Data m_Data{};
		typename Data::size_type m_StringLength{0u};
		OnCharWriteListener m_OnCharWrite{Dummy<OnCharWriteListener>::Function};
		OnClearListener m_OnClear{Dummy<OnClearListener>::Function};

	public:
		static_assert(std::tuple_size_v<decltype(m_Data)> > 0u, "Assert can subtract from size without underflow.");

		// The last position is reserved for the null character.
		static constexpr typename Data::size_type MAX_STRING_LENGTH{ std::tuple_size_v<decltype(m_Data)> - 1u };

		[[gnu::always_inline]] constexpr Data const& GetData() const
		{
			return m_Data;
		}

		[[gnu::always_inline]] constexpr typename Data::size_type GetStringLength() const
		{
			return m_StringLength;
		}

		constexpr CharBuffer() = default;

		explicit constexpr CharBuffer(OnCharWriteListener const p_OnCharWriteListener,
												OnClearListener const p_OnClearListener)
			: m_OnCharWrite{ NonNull(p_OnCharWriteListener) },
			  m_OnClear{ NonNull(p_OnClearListener) } {};

		constexpr bool Insert(typename Data::size_type const p_Index, CharT const p_Character)
		{
			if (p_Index <= m_StringLength and m_StringLength < MAX_STRING_LENGTH)
			{
				for (typename Data::size_type index{ m_StringLength }; index > p_Index; --index)
				{
					m_OnCharWrite(index, m_Data[index] = m_Data[index - 1u]);
				}

				m_OnCharWrite(p_Index,  m_Data[p_Index] = p_Character);

				m_Data[++m_StringLength] = '\0';

				return true;
			}

			return false;
		}

		constexpr bool Push(CharT const p_Character)
		{
			if (m_StringLength < MAX_STRING_LENGTH)
			{
				m_OnCharWrite(m_StringLength, m_Data[m_StringLength] = p_Character);

				m_Data[++m_StringLength] = '\0';

				return true;
			}

			return false;
		}

		constexpr bool Remove(typename Data::size_type const p_Index)
		{
			// Can only remove if the index is a valid position in the string; 
			// the index must be strictly less than the string length.
			// The index cannot be equal to the string length because the string length is the
			// the position where the null character currently is,
			// and the null character should not be removed.
			if (p_Index < m_StringLength)
			{
				// Starting from the index of the character to remove,
				// will iterate rightward shifting each character to the left by one position
				// including the null character.
				// When the index is one position before string length,
				// that is when the null character at the string length is shifted to the left.
				// So, this operation maintains a null terminated string.
				for (typename Data::size_type l_Index{ p_Index }; l_Index < m_StringLength; ++l_Index)
				{
					// Replace the character at the current index with the
					// character to the right;
					m_OnCharWrite(l_Index, m_Data[l_Index] = m_Data[l_Index + 1u]);
				}

				// One character was removed, so decrement the string length by one.
				--m_StringLength;

				return true;
			}

			return false;
		}

		[[gnu::always_inline]] constexpr void Clear()
		{
			m_Data[m_StringLength = 0u] = '\0';
			m_OnClear();
		}

		[[gnu::always_inline]] constexpr std::string_view View() const
		{
			return std::string_view(m_Data.data(), m_StringLength);
		}
};
