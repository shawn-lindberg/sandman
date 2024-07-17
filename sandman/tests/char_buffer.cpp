#include "char_buffer.h"

#include "catch_amalgamated.hpp"

namespace Debug {
	template <typename CharT>
	static constexpr std::enable_if_t<std::is_integral_v<CharT>, std::string> ToString(CharT const p_Character) {
		switch (p_Character)
		{
			case '\0': return "NULL";
			default: return {p_Character};
		}
	}
}

namespace Require
{
	// The next character after the string content should always be the null character.
	template <typename CharT, std::size_t t_Capacity>
	static void StringNullTerminated(CharBuffer<CharT, t_Capacity> const& p_Buffer)
	{
		INFO('\"' << p_Buffer.GetData().data() << "\" with string length "
					 << p_Buffer.GetStringLength() << " is not null terminated correctly. "
					 << "The character at index " << p_Buffer.GetStringLength() << " is \'"
					 << p_Buffer.GetData().at(p_Buffer.GetStringLength()) << "\'.");
		REQUIRE(p_Buffer.GetData().at(p_Buffer.GetStringLength()) == '\0');
	}

	template <typename CharT, std::size_t t_Capacity>
	static void ReplaceString(CharBuffer<CharT, t_Capacity>& p_Buffer,
									  typename CharBuffer<CharT, t_Capacity>::Data::size_type const p_Index,
									  std::string_view const string)
	{
		INFO("Attempt to replace position " << p_Index << " with \"" << string << "\".");
		auto const l_OriginalStringLength{ p_Buffer.GetStringLength() };
		for (std::string_view::size_type count{ 0u }; count < string.length(); ++count)
		{
			auto const l_CharacterToRemove{ p_Buffer.GetData().at(p_Index) };
			auto const l_CharacterToCopyShiftLeft{ p_Buffer.GetData().at(p_Index + 1u) };
			INFO("On attempt to remove single character.");
			INFO("Characters removed successfully: " << count);
			INFO("Character to remove: \'" << l_CharacterToRemove << '\'');
			INFO("Character to copy shift left: \'" << l_CharacterToCopyShiftLeft << '\'');
			{
				INFO("Failed to remove character.");
				REQUIRE(p_Buffer.Remove(p_Index));
			}
			INFO("String: \"" << p_Buffer.GetData().data() << '\"');
			{
				INFO("The character to the right of the position to remove "
					  "was not copy shifted left correctly.");
				INFO("The character to the right of this position is \'" << p_Buffer.GetData().at(p_Index + 1u)
																		  << "\'.");
				INFO("The character to the left of this position is " <<
					  (p_Index > 0u ?
							std::string({ '\'', p_Buffer.GetData().at(p_Index - 1u), '\''}) :
							std::string("(none)")) <<
					  ".");

				REQUIRE(p_Buffer.GetData().at(p_Index) == l_CharacterToCopyShiftLeft);
			}
			{
				INFO("The string length was not correctly updated.");
				REQUIRE(p_Buffer.GetStringLength() == l_OriginalStringLength - (count + 1u));
			}
			{
				INFO("The string was not correctly null terminated.");
				Require::StringNullTerminated(p_Buffer);
			}
		}

		{
			INFO("After removed characters, the string length of the buffer is not correct.");
			REQUIRE(p_Buffer.GetStringLength() == l_OriginalStringLength - string.length());
		}

		{
			INFO("After removed characters, the buffer string is not correctly null terminated.");
			Require::StringNullTerminated(p_Buffer);
		}

		for (std::string_view::size_type l_Offset{ 0u }; l_Offset < string.size(); ++l_Offset)
		{
			INFO("Failed to insert character.");
			REQUIRE(p_Buffer.Insert(p_Index + l_Offset, string[l_Offset]));
		}

		{
			INFO("Adter inserted characters, the string length of the buffer is not correct.");
			REQUIRE(p_Buffer.GetStringLength() == l_OriginalStringLength);
		}

		{
			INFO("After inserted characters, the buffer string is not correctly null terminated.");
			Require::StringNullTerminated(p_Buffer);
		}
	};
} // namespace Require

TEST_CASE("`CharBuffer`", "[.CharBuffer]")
{
	using namespace std::string_view_literals;

	static constexpr std::string_view s_BackwardSentence(
		".god yzal eht revo depmuj xof nworb ehT"sv);

	// Initialize buffer with size of the sentence plus one for null character.
	static constexpr std::size_t s_BufferCapacity{ s_BackwardSentence.size() + 1u };
	CharBuffer<char, s_BufferCapacity> l_Buffer;

	SECTION("properly initialized")
	{
		// The buffer starts with an empty string of size zero.
		REQUIRE(l_Buffer.GetStringLength() == 0u);
		REQUIRE(l_Buffer.View().size() == 0u);

		static_assert(
			l_Buffer.MAX_STRING_LENGTH == s_BufferCapacity - 1u,
			"The maximum string length is the buffer capacity minus one because"
			"the last character in the buffer is reserved for the null character"
			"to remain compatible with functions that expect strings to be null terminated.");

		static_assert(l_Buffer.GetData().size() == s_BufferCapacity,
						  "The size of the internal array is the buffer capacity.");

		static_assert(l_Buffer.GetData().max_size() == s_BufferCapacity,
						  "The maximum size of the internal array is the buffer capacity.");

		// All characters are initialized to the null character.
		for (char const l_Character : l_Buffer.GetData())
			REQUIRE(l_Character == '\0');
	}

	SECTION("small string")
	{
		REQUIRE(l_Buffer.Push('a'));
		REQUIRE(l_Buffer.Push('b'));
		REQUIRE(l_Buffer.Push('c'));
		REQUIRE(l_Buffer.Push('d'));
		REQUIRE(l_Buffer.View() == "abcd"sv);
		REQUIRE(l_Buffer.Remove(1u));
		REQUIRE(l_Buffer.View() == "acd"sv);
	}

	SECTION("insert characters")
	{
		// Insert all characters in the sentence into the front of the buffer.
		for (char const l_Character : s_BackwardSentence) REQUIRE(l_Buffer.Insert(0u, l_Character));
		Require::StringNullTerminated(l_Buffer);

		static constexpr std::string_view s_ForwardSentence(
			"The brown fox jumped over the lazy dog."sv);

		// Pushing characters to the front of the buffer should work like pushing to a stack.
		REQUIRE(l_Buffer.View() == s_ForwardSentence);

		// The buffer is full.
		REQUIRE(l_Buffer.GetStringLength() == l_Buffer.MAX_STRING_LENGTH);

		SECTION("unchanged when attempt to insert at maximum capacity")
		{
			// Attempting to insert more characters while the buffer is at the capacity
			// should not change the contents of the string.
			for (char const l_Character : "More text."sv)
				REQUIRE_FALSE(l_Buffer.Insert(0u, l_Character));
			Require::StringNullTerminated(l_Buffer);

			// Remains unchanged.
			REQUIRE(l_Buffer.View() == s_ForwardSentence);
		}

		SECTION("clear")
		{
			l_Buffer.Clear();
			Require::StringNullTerminated(l_Buffer);
			REQUIRE(l_Buffer.View() == ""sv);
		}

		SECTION("remove and insert characters")
		{
			Require::ReplaceString(l_Buffer, 4u, "green"sv);

			Require::ReplaceString(l_Buffer, 14u, "hopped"sv);

			Require::ReplaceString(l_Buffer, 35u, "cat"sv);

			REQUIRE(l_Buffer.View() == "The green fox hopped over the lazy cat.");
		}
	}
}
