#include "global_data.h"

bool is_warning_enabled(ctx::warning_kind kind) noexcept
{
	bz_assert(kind != ctx::warning_kind::_last);
	return warnings[static_cast<size_t>(kind)];
}

bool is_optimization_enabled(bc::optimization_kind kind) noexcept
{
	bz_assert(kind != bc::optimization_kind::_last);
	return optimizations[static_cast<size_t>(kind)];
}

bool is_any_optimization_enabled(void) noexcept
{
	for (auto const opt : optimizations)
	{
		if (opt)
		{
			return true;
		}
	}
	return false;
}
