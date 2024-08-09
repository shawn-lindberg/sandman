#pragma once

#include "common/enum.h"
#include "common/forward_alias.h"
#include "common/box.h"
#include "shell_attr.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <type_traits>
#include <utility>

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

	class [[nodiscard]] Lock
	{
		private: std::lock_guard<std::mutex> m_Lock;
		public: [[nodiscard]] explicit Lock();
	};

	/// @brief Initialize NCurses state and other logical state for managing the shell.
	///
	/// @attention Only call this function once. Call this function successfully before
	/// calling any other functions in the `Shell` namespace.
	///
	void Initialize();

	/// @brief Uninitialize NCurses state and other logical state.
	///
	/// @attention Only call this function once.
	/// Only call this function after a successfull call to `Shell::Initialize`.
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
	namespace LoggingWindow {

		// Refresh the logging window.
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

		// `PushAttributes` instead.
		void Write(Attr const attributes) = delete;

		// This function is deleted to stop from writing integral types with the expectation
		// that they will be formated as decimal numbers on the logging window;
		// they would actually be interprected as characters if this function wasn't deleted.
		template <typename IntT>
		std::enable_if_t<std::is_integral_v<IntT>, void> Write(IntT const) = delete;

		// The maximum amount of attribute objects that the stack can contain.
		// (Each attribute object can still contain several Curses attributes though.)
		inline constexpr std::size_t kMaxAttributeObjectCount{ 1u << 7u };

		// Push an attribute object and apply the attributes.
		void PushAttributes(Attr const attributes);

		// Pop an attribute object and revert to the previous attribute object's attributes.
		// If there is no previoues attribute object, revert to no attributes being applied.
		void PopAttributes();

		// Clear the window of all applied attributes and clear the stack of attribute objects.
		void ClearAllAttributes();

		// Print one or more objects to a the logging window,
		// then clear all attributes and refresh.
		template <Attr::Value kAttributes=Normal.m_Value, typename T, typename... ParamsT>
		[[gnu::always_inline]] inline void Print(T const object, ParamsT const... arguments)
		{
			PushAttributes(Attr(kAttributes));

			Write(object);

			if constexpr (sizeof...(arguments) > 0u)
			{
				return Print(arguments...);
			}
			else
			{
				ClearAllAttributes();
				Refresh();
			}
		}

		// Same as `Print`, but also print a newline character.
		template <Attr::Value kAttributes=Normal.m_Value, typename... ParamsT>
		[[gnu::always_inline]] inline void Println(ParamsT const... arguments)
		{
			Print<kAttributes>(arguments..., chtype{'\n'});
		}

		/// @brief Get the pointer to the logging window.
		///
		/// @attention Do not call this function before having called `Shell::Initialize`
		/// successfully.
		///
		/// @return NCurses window pointer
		///
		/// @warning If `Shell::Initialize` has not been called successfully, this function likely
		/// returns `nullptr`. Otherwise, the pointer returned by this function is valid until
		/// `Shell::Uninitialize` is called.
		///
		/// @note The logging window is the region on the terminal where the logger outputs characters
		/// to. After `Shell::Initialize` is called successfully, this function always returns the
		/// same pointer.
		///
		[[deprecated("Manage this window through other functions.")]] [[nodiscard]] WINDOW* Get();

	} // namespace LoggingWindow

	namespace InputWindow
	{
		/// @brief The starting location of the cursor for the input window.
		///
		inline constexpr int kCursorStartY{ 1 }, kCursorStartX{ 2 };

		// The input window will have a height of 3.
		inline constexpr int kRowCount{ 3 };

		/// @brief Get the pointer to the input window.
		///
		/// @attention Do not call this function before having called `Shell::Initialize`
		/// successfully.
		///
		/// @return NCurses window pointer
		///
		/// @warning If `Shell::Initialize` has not been called successfully, this function likely
		/// returns returns `nullptr`. Otherwise, the pointer returned by this function is valid until
		/// `Shell::Uninitialize` is called.
		///
		/// @note The input window is the region on the terminal where the user input is echoed to.
		/// After `Shell::Initialize` is called successfully, this function always returns the same
		/// pointer.
		///
		[[deprecated("Manage this window through other functions.")]] [[nodiscard]] WINDOW* Get();

		// Maximum length of a string that can be submitted as input in the input window.
		inline constexpr std::size_t kMaxInputStringLength{ 1u << 7u };

		/// @brief Process a single key input from the user, if any.
		/// @warning Not thread safe.
		/// @returns `true` if the "quit" command was processed, `false` otherwise.
		///
		bool ProcessSingleUserKey();
	}

}
