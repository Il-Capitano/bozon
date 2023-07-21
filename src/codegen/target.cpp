#include "target.h"
#include "config.h"
#include "llvm_latest/target.h"

namespace codegen
{

static bz::u8string_view native_target_triple = BOZON_CONFIG_NATIVE_TARGET;

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

	if (triple == "" || triple == "native")
	{
		triple = native_target_triple;
	}

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
		if constexpr (config::backend_llvm)
		{
			result = llvm_latest::get_target_properties(this->triple);
		}
		break;
	}

	return result;
}

static bz::u8string_view get_arch_string(architecture_kind arch)
{
	switch (arch)
	{
	case architecture_kind::unknown:
		return "unknown";
	case architecture_kind::x86_64:
		return "x86_64";
	}
}

static bz::u8string_view get_vendor_string(vendor_kind vendor)
{
	switch (vendor)
	{
	case vendor_kind::unknown:
		return "unknown";
	case vendor_kind::w64:
		return "w64";
	case vendor_kind::pc:
		return "pc";
	}
}

static bz::u8string_view get_os_string(os_kind os)
{
	switch (os)
	{
	case os_kind::unknown:
		return "unknown";
	case os_kind::windows:
		return "windows";
	case os_kind::linux:
		return "linux";
	}
}

static bz::u8string_view get_environment_string(environment_kind environment)
{
	switch (environment)
	{
	case environment_kind::unknown:
		return "unknown";
	case environment_kind::gnu:
		return "gnu";
	}
}

bz::u8string target_triple::get_normalized_target(void) const
{
	// fall back to LLVM target normalization
	if constexpr (config::backend_llvm)
	{
		if (
			this->arch == architecture_kind::unknown
			|| this->vendor == vendor_kind::unknown
			|| this->os == os_kind::unknown
			|| this->environment == environment_kind::unknown
		)
		{
			return llvm_latest::get_normalized_target(this->triple);
		}
	}

	return bz::format(
		"{}-{}-{}-{}",
		get_arch_string(this->arch),
		get_vendor_string(this->vendor),
		get_os_string(this->os),
		get_environment_string(this->environment)
	);
}

} // namespace codegen
