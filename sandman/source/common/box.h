#pragma once

#include <utility>

namespace Common
{
	/// Sometimes, when a function takes two or more parameters of the same type,
	/// often with primitive types, it can be easy to erroneously swap them.
	/// In that case, it may be good to designate different objects
	/// with explicit constructors for each parameter such that the object works
	/// as a "named parameter" for a boxed value in order to differentiate
	/// different inputs to a function in a way that is enforced at compile time.
	///
	/// This utility type exists so that declaring "named parameter" types
	/// is less verbose; a `struct` or `class` can be declared
	/// publicly inheriting from this type and also inheriting the constructors.
	template <typename BoxedValueT>
	struct Box
	{
		BoxedValueT m_Value;

		[[nodiscard]] explicit constexpr Box(BoxedValueT const& value) : m_Value(value) {}

		[[nodiscard]] explicit constexpr Box(BoxedValueT&& value) : m_Value(std::move(value)) {}
	};
}
