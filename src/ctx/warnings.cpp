#include "warnings.h"

namespace ctx
{

static_assert(
	warning_infos.is_all([](auto const &info) { return !info.name.starts_with("no-"); }),
	"a warning name starts with 'no-'"
);

static_assert([]() {
	for (size_t i = 0; i < warning_infos.size(); ++i)
	{
		if (static_cast<size_t>(warning_infos[i].kind) != i)
		{
			return false;
		}
	}
	return true;
}(), "warning_infos is not sorted");


bz::u8string_view get_warning_name(warning_kind kind)
{
	return warning_infos[static_cast<size_t>(kind)].name;
}

warning_kind get_warning_kind(bz::u8string_view name)
{
	for (auto &info : warning_infos)
	{
		if (info.name == name)
		{
			return info.kind;
		}
	}
	return warning_kind::_last;
}

} // namespace ctx
