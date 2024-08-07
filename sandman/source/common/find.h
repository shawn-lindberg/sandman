#pragma once

#include <iterator>

namespace Common
{

	// `std::find` isn't `constexpr` in C++ 17. This is a `constexpr` alternative.
	template <class IteratorT, class T = typename std::iterator_traits<IteratorT>::value_type>
	constexpr IteratorT Find(IteratorT first, IteratorT const last, T const& value)
	{
		for (; first != last; ++first)
		{
			if (*first == value)
			{
				return first;
			}
		}

		return last;
	}

	// `std::find_if` isn't `constexpr` in C++ 17. This is a `constexpr` alternative.
	template <class IteratorT, class UnaryPredicateT>
	constexpr IteratorT FindIf(IteratorT first, IteratorT const last,
										UnaryPredicateT const predicate)
	{
		for (; first != last; ++first)
		{
			if (predicate(*first))
			{
				return first;
			}
		}

		return last;
	}

	// `std::find_if_not` isn't `constexpr` in C++ 17. This is a `constexpr` alternative.
	template <class IteratorT, class UnaryPredicateT>
	constexpr IteratorT FindIfNot(IteratorT first, IteratorT const last,
											UnaryPredicateT const predicate)
	{
		for (; first != last; ++first)
		{
			if (!predicate(*first))
			{
				return first;
			}
		}

		return last;
	}

} // namespace Common
