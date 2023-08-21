#include "lex/lexer.h"
#include "ctx/lex_context.h"
#include "ctx/global_context.h"

extern "C" int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
	static ctx::global_context global_ctx;

	auto const input = bz::u8string_view(data, data + size);
	if (input.verify())
	{
		ctx::lex_context lex_ctx(global_ctx);
		lex::get_tokens(input, 0, lex_ctx);
	}
	global_ctx.clear_errors_and_warnings();
	return 0;
}

#include "lex/lexer.cpp"
#include "ctx/global_context.cpp"
#include "ctx/lex_context.cpp"
#include "ast/statement.cpp"
#include "ast/expression.cpp"
#include "ast/typespec.cpp"
#include "ast/allocator.cpp"
#include "global_data.cpp"
