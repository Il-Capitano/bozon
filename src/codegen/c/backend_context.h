#ifndef CODEGEN_C_BACKEND_CONTEXT_H
#define CODEGEN_C_BACKEND_CONTEXT_H

#include "core.h"
#include "config.h"
#include "codegen/backend_context.h"

namespace codegen::c
{

#ifdef BOZON_CONFIG_BACKEND_C

struct backend_context : virtual ::codegen::backend_context
{
	bz::u8string code_string;

	backend_context(void);

	[[nodiscard]] virtual bool generate_and_output_code(
		ctx::global_context &global_ctx,
		bz::optional<bz::u8string_view> output_path
	) override;

	bool generate_code(ctx::global_context &global_ctx);
	bool emit_file(ctx::global_context &global_ctx, bz::u8string_view output_path);
};

#else

struct backend_context : virtual ::codegen::backend_context
{
	backend_context(ctx::global_context &global_ctx)
	{}

	[[nodiscard]] virtual bool generate_and_output_code(
		ctx::global_context &global_ctx,
		bz::optional<bz::u8string_view> output_path
	) override
	{
		return false;
	}
};

#endif // BOZON_CONFIG_BACKEND_C

} // namespace codegen::c

#endif // CODEGEN_LLVM_LATEST_LLVM_CONTEXT_H
