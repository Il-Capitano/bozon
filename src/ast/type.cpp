#include "type.h"

namespace ast
{

typespec_ptr parse_typespec(
	src_tokens::pos &stream,
	src_tokens::pos  end
)
{
	if (stream == end)
	{
		bad_token(stream);
	}

	switch (stream->kind)
	{
	case token::identifier:
	{
		auto id = stream->value;
		++stream;
		return make_name_typespec(id);
	}

	case token::kw_const:
		++stream; // 'const'
		return make_constant_typespec(parse_typespec(stream, end));

	case token::star:
		++stream; // '*'
		return make_pointer_typespec(parse_typespec(stream, end));

	case token::ampersand:
		++stream; // '&'
		return make_reference_typespec(parse_typespec(stream, end));

	case token::kw_function:
	{
		++stream; // 'function'
		assert_token(stream, token::paren_open);

		bz::vector<typespec_ptr> param_types = {};
		if (stream->kind != token::paren_close) while (stream != end)
		{
			param_types.push_back(parse_typespec(stream, end));
			if (stream->kind != token::paren_close)
			{
				assert_token(stream, token::comma);
			}
			else
			{
				break;
			}
		}
		assert(stream != end);
		assert_token(stream, token::paren_close);
		assert_token(stream, token::arrow);

		auto ret_type = parse_typespec(stream, end);

		return make_function_typespec(ret_type, std::move(param_types));
	}

	case token::square_open:
	{
		++stream; // '['

		bz::vector<typespec_ptr> types = {};
		if (stream->kind != token::square_close) while (stream != end)
		{
			types.push_back(parse_typespec(stream, end));
			if (stream->kind != token::square_close)
			{
				assert_token(stream, token::comma);
			}
			else
			{
				break;
			}
		}
		assert(stream != end);
		assert_token(stream, token::square_close);

		return make_tuple_typespec(std::move(types));
	}

	default:
		assert(false);
	}
}

void typespec::resolve(void)
{
	switch (this->kind())
	{
	case unresolved:
	{
		auto &type = this->get<unresolved>();

		auto stream = type.typespec.begin;
		auto end    = type.typespec.end;

		if (stream == end)
		{
			this->emplace<none>();
			return;
		}

		*this = *parse_typespec(stream, end);
		return;
	}

	case name:
		return;

	case constant:
		this->get<constant>().base->resolve();
		return;

	case pointer:
		this->get<pointer>().base->resolve();
		return;

	case reference:
		this->get<reference>().base->resolve();
		return;

	case function:
	{
		auto &fn = this->get<function>();
		fn.return_type->resolve();
		for (auto &t : fn.argument_types)
		{
			t->resolve();
		}
		return;
	}

	case tuple:
	{
		auto &tp = this->get<tuple>();
		for (auto &type : tp.types)
		{
			type->resolve();
		}
		return;
	}

	case none:
		return;

	default:
		assert(false);
		return;
	}
}

} // namespace ast
