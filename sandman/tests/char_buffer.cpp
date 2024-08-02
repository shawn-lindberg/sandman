#include "char_buffer.h"

#include "catch_amalgamated.hpp"

namespace Require
{
	// The next character after the string content should always be the null character.
	template <typename CharT, std::size_t kCapacity>
	static void StringNullTerminated(CharBuffer<CharT, kCapacity> const& buffer)
	{
		INFO('\"' << buffer.GetData().data() << "\" with string length "
					 << buffer.GetStringLength() << " is not null terminated correctly. "
					 << "The character at index " << buffer.GetStringLength() << " is \'"
					 << buffer.GetData().at(buffer.GetStringLength()) << "\'.");
		REQUIRE(buffer.GetData().at(buffer.GetStringLength()) == '\0');
	}

	template <typename CharT, std::size_t kCapacity>
	static void ReplaceString(CharBuffer<CharT, kCapacity>& buffer,
									  typename CharBuffer<CharT, kCapacity>::Data::size_type const index,
									  std::string_view const string)
	{
		INFO("Attempt to replace position " << index << " with \"" << string << "\".");
		auto const originalStringLength{ buffer.GetStringLength() };
		for (std::string_view::size_type count{ 0u }; count < string.length(); ++count)
		{
			auto const characterToRemove{ buffer.GetData().at(index) };
			auto const characterToCopyShiftLeft{ buffer.GetData().at(index + 1u) };
			INFO("On attempt to remove single character.");
			INFO("Characters removed successfully: " << count);
			INFO("Character to remove: \'" << characterToRemove << '\'');
			INFO("Character to copy shift left: \'" << characterToCopyShiftLeft << '\'');
			{
				INFO("Failed to remove character.");
				REQUIRE(buffer.Remove(index));
			}
			INFO("String: \"" << buffer.GetData().data() << '\"');
			{
				INFO("The character to the right of the position to remove "
					  "was not copy shifted left correctly.");
				INFO("The character to the right of this position is \'" << buffer.GetData().at(index + 1u)
																		  << "\'.");
				INFO("The character to the left of this position is " <<
					  (index > 0u ?
							std::string({ '\'', buffer.GetData().at(index - 1u), '\''}) :
							std::string("(none)")) <<
					  ".");

				REQUIRE(buffer.GetData().at(index) == characterToCopyShiftLeft);
			}
			{
				INFO("The string length was not correctly updated.");
				REQUIRE(buffer.GetStringLength() == originalStringLength - (count + 1u));
			}
			{
				INFO("The string was not correctly null terminated.");
				Require::StringNullTerminated(buffer);
			}
		}

		{
			INFO("After removed characters, the string length of the buffer is not correct.");
			REQUIRE(buffer.GetStringLength() == originalStringLength - string.length());
		}

		{
			INFO("After removed characters, the buffer string is not correctly null terminated.");
			Require::StringNullTerminated(buffer);
		}

		for (std::string_view::size_type offset{ 0u }; offset < string.size(); ++offset)
		{
			INFO("Failed to insert character.");
			REQUIRE(buffer.Insert(index + offset, string[offset]));
		}

		{
			INFO("After inserted characters, the string length of the buffer is not correct.");
			REQUIRE(buffer.GetStringLength() == originalStringLength);
		}

		{
			INFO("After inserted characters, the buffer string is not correctly null terminated.");
			Require::StringNullTerminated(buffer);
		}
	};
} // namespace Require

TEST_CASE("`CharBuffer`", "[.CharBuffer]")
{
	using namespace std::string_view_literals;

	static constexpr std::string_view kBackwardSentence(
		".god yzal eht revo depmuj xof nworb ehT"sv);

	// Initialize buffer with size of the sentence plus one for null character.
	static constexpr std::size_t kBufferCapacity{ kBackwardSentence.size() + 1u };
	CharBuffer<char, kBufferCapacity> buffer;

	SECTION("properly initialized")
	{
		// The buffer starts with an empty string of size zero.
		REQUIRE(buffer.GetStringLength() == 0u);
		REQUIRE(buffer.View().size() == 0u);

		static_assert(
			buffer.kMaxStringLength == kBufferCapacity - 1u,
			"The maximum string length is the buffer capacity minus one because"
			"the last character in the buffer is reserved for the null character"
			"to remain compatible with functions that expect strings to be null terminated.");

		static_assert(buffer.GetData().size() == kBufferCapacity,
						  "The size of the internal array is the buffer capacity.");

		static_assert(buffer.GetData().max_size() == kBufferCapacity,
						  "The maximum size of the internal array is the buffer capacity.");

		// All characters are initialized to the null character.
		for (char const character : buffer.GetData())
		{
			REQUIRE(character == '\0');
		}
	}

	SECTION("small string")
	{
		REQUIRE(buffer.Push('a'));
		REQUIRE(buffer.Push('b'));
		REQUIRE(buffer.Push('c'));
		REQUIRE(buffer.Push('d'));
		REQUIRE(buffer.View() == "abcd"sv);
		REQUIRE(buffer.Remove(1u));
		REQUIRE(buffer.View() == "acd"sv);
	}

	SECTION("insert characters")
	{
		// Insert all characters in the sentence into the front of the buffer.
		for (char const character : kBackwardSentence)
		{
			REQUIRE(buffer.Insert(0u, character));
		}

		Require::StringNullTerminated(buffer);

		static constexpr std::string_view kForwardSentence("The brown fox jumped over the lazy dog."sv);

		// Pushing characters to the front of the buffer should work like pushing to a stack.
		REQUIRE(buffer.View() == kForwardSentence);

		// The buffer is full.
		REQUIRE(buffer.GetStringLength() == buffer.kMaxStringLength);

		SECTION("unchanged when attempt to insert at maximum capacity")
		{
			// Attempting to insert more characters while the buffer is at the capacity
			// should not change the contents of the string.
			for (char const character : "More text."sv)
			{
				REQUIRE_FALSE(buffer.Insert(0u, character));
			}
			Require::StringNullTerminated(buffer);

			// Remains unchanged.
			REQUIRE(buffer.View() == kForwardSentence);
		}

		SECTION("clear")
		{
			buffer.Clear();
			Require::StringNullTerminated(buffer);
			REQUIRE(buffer.View() == ""sv);
		}

		SECTION("remove and insert characters")
		{
			Require::ReplaceString(buffer, 4u, "green"sv);

			Require::ReplaceString(buffer, 14u, "hopped"sv);

			Require::ReplaceString(buffer, 35u, "cat"sv);

			Require::ReplaceString(buffer, 12u, "g"sv);

			Require::ReplaceString(buffer, 30u, "m"sv);

			Require::ReplaceString(buffer, 32u, "d"sv);

			REQUIRE(buffer.Remove(33u));

			REQUIRE(buffer.Insert(11u, 'r'));

			REQUIRE(buffer.View() == "The green frog hopped over the mad cat."sv);
		}
	}
}
