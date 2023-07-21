#include "backend_context.h"

namespace codegen::c
{

backend_context::backend_context(ctx::global_context &_global_ctx)
	: global_ctx(_global_ctx)
{}

[[nodiscard]] bool backend_context::generate_and_output_code(
	ctx::global_context &global_ctx,
	bz::optional<bz::u8string_view> output_path
)
{
	bz_unreachable;
	return false;
}

} // namespace codegen::c
