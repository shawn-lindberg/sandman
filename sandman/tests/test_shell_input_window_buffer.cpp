#include "shell/input_window_eventful_buffer.h"

#include "catch_amalgamated.hpp"

#include <sstream>
#include <type_traits>

namespace Require
{
	// The next character after the string content should always be the null character.
	template <typename CharT, std::size_t kN>
	static void StringIsNullTerminated(Shell::InputWindow::EventfulBuffer<CharT, kN> const& buffer)
	{
		INFO('\"' << buffer.GetData().data() << "\" with string length " << buffer.GetLength()
					 << " is not null terminated correctly. "
					 << "The character at index " << buffer.GetLength() << " is \'"
					 << buffer.GetData().at(buffer.GetLength()) << "\'.");
		REQUIRE(buffer.GetData().at(buffer.GetLength()) == '\0');
	}

	template <typename CharT, std::size_t kN>
	static void ReplaceString(Shell::InputWindow::EventfulBuffer<CharT, kN>& buffer,
									  typename Shell::InputWindow::EventfulBuffer<CharT, kN>::Data::size_type const index,
									  std::string_view const string)
	{
		INFO("Attempt to replace position " << index << " with \"" << string << "\".");
		auto const originalStringLength{ buffer.GetLength() };
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
				INFO("The character to the right of this position is \'"
					  << buffer.GetData().at(index + 1u) << "\'.");
				INFO("The character to the left of this position is "
					  << (index > 0u ? std::string({ '\'', buffer.GetData().at(index - 1u), '\'' }) :
											 std::string("(none)"))
					  << ".");

				REQUIRE(buffer.GetData().at(index) == characterToCopyShiftLeft);
			}
			{
				INFO("The string length was not correctly updated.");
				REQUIRE(buffer.GetLength() == originalStringLength - (count + 1u));
			}
			{
				INFO("The string was not correctly null terminated.");
				Require::StringIsNullTerminated(buffer);
			}
		}

		{
			INFO("After removed characters, the string length of the buffer is not correct.");
			REQUIRE(buffer.GetLength() == originalStringLength - string.length());
		}

		{
			INFO("After removed characters, the buffer string is not correctly null terminated.");
			Require::StringIsNullTerminated(buffer);
		}

		for (std::string_view::size_type offset{ 0u }; offset < string.size(); ++offset)
		{
			INFO("Failed to insert character.");
			REQUIRE(buffer.Insert(index + offset, string[offset]));
		}

		{
			INFO("After inserted characters, the string length of the buffer is not correct.");
			REQUIRE(buffer.GetLength() == originalStringLength);
		}

		{
			INFO("After inserted characters, the buffer string is not correctly null terminated.");
			Require::StringIsNullTerminated(buffer);
		}
	};
} // namespace Require

TEST_CASE("`Shell, Input Window, Eventful Buffer`", "[.shell]")
{
	using namespace std::string_view_literals;

	static constexpr std::string_view kBackwardSentence(
		".god yzal eht revo spmuj xof nworb kciuq ehT"sv);

	// Initialize buffer with size of the sentence; the null character is not included in the size.
	static constexpr std::size_t kBufferCapacity{ kBackwardSentence.size() };
	Shell::InputWindow::EventfulBuffer<char, kBufferCapacity> buffer;

	SECTION("properly initialized")
	{
		// The buffer starts with an empty string of size zero.
		REQUIRE(buffer.GetLength() == 0u);
		REQUIRE(buffer.View().size() == 0u);

		static_assert(buffer.kMaxStringLength == kBufferCapacity,
						  "The maximum string length is equal to the buffer capacity."
						  "The null character is not included in the maximum string length constant.");

		static_assert(buffer.GetData().size() == kBufferCapacity + 1u,
						  "The size of the internal array is equal to "
						  "the buffer capacity plus one for the null terminator.");

		static_assert(buffer.GetData().max_size() == kBufferCapacity + 1u,
						  "The maximum size of the internal array is equal to "
						  "the buffer capacity plus one for the null terminator.");

		// All characters are initialized to the null character.
		for (char const character : buffer.GetData())
		{
			REQUIRE(character == '\0');
		}
	}

	SECTION("small string: pushing characters and remove one character")
	{
		REQUIRE(buffer.PushBack('a'));
		REQUIRE(buffer.PushBack('b'));
		REQUIRE(buffer.PushBack('c'));
		REQUIRE(buffer.PushBack('d'));
		REQUIRE(buffer.View() == "abcd"sv);
		REQUIRE(buffer.Remove(1u));
		REQUIRE(buffer.View() == "acd"sv);
	}

	SECTION("insert characters")
	{

		REQUIRE(buffer.GetLength() == 0u);

		decltype(buffer)::Data::size_type insertCount{ 0u };

		static constexpr std::string_view kForwardSentence(
			"The quick brown fox jumps over the lazy dog."sv);

		// Insert all characters in the sentence into the front of the buffer.
		for (char const character : kBackwardSentence)
		{
			CHECK(buffer.View() ==
					kForwardSentence.substr(kForwardSentence.length() - insertCount, insertCount));
			REQUIRE(buffer.Insert(0u, character));
			REQUIRE(buffer.GetLength() == ++insertCount);
		}

		Require::StringIsNullTerminated(buffer);

		// Pushing characters to the front of the buffer should work like pushing to a stack.
		REQUIRE(buffer.View() == kForwardSentence);

		// The buffer is full.
		REQUIRE(buffer.GetLength() == buffer.kMaxStringLength);

		SECTION("unchanged when attempt to insert at maximum capacity")
		{
			// Attempting to insert more characters while the buffer is at the capacity
			// should not change the contents of the string.
			for (char const character : "More text."sv)
			{
				REQUIRE_FALSE(buffer.Insert(0u, character));
			}
			Require::StringIsNullTerminated(buffer);

			// Remains unchanged.
			REQUIRE(buffer.View() == kForwardSentence);
		}

		SECTION("clear")
		{
			buffer.Clear();
			Require::StringIsNullTerminated(buffer);
			REQUIRE(buffer.View() == ""sv);
		}

		SECTION("remove and insert characters")
		{
			// "The quick brown fox jumps over the lazy dog."
			//  01234567890123456789012345678901234567890123
			//  0    5   10   15   20   25   30   35   40 43

			Require::ReplaceString(buffer, 10u, "gree"sv);
			Require::ReplaceString(buffer, 18u, "g"sv);
			Require::ReplaceString(buffer, 20u, "ho"sv);
			Require::ReplaceString(buffer, 35u, "keen"sv);
			Require::ReplaceString(buffer, 40u, "cat"sv);

			REQUIRE(buffer.GetLength() == buffer.kMaxStringLength);
			REQUIRE_FALSE(buffer.PushBack('Z'));

			REQUIRE(buffer.Remove(22u));

			for (decltype(buffer)::Data::size_type i{ 0u }; i < "quick "sv.length(); ++i)
			{
				REQUIRE(buffer.Remove(4u));
			}

			REQUIRE(buffer.Insert(17u - "quick "sv.length(), 'r'));

			REQUIRE(buffer.View() == "The green frog hops over the keen cat."sv);
		}
	}
}
