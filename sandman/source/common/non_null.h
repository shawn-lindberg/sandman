#pragma once

#include <cassert>

namespace Common
{
	/// Wrapper around a pointer that is not null.
	template <typename> class NonNull;
}

// Wrapper around a function pointer that is not null.
// If initialized with a null pointer, the internal pointer value is not set to null pointer;
// the internal pointer value is instead initialized with a function with no side effects
// that returns a default value.
template <typename ReturnT, typename... ParametersT>
class Common::NonNull<ReturnT (*)(ParametersT...)>
{
public:
	using FunctionPointerT = ReturnT (*)(ParametersT...);

	/// Does nothing with the parameters, and returns a default constructed value.
	[[gnu::always_inline]] static constexpr ReturnT Simulacrum(ParametersT...)
	{
		return ReturnT{};
	}

private:
	FunctionPointerT m_function;

public:
	[[nodiscard]] constexpr NonNull() : m_function{ Simulacrum }
	{
		assert(m_function != nullptr);
	}

	[[nodiscard]] constexpr NonNull(FunctionPointerT const functionPointer)
		: m_function{ functionPointer != nullptr ? functionPointer : Simulacrum }
	{
		assert(m_function != nullptr);
	}

	[[gnu::always_inline]] [[nodiscard]] constexpr operator FunctionPointerT() const
	{
		assert(m_function != nullptr);
		return m_function;
	}
};
