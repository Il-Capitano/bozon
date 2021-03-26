#include "lex/lexer.h"
#include "ctx/lex_context.h"
#include "ctx/global_context.h"
#include "ctx/src_manager.h"

extern "C" int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
	static ctx::src_manager manager{};

	auto const input = bz::u8string_view(data, data + size);
	if (input.verify())
	{
		ctx::lex_context lex_ctx{manager._global_ctx};
		lex::get_tokens(input, 0, lex_ctx);
	}
	manager._global_ctx.clear_errors_and_warnings();
	return 0;
}

#include "lex/lexer.cpp"
