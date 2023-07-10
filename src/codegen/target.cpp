#include "target.h"
#include "llvm_latest/target_properties.h"

namespace codegen
{

static bz::vector<bz::u8string_view> split_triple(bz::u8string_view triple)
{
	auto result = bz::vector<bz::u8string_view>();

	result.reserve(4);

	auto it = triple.find('-');
	while (it != triple.end())
	{
		result.push_back(bz::u8string_view(triple.begin(), it));
		triple = bz::u8string_view(it + 1, triple.end());
		it = triple.find('-');
	}
	result.push_back(triple);

	return result;
}

static architecture_kind parse_arch(bz::u8string_view arch)
{
	if (arch == "x86_64")
	{
		return architecture_kind::x86_64;
	}
	else
	{
		return architecture_kind::unknown;
	}
}

static vendor_kind parse_vendor(bz::u8string_view vendor)
{
	if (vendor == "w64")
	{
		return vendor_kind::w64;
	}
	else if (vendor == "pc")
	{
		return vendor_kind::pc;
	}
	else
	{
		return vendor_kind::unknown;
	}
}

static os_kind parse_os(bz::u8string_view os)
{
	if (os == "windows")
	{
		return os_kind::windows;
	}
	else if (os == "linux")
	{
		return os_kind::linux;
	}
	else
	{
		return os_kind::unknown;
	}
}

static environment_kind parse_environment(bz::u8string_view env)
{
	if (env == "gnu")
	{
		return environment_kind::gnu;
	}
	else
	{
		return environment_kind::unknown;
	}
}

target_triple target_triple::parse(bz::u8string_view triple)
{
	auto result = target_triple();

	result.triple = triple;
	triple = result.triple;

	auto const components = split_triple(triple);

	switch (components.size())
	{
	default:
		// we don't care about more components
		[[fallthrough]];
	case 4:
		result.environment = parse_environment(components[2]);
		[[fallthrough]];
	case 3:
		result.os = parse_os(components[2]);
		[[fallthrough]];
	case 2:
		result.vendor = parse_vendor(components[1]);
		[[fallthrough]];
	case 1:
		result.arch = parse_arch(components[0]);
		[[fallthrough]];
	case 0:
		// nothing to parse
		break;
	}

	return result;
}

target_properties target_triple::get_target_properties(void) const
{
	auto result = target_properties();

	switch (this->arch)
	{
	case architecture_kind::x86_64:
		result.pointer_size = 8;
		result.endianness = comptime::memory::endianness_kind::little;
		break;
	default:
		// fall back to LLVM
		result = llvm_latest::get_target_properties(this->triple);
		break;
	}

	return result;
}

} // namespace codegen
