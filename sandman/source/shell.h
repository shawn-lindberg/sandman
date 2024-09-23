#pragma once

#include "common/enum.h"
#include "common/box.h"
#include "shell/attributes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <type_traits>
#include <utility>
#include <optional>

// This is the standard include directive for NCurses
// as noted in the "SYNOPSIS" section of the manual page `man 3NCURSES ncurses`.
#include <curses.h>

/// @brief This namespace serves to encapsulate state and functionality
/// relevant to the Shell user interface, and the usage of NCurses.
/// The `Shell` namespace assumes full control over the NCurses library,
/// so it is not recommended to interact with the NCurses library without going through
/// this `Shell` namespace.
///
namespace Shell
{
	using namespace std::string_view_literals;

	// This lock is implemented using a recursive mutex,
	// so multiple instances of this class can be created within the same thread.
	class [[nodiscard]] Lock final
	{
		private:
			std::lock_guard<std::recursive_mutex> m_Lock;
		public:
			[[nodiscard]] explicit Lock();
	};

	/// @brief Initialize NCurses state and other state necessary for managing the shell.
	/// Will register a signal handler for `SIGWINCH`, which will override
	/// any previous signal handler for `SIGWINCH`. This signal handler is registered
	/// in order to handle screen resizes.
	///
	/// @attention Only call this function once. Call this function successfully before
	/// calling any other functions in the `Shell` namespace.
	///
	void Initialize();

	/// @brief Uninitialize NCurses state and other logical state.
	///
	/// @attention Only call this function once.
	/// Only call this function after a successful call to `Shell::Initialize`.
	/// This does not necessarily clear the screen.
	///
	/// @note This frees the windows used by NCurses.
	///
	void Uninitialize();

	// Key constants.
	namespace Key
	{
		// `Ctrl+CharT` key combination constant.
		template <char kName, typename CharT = int>
		// Function-like constant.
		// NOLINTNEXTLINE(readability-identifier-naming)
		inline constexpr std::enable_if_t<std::is_integral_v<CharT>, CharT> Ctrl{kName bitand 0x1F};
	}

	// Log messages are printed to this window.
	namespace LoggingWindow
	{

		// Refresh the logging window.
		//
		// This applies the writes to this window that otherwise would not have been shown.
		void Refresh();

		// Write a character.
		void Write(chtype const character);

		// Write a null terminated string.
		void Write(char const* const string);

		// Write a string view.
		template <typename CharT>
		std::enable_if_t<std::is_same_v<CharT, char> or std::is_same_v<CharT, chtype>, void>
		Write(std::basic_string_view<CharT> const string)
		{
			for (CharT const character : string)
			{
				Write(static_cast<chtype>(character));
			}
		}

		// Write "true" if `true` and write "false" if `false`.
		[[gnu::always_inline]] inline void Write(bool const booleanValue)
		{
			if (booleanValue == true)
			{
				Write("true");
			}
			else
			{
				Write("false");
			}
		}

		// Use `PushAttributes` instead.
		void Write(AttributeBundle const attributes) = delete;

		// This function is deleted to stop attempts to write integral types with the expectation
		// that they will be formatted as decimal numbers on the logging window;
		// they would actually be interprected as characters if this function wasn't deleted.
		template <typename IntT>
		std::enable_if_t<std::is_integral_v<IntT>, void> Write(IntT const) = delete;

		// Push an attribute object and apply the attributes.
		//
		// Returns `true` on success, `false` otherwise.
		bool PushAttributes(AttributeBundle const attributes);

		// Pop an attribute object and revert to the previous attribute object's attributes.
		// If there is no previoues attribute object, revert to no attributes being applied.
		void PopAttributes();

		// Clear the logging window of all applied attributes and clear the stack of attribute
		// objects.
		void ClearAllAttributes();

		// Variable-argument write function.
		template <typename FirstT, typename... ParametersT>
		[[gnu::always_inline]] inline void Write(FirstT&& first, ParametersT&& ... arguments)
		{
			// Process the first argument.
			if constexpr (IsObjectBundle<std::decay_t<FirstT>>)
			{
				bool const didPushAttributes{ PushAttributes(first.m_Attributes) };

				std::apply(
					[](auto&&... objects) -> void
					{
						return Write(std::forward<decltype(objects)>(objects)...);
					},
					first.m_Objects
				);

				if (didPushAttributes)
				{
					PopAttributes();
				}
			}
			else
			{
				Write(std::forward<FirstT>(first));
			}

			// Process the other arguments; if none, clear all attributes and refresh.
			if constexpr (sizeof...(arguments) > 0u)
			{
				return Write(std::forward<ParametersT>(arguments)...);
			}
			else
			{
				ClearAllAttributes();
				Refresh();
			}
		}

		// Print one or more objects to the logging window,
		// then clear all attributes and refresh.
		template <typename... ParametersT>
		[[gnu::always_inline]] inline void Print(ParametersT&&... arguments)
		{
			Write(std::forward<ParametersT>(arguments)...);
			ClearAllAttributes();
			Refresh();
		}

		// Same as `Print`, but also print a newline character.
		template <typename... ParametersT>
		[[gnu::always_inline]] inline void PrintLine(ParametersT&&... arguments)
		{
			Print(std::forward<ParametersT>(arguments)..., chtype{'\n'});
		}

	} // namespace LoggingWindow

	// User input is echoed to this window.
	namespace InputWindow
	{
		/// @brief The starting location of the cursor for the input window.
		///
		inline constexpr int kCursorStartY{ 1 }, kCursorStartX{ 2 };

		// The input window will have a height of 3.
		inline constexpr int kRowCount{ 3 };

		// Maximum length of a string that can be submitted as input in the input window.
		inline constexpr std::size_t kMaxInputStringLength{ 1u << 7u };

		/// @brief Process a single key input from the user, if any.
		/// @warning Not thread safe.
		/// @returns `true` if the "quit" command was processed, `false` otherwise.
		///
		bool ProcessSingleUserKey();
	}

	// Adjusts the windows to the new dimensions of
	// the terminal screen if a resize occurred.
	void CheckResize();

}
