#ifndef CODEGEN_LLVM_LATEST_TARGET_H
#define CODEGEN_LLVM_LATEST_TARGET_H

#include "core.h"
#include "codegen/target.h"

namespace codegen::llvm_latest
{

target_properties get_target_properties(bz::u8string_view triple);
bz::u8string get_normalized_target(bz::u8string_view triple);

} // namespace codegen::llvm_latest

#endif // CODEGEN_LLVM_LATEST_TARGET_H
