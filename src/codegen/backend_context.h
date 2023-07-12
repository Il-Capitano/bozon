#ifndef CODEGEN_BACKEND_CONTEXT_H
#define CODEGEN_BACKEND_CONTEXT_H

#include "core.h"

#include "ctx/context_forward.h"

namespace codegen
{

struct backend_context
{
	virtual ~backend_context(void) = default;

	[[nodiscard]] virtual bool generate_and_output_code(
		ctx::global_context &global_ctx,
		bz::optional<bz::u8string_view> output_path
	) = 0;
};

std::unique_ptr<backend_context> create_backend_context(ctx::global_context &global_ctx);

} // namespace codegen

#endif // CODEGEN_BACKEND_CONTEXT_H
