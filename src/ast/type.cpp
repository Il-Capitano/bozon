#include "type.h"
#include "../context.h"

namespace ast
{

typespec parse_typespec(
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
		auto id = stream;
		++stream;
		return make_ts_base_type(context.get_type(id));
	}

	case token::kw_const:
		++stream; // 'const'
		return make_ts_constant(parse_typespec(stream, end));

	case token::star:
		++stream; // '*'
		return make_ts_pointer(parse_typespec(stream, end));

	case token::ampersand:
		++stream; // '&'
		return make_ts_reference(parse_typespec(stream, end));

	case token::kw_function:
	{
		++stream; // 'function'
		assert_token(stream, token::paren_open);

		bz::vector<typespec> param_types = {};
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

		return make_ts_function(std::move(ret_type), std::move(param_types));
	}

	case token::square_open:
	{
		++stream; // '['

		bz::vector<typespec> types = {};
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

		return make_ts_tuple(std::move(types));
	}

	default:
		assert(false);
		return typespec();
	}
}

template<>
void typespec::resolve(void)
{
	switch (this->kind())
	{
	case index<ts_unresolved>:
	{
		auto &type = this->get<ts_unresolved_ptr>();
		*this = parse_typespec(type->tokens.begin, type->tokens.end);
		return;
	}

	case index<ts_base_type>:
		return;

	case index<ts_constant>:
		this->get<ts_constant_ptr>()->base.resolve();
		return;

	case index<ts_pointer>:
		this->get<ts_pointer_ptr>()->base.resolve();
		return;

	case index<ts_reference>:
		this->get<ts_reference_ptr>()->base.resolve();
		return;

	case index<ts_function>:
	{
		auto &fn_t = this->get<ts_function_ptr>();
		fn_t->return_type.resolve();
		for (auto &t : fn_t->argument_types)
		{
			t.resolve();
		}
		return;
	}

	case index<ts_tuple>:
	{
		auto &tp_t = this->get<ts_tuple_ptr>();
		for (auto &t : tp_t->types)
		{
			t.resolve();
		}
		return;
	}

	default:
		assert(false);
		return;
	}
}

} // namespace ast
