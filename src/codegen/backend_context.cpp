#include "backend_context.h"
#include "global_data.h"
#include "ctx/global_context.h"
#include "config.h"
#include "llvm_latest/backend_context.h"
#include "c/backend_context.h"

namespace codegen
{

[[maybe_unused]] static llvm_latest::output_code_kind output_code_kind_from_emit_type(emit_type type)
{
	switch (type)
	{
	case emit_type::obj:
		return llvm_latest::output_code_kind::obj;
	case emit_type::asm_:
		return llvm_latest::output_code_kind::asm_;
	case emit_type::llvm_bc:
		return llvm_latest::output_code_kind::llvm_bc;
	case emit_type::llvm_ir:
		return llvm_latest::output_code_kind::llvm_ir;
	default:
		bz_unreachable;
	}
};

std::unique_ptr<backend_context> create_backend_context(ctx::global_context &global_ctx)
{
	switch (global_data::emit_file_type)
	{
	case emit_type::obj:
	case emit_type::asm_:
	case emit_type::llvm_bc:
	case emit_type::llvm_ir:
	{
		if constexpr (config::backend_llvm)
		{
			bool error = false;
			auto result = std::make_unique<codegen::llvm_latest::backend_context>(
				global_ctx,
				global_ctx.target_triple.triple,
				output_code_kind_from_emit_type(global_data::emit_file_type),
				error
			);

			if (error)
			{
				result.reset();
			}

			return result;
		}
		else
		{
			return nullptr;
		}
	}
	case emit_type::c:
	{
		if constexpr (config::backend_c)
		{
			return std::make_unique<codegen::c::backend_context>(global_ctx);
		}
		else
		{
			return nullptr;
		}
	}
	case emit_type::null:
		return nullptr;
	}
}

} // namespace codegen
