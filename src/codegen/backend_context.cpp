#include "backend_context.h"
#include "llvm_latest/backend_context.h"
#include "global_data.h"
#include "ctx/global_context.h"

namespace codegen
{

static codegen::llvm_latest::output_code_kind output_code_kind_from_emit_type(emit_type type)
{
		switch (emit_file_type)
		{
		case emit_type::obj:
			return codegen::llvm_latest::output_code_kind::obj;
		case emit_type::asm_:
			return codegen::llvm_latest::output_code_kind::asm_;
		case emit_type::llvm_bc:
			return codegen::llvm_latest::output_code_kind::llvm_bc;
		case emit_type::llvm_ir:
			return codegen::llvm_latest::output_code_kind::llvm_ir;
		case emit_type::null:
			bz_unreachable;
		}
}

std::unique_ptr<backend_context> create_backend_context(ctx::global_context &global_ctx)
{
	switch (emit_file_type)
	{
	case emit_type::obj:
	case emit_type::asm_:
	case emit_type::llvm_bc:
	case emit_type::llvm_ir:
	{
		bool error = false;
		auto result = std::make_unique<codegen::llvm_latest::backend_context>(
			global_ctx,
			global_ctx.target_triple.triple,
			output_code_kind_from_emit_type(emit_file_type),
			error
		);

		if (error)
		{
			result.reset();
		}

		return result;
	}
	case emit_type::null:
		return nullptr;
	}
}

} // namespace codegen
