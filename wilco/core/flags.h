#pragma once

namespace flags
{

template <typename FlagType>
struct Flags
{
	Flags()
	{ }

	Flags(FlagType value)
		: mask(1 << (uint32_t)value)
	{ }

	constexpr bool has(FlagType value)
	{
		return (mask & (1 << (uint32_t)value)) != 0;
	}

	constexpr Flags<FlagType> operator|(FlagType value)
	{
		return Flags<FlagType> {mask | (1 << (uint32_t)value)};
	}

private:
	Flags(uint32_t mask)
		: mask(mask)
	{ }

	const uint32_t mask = 0;
};

template <typename FlagType>
Flags<FlagType> operator|(FlagType a, FlagType b)
{
	return Flags(a) | b;
}

} // namespace flags