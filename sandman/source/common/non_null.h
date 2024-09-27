#pragma once

namespace Common {
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
	using FunctionT = ReturnT (*)(ParametersT...);

	/// Does nothing with the parameters, and returns a default constructed value.
	[[gnu::always_inline]] static constexpr ReturnT Simulacrum(ParametersT...)
	{
		return ReturnT{};
	}

private:
	FunctionT m_function;

public:
	[[nodiscard]] constexpr NonNull() : m_function{ Simulacrum } {}

	[[nodiscard]] constexpr NonNull(FunctionT const functionPointer)
		: m_function{ functionPointer != nullptr ? functionPointer : Simulacrum }
	{}

	[[gnu::always_inline]] [[nodiscard]] constexpr operator FunctionT() const
	{
		return m_function;
	}
};
