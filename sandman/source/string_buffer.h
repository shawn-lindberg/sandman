#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

template <typename CharT, std::size_t t_Capacity>
class StringBuffer
{
	static_assert(std::is_integral_v<CharT>);

	public:
		using Data = std::array<CharT, t_Capacity>;

	private:
		Data m_Data;
		typename Data::size_type m_StringLength;

	public:
		static_assert(std::tuple_size_v<decltype(m_Data)> > 0u,
						"Assert can subtract from size without underflow.");

		// The last position is reserved for the null character.
		static constexpr typename Data::size_type MAX_STRING_LENGTH{ std::tuple_size_v<decltype(m_Data)> - 1u };

		[[gnu::always_inline]] constexpr Data const& GetData()
		{
			return m_Data;
		}

		[[gnu::always_inline]] constexpr typename Data::size_type GetStringLength()
		{
			return m_StringLength;
		}

		[[gnu::always_inline]] constexpr StringBuffer(Data p_Data = {}): m_Data(std::move(p_Data)), m_StringLength{ 0u } {}

		constexpr bool Insert(typename Data::size_type const p_Index,
									typename Data::value_type const p_Character)
		{
			if (p_Index <= m_StringLength and m_StringLength < MAX_STRING_LENGTH)
			{
				for (typename Data::size_type index{ m_StringLength }; index > p_Index; --index)
				{
					m_Data[index] = m_Data[index - 1u];
				}

				m_Data[p_Index] = p_Character;

				m_Data[++m_StringLength] = '\0';

				return true;
			}

			return false;
		}

		constexpr bool Remove(typename Data::size_type const p_Index)
		{
			if (p_Index < m_StringLength)
			{
				for (typename Data::size_type index{ m_StringLength }; index < m_StringLength; ++index)
				{
					m_Data[p_Index] = m_Data[index + 1u];
				}

				return true;
			}

			return false;
		}

		[[gnu::always_inline]] constexpr void Clear()
		{
			m_Data[m_StringLength = 0u] = '\0';
		}
};
