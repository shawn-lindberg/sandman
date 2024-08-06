#pragma once

namespace Common { template <typename> class NonNull; }

template <typename ReturnT, typename... ParamsT>
class Common::NonNull<ReturnT (*)(ParamsT...)>
{
public:
	using FunctionT = ReturnT (*)(ParamsT...);

	[[gnu::always_inline]] static constexpr ReturnT Simulacrum(ParamsT...)
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
