#include "lex_context.h"
#include "global_context.h"

namespace ctx
{

void lex_context::report_error(error &&err)
{
	this->global_ctx.report_error(std::move(err));
}

} // namespace ctx
