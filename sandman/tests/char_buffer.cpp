#include "common/char_buffer.h"

#include "catch_amalgamated.hpp"

#include <sstream>
#include <type_traits>

#if false
inline namespace Debug
{

	template <typename CharT, std::size_t kN>
	std::ostream& operator<<(std::ostream& outputStream, Common::CharBuffer<CharT, kN> const& buffer)
	{
		outputStream << "Buffer[";
		for (typename Common::CharBuffer<CharT, kN>::Data::size_type i{ 0u }; i < kN; ++i)
		{
			if (i == buffer.GetStringLength()) { outputStream << '@'; continue; }
			CharT const c{ buffer.GetData().at(i) };
			switch (c)
			{
				case '\0': outputStream << '0'; break;
				default: outputStream << c; break;
			}
		}
		outputStream << ']';
		return outputStream;
	}

	template <typename>
	static constexpr bool kIsCharBuffer{ false };

	template <typename CharT, std::size_t kN>
	static constexpr bool kIsCharBuffer<Common::CharBuffer<CharT, kN>>{ true };

	template <typename T>
	[[nodiscard]] std::string ToString(T&& object)
	{
		std::ostringstream outputStream;
		outputStream << std::forward<T>(object);
		return outputStream.str();
	}

	template <typename... ParamsT>
	static void Println(ParamsT&&... args)
	{
		(Catch::cout() << ... << std::forward<ParamsT>(args)) << '\n';
	}

} // namespace Debug
#endif

namespace Require
{
	// The next character after the string content should always be the null character.
	template <typename CharT, std::size_t kN>
	static void StringNullTerminated(Common::CharBuffer<CharT, kN> const& buffer)
	{
		INFO('\"' << buffer.GetData().data() << "\" with string length "
					 << buffer.GetStringLength() << " is not null terminated correctly. "
					 << "The character at index " << buffer.GetStringLength() << " is \'"
					 << buffer.GetData().at(buffer.GetStringLength()) << "\'.");
		REQUIRE(buffer.GetData().at(buffer.GetStringLength()) == '\0');
	}

	template <typename CharT, std::size_t kN>
	static void
		ReplaceString(Common::CharBuffer<CharT, kN>& buffer,
						  typename Common::CharBuffer<CharT, kN>::Data::size_type const index,
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
		".god yzal eht revo spmuj xof nworb kciuq ehT"sv
		// ".god yzal eht revo depmuj xof nworb ehT"sv
	);

	// Initialize buffer with size of the sentence plus one for null character terminator.
	static constexpr std::size_t kBufferCapacity{ kBackwardSentence.size() + 1u };
	Common::CharBuffer<char, kBufferCapacity> buffer;

	SECTION("properly initialized")
	{
		// The buffer starts with an empty string of size zero.
		REQUIRE(buffer.GetStringLength() == 0u);
		REQUIRE(buffer.View().size() == 0u);

		static_assert(
			buffer.kMaxStringLength == kBufferCapacity - 1u,
			"The maximum string length is the buffer capacity minus one because" " "
			"the last character in the buffer is reserved for the null character" " "
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

	SECTION("small string: pushing characters and remove one character")
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

		REQUIRE(buffer.GetStringLength() == 0u);

		decltype(buffer)::Data::size_type insertCount{ 0u };

		static constexpr std::string_view kForwardSentence(
			"The quick brown fox jumps over the lazy dog."sv);

		// Insert all characters in the sentence into the front of the buffer.
		for (char const character : kBackwardSentence)
		{
			CHECK(buffer.View() ==
					kForwardSentence.substr(kForwardSentence.length() - insertCount, insertCount));
			REQUIRE(buffer.Insert(0u, character));
			REQUIRE(buffer.GetStringLength() == ++insertCount);
		}

		Require::StringNullTerminated(buffer);

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
			// "The quick brown fox jumps over the lazy dog."
			//  01234567890123456789012345678901234567890123
			//  0    5   10   15   20   25   30   35   40 43

			Require::ReplaceString(buffer, 10u, "gree"sv);
			Require::ReplaceString(buffer, 18u, "g"sv);
			Require::ReplaceString(buffer, 20u, "ho"sv);
			Require::ReplaceString(buffer, 35u, "keen"sv);
			Require::ReplaceString(buffer, 40u, "cat"sv);

			REQUIRE(buffer.GetStringLength() == buffer.kMaxStringLength);
			REQUIRE_FALSE(buffer.Push('Z'));

			REQUIRE(buffer.Remove(22u));

			for (decltype(buffer)::Data::size_type i{ 0u }; i < ("agile "sv).length(); ++i)
			{
				REQUIRE(buffer.Remove(4u));
			}

			REQUIRE(buffer.Insert(17u - ("agile "sv).length(), 'r'));

			REQUIRE(buffer.View() == "The green frog hops over the keen cat."sv);
		}
	}
}
