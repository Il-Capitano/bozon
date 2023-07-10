#include "target_properties.h"
#include <llvm/TargetParser/Triple.h>

namespace codegen::llvm_latest
{

target_properties get_target_properties(bz::u8string_view triple_str)
{
	auto const triple_str_ref = llvm::StringRef(triple_str.data(), triple_str.size());
	auto const triple = llvm::Triple(llvm::Triple::normalize(triple_str_ref));

	auto result = target_properties();

	if (triple.getArch() != llvm::Triple::UnknownArch)
	{
		// pointer size
		if (triple.isArch64Bit())
		{
			result.pointer_size = 8;
		}
		else if (triple.isArch32Bit())
		{
			result.pointer_size = 4;
		}
		else if (triple.isArch16Bit())
		{
			result.pointer_size = 2;
		}
		else
		{
			result.pointer_size = 0;
		}

		// endianness
		if (triple.isLittleEndian())
		{
			result.endianness = comptime::memory::endianness_kind::little;
		}
		else
		{
			result.endianness = comptime::memory::endianness_kind::big;
		}
	}

	return result;
}

bz::u8string get_normalized_target(bz::u8string_view triple)
{
	auto const triple_str_ref = llvm::StringRef(triple.data(), triple.size());
	auto const normalized_triple = llvm::Triple::normalize(triple_str_ref);
	return bz::u8string_view(normalized_triple.data(), normalized_triple.data() + normalized_triple.size());
}

} // namespace codegen::llvm_latest
