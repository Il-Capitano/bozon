#ifndef CTX_LEX_CONTEXT_H
#define CTX_LEX_CONTEXT_H

#include "error.h"

namespace ctx
{

struct global_context;

struct lex_context
{
	global_context &global_ctx;

	lex_context(global_context &_global_ctx)
		: global_ctx(_global_ctx)
	{}

	void report_error(error &&err);
};

} // namespace ctx

#endif // CTX_LEX_CONTEXT_H
