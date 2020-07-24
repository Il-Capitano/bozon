#include "global_data.h"

void enable_warning(ctx::warning_kind kind)
{
	warnings[static_cast<size_t>(kind)] = true;
}

void disable_warning(ctx::warning_kind kind)
{
	warnings[static_cast<size_t>(kind)] = false;
}

void enable_Wall(void)
{
	for (auto &warning : warnings)
	{
		warning = true;
	}
}

bool is_warning_enabled(ctx::warning_kind kind)
{
	bz_assert(kind != ctx::warning_kind::_last);
	return warnings[static_cast<size_t>(kind)];
}
