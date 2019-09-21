#include "ast_type.h"


static ast_typespec_ptr parse_ast_typespec_internal(
	src_tokens::pos &stream,
	src_tokens::pos  end
)
{
	assert(stream != end);

	switch (stream->kind)
	{
	case token::paren_open:
	{
		++stream; // '('
		auto t = parse_ast_typespec_internal(stream, end);
		if (stream == end)
		{
			assert(false);
		}
		assert_token(stream, token::paren_close);
		return std::move(t);
	}

	case token::ampersand:
		++stream; // '&'
		return make_ast_typespec<ast_typespec::reference>(parse_ast_typespec_internal(stream, end));

	case token::star:
		++stream; // '*'
		return make_ast_typespec<ast_typespec::pointer>(parse_ast_typespec_internal(stream, end));

	case token::kw_const:
		++stream; // 'const'
		return make_ast_typespec<ast_typespec::constant>(parse_ast_typespec_internal(stream, end));

	case token::identifier:
	{
		auto id = stream->value;
		++stream;
		return make_ast_typespec<ast_typespec::name>(id);
	}

	default:
		// TODO: auto type
		assert(false);
		return nullptr;
	}
}

ast_typespec_ptr parse_ast_typespec(
	src_tokens::pos &stream,
	src_tokens::pos  end
)
{
	return parse_ast_typespec_internal(stream, end);
}

ast_typespec_ptr parse_ast_typespec(token_range type)
{
	auto stream = type.begin;
	auto end    = type.end;

	auto rv = parse_ast_typespec_internal(stream, end);
	if (stream != end)
	{
		bad_token(stream);
	}

	return std::move(rv);
}
