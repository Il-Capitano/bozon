#include "ast_type.h"



ast_typespec_ptr ast_typespec::clone(void) const
{
	switch(this->kind)
	{
	case type::constant:
	case type::reference:
	case type::pointer:
		return make_ast_typespec(this->kind, this->base->clone());
	case type::name:
		return make_ast_typespec(this->name);
	default:
		assert(false);
		return nullptr;
	}
}



static ast_typespec_ptr parse_ast_typespec_internal(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end
)
{
	if (stream == end)
	{
		// TODO: auto type
		assert(false);
		return nullptr;
	}

	switch (stream->kind)
	{
	case token::paren_open:
	{
		++stream;
		auto t = parse_ast_typespec_internal(stream, end);
		if (stream == end)
		{
			assert(false);
		}
		assert_token(*stream, token::paren_close);
		++stream;
		return std::move(t);
	}

	case token::ampersand:
		++stream;
		return make_ast_typespec(type::reference, parse_ast_typespec_internal(stream, end));

	case token::star:
		++stream;
		return make_ast_typespec(type::pointer, parse_ast_typespec_internal(stream, end));

	case token::kw_const:
		++stream;
		return make_ast_typespec(type::constant, parse_ast_typespec_internal(stream, end));

	case token::identifier:
	{
		auto id = stream->value;
		++stream;
		return make_ast_typespec(id);
	}

	default:
		// TODO: auto type
		assert(false);
		return nullptr;
	}
}

ast_typespec_ptr parse_ast_typespec(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end
)
{
	return parse_ast_typespec_internal(stream, end);
}

ast_typespec_ptr parse_ast_typespec(std::vector<token> const &t)
{
	auto begin = t.cbegin();
	auto end   = t.cend();

	auto rv = parse_ast_typespec_internal(begin, end);
	if (begin != end)
	{
		bad_token(*begin);
	}

	return std::move(rv);
}
