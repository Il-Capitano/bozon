#ifndef CODEGEN_TARGET_H
#define CODEGEN_TARGET_H

#include "core.h"
#include "comptime/memory_common.h"

namespace codegen
{

struct target_properties
{
	bz::optional<uint32_t> pointer_size;
	bz::optional<comptime::memory::endianness_kind> endianness;
};

enum class architecture_kind
{
	unknown,

	x86_64,
};

enum class vendor_kind
{
	unknown,

	w64,
	pc,
};

enum class os_kind
{
	unknown,

	windows,
	linux,
};

enum class environment_kind
{
	unknown,

	gnu,
};

struct target_triple
{
	bz::u8string triple;
	architecture_kind arch = architecture_kind::unknown;
	vendor_kind       vendor = vendor_kind::unknown;
	os_kind           os = os_kind::unknown;
	environment_kind  environment = environment_kind::unknown;

	static target_triple parse(bz::u8string_view triple);

	target_properties get_target_properties(void) const;
};

} // namespace codegen

#endif // CODEGEN_TARGET_H
