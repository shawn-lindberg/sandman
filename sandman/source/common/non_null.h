#pragma once

/// Wrapper around a pointer that is not null.
namespace Common { template <typename> class NonNull; }

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
		return ReturnT();
	}

private:
	FunctionT m_Function;

public:
	[[nodiscard]] constexpr NonNull() : m_Function{ Simulacrum } {}

	[[nodiscard]] constexpr NonNull(FunctionT const functionPointer)
		: m_Function{ functionPointer != nullptr ? functionPointer : Simulacrum }
	{}

	[[gnu::always_inline]] [[nodiscard]] constexpr operator FunctionT() const
	{
		return m_Function;
	}
};
