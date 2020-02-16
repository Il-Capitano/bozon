#include "expression.h"

namespace ast
{

static bytecode::type_kind get_type_kind(ast::typespec const &ts)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_base_type>:
	{
		assert(false);
		return bytecode::type_kind::int32;
	}
	case ast::typespec::index<ast::ts_constant>:
		return get_type_kind(ts.get<ast::ts_constant_ptr>()->base);
	case ast::typespec::index<ast::ts_pointer>:
		return bytecode::type_kind::ptr;
	case ast::typespec::index<ast::ts_reference>:
	case ast::typespec::index<ast::ts_function>:
	case ast::typespec::index<ast::ts_tuple>:
	default:
		assert(false);
		return bytecode::type_kind::int32;
	}
}


token_pos expr_unary_op::get_tokens_begin(void) const
{ return this->op; }

token_pos expr_unary_op::get_tokens_pivot(void) const
{ return this->op; }

token_pos expr_unary_op::get_tokens_end(void) const
{ return this->expr.get_tokens_end(); }


token_pos expr_binary_op::get_tokens_begin(void) const
{ return this->lhs.get_tokens_begin(); }

token_pos expr_binary_op::get_tokens_pivot(void) const
{ return this->op; }

token_pos expr_binary_op::get_tokens_end(void) const
{ return this->rhs.get_tokens_end(); }


token_pos expr_function_call::get_tokens_begin(void) const
{ return this->called.get_tokens_begin(); }

token_pos expr_function_call::get_tokens_pivot(void) const
{ return this->op; }

token_pos expr_function_call::get_tokens_end(void) const
{
	if (this->params.size() == 0)
	{
		return this->called.get_tokens_end() + 2;
	}
	else
	{
		return this->params.back().get_tokens_end() + 1;
	}
}



expr_literal::expr_literal(token_pos stream)
	: src_pos(stream)
{
	switch(stream->kind)
	{
	case token::number_literal:
	{
		bz::string value = stream->value;
		value.erase('\'');

		double num = 0;
		size_t i;
		for (i = 0; i < value.length() && value[i] != '.'; ++i)
		{
			assert(value[i] >= '0' && value[i] <= '9');
			num *= 10;
			num += value[i] - '0';
		}

		if (i == value.length())
		{
			this->value.emplace<integer_number>(static_cast<uint64_t>(num));
		}
		else
		{
			assert(value[i] == '.');
			++i;

			double level = 1;
			while (i < value.length())
			{
				assert(value[i] >= '0' && value[i] <= '9');
				level *= 0.1;
				num += level * (value[i] - '0');
				++i;
			}

			this->value.emplace<floating_point_number>(num);
		}
		break;
	}

	case token::string_literal:
	{
		auto &str = stream->value;
		bz::string res = "";
		res.reserve(str.length());
		for (auto it = str.begin(), end = str.end(); it != end; ++it)
		{
			switch (*it)
			{
			case '\\':
				++it;
				assert(it != end);
				switch (*it)
				{
				case '\\':
					res += '\\';
					break;
				case '\'':
					res += '\'';
					break;
				case '\"':
					res += '\"';
					break;
				case 'n':
					res += '\n';
					break;
				case 't':
					res += '\t';
					break;
				default:
					assert(false);
					break;
				}
				break;

			default:
				res += *it;
				break;
			}
		}
		this->value.emplace<string>(std::move(res));
		break;
	}

	case token::character_literal:
		if (stream->value.length() == 1)
		{
			this->value.emplace<character>(stream->value[0]);
		}
		else
		{
			assert(stream->value.length() == 2);
			assert(stream->value[0] == '\\');
			switch (stream->value[1])
			{
			case '\\':
				this->value.emplace<character>('\\');
				break;
			case '\'':
				this->value.emplace<character>('\'');
				break;
			case '\"':
				this->value.emplace<character>('\"');
				break;
			case 'n':
				this->value.emplace<character>('\n');
				break;
			case 't':
				this->value.emplace<character>('\t');
				break;
			default:
				assert(false);
				break;
			}
		}
		break;

	case token::kw_true:
		this->value.emplace<bool_true>();
		break;

	case token::kw_false:
		this->value.emplace<bool_false>();
		break;

	case token::kw_null:
		this->value.emplace<null>();
		break;

	default:
		assert(false);
		break;
	}
}

} // namespace ast
