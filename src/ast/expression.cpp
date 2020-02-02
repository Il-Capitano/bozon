#include "expression.h"
#include "../context.h"

namespace ast
{

static bytecode::type_kind get_type_kind(ast::typespec const &ts)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_base_type>:
	{
		auto &base = ts.get<ast::ts_base_type_ptr>()->base_type;
		switch (base->kind())
		{
		case ast::type::index_of<ast::built_in_type>:
		{
			auto &built_in = base->get<ast::built_in_type>();
			switch (built_in.kind)
			{
			case ast::built_in_type::int8_:
				return bytecode::type_kind::int8;
			case ast::built_in_type::int16_:
				return bytecode::type_kind::int16;
			case ast::built_in_type::int32_:
				return bytecode::type_kind::int32;
			case ast::built_in_type::int64_:
				return bytecode::type_kind::int64;
			case ast::built_in_type::uint8_:
				return bytecode::type_kind::uint8;
			case ast::built_in_type::uint16_:
				return bytecode::type_kind::uint16;
			case ast::built_in_type::uint32_:
				return bytecode::type_kind::uint32;
			case ast::built_in_type::uint64_:
				return bytecode::type_kind::uint64;
			case ast::built_in_type::float32_:
				return bytecode::type_kind::float32;
			case ast::built_in_type::float64_:
				return bytecode::type_kind::float64;
			case ast::built_in_type::char_:
				return bytecode::type_kind::uint32;
			case ast::built_in_type::bool_:
				return bytecode::type_kind::uint8;
			case ast::built_in_type::null_t_:
				return bytecode::type_kind::ptr;
			case ast::built_in_type::str_:
			case ast::built_in_type::void_:
			default:
				assert(false);
				return bytecode::type_kind::int32;
			}
		}
		case ast::type::index_of<ast::aggregate_type>:
		default:
			assert(false);
			return bytecode::type_kind::int32;
		}
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


src_file::token_pos expr_unary_op::get_tokens_begin(void) const
{ return this->op; }

src_file::token_pos expr_unary_op::get_tokens_pivot(void) const
{ return this->op; }

src_file::token_pos expr_unary_op::get_tokens_end(void) const
{ return this->expr.get_tokens_end(); }


src_file::token_pos expr_binary_op::get_tokens_begin(void) const
{ return this->lhs.get_tokens_begin(); }

src_file::token_pos expr_binary_op::get_tokens_pivot(void) const
{ return this->op; }

src_file::token_pos expr_binary_op::get_tokens_end(void) const
{ return this->rhs.get_tokens_end(); }


src_file::token_pos expr_function_call::get_tokens_begin(void) const
{ return this->called.get_tokens_begin(); }

src_file::token_pos expr_function_call::get_tokens_pivot(void) const
{ return this->op; }

src_file::token_pos expr_function_call::get_tokens_end(void) const
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


void expr_identifier::resolve(void)
{}

void expr_identifier::emit_bytecode(
	bz::vector<bytecode::instruction> &out,
	bz::optional<bytecode::value_pos_t> ret_pos
)
{
	if (!ret_pos.has_value())
	{
		return;
	}

	using namespace bytecode;
	auto offset = context.get_identifier_stack_offset(this->identifier);
	auto type = context.get_identifier_type(this->identifier);

	if (type.kind() == ast::typespec::index<ast::ts_reference>)
	{
		// TODO
		assert(false, "reference types are not yet implemented");
	}
	else
	{
		auto type_kind = get_type_kind(type);
		out.push_back(mov{
			*ret_pos,
			ptr_value(offset),
			type_kind
		});
	}
}

void expr_literal::resolve(void)
{}

void expr_literal::emit_bytecode(
	bz::vector<bytecode::instruction> &out,
	bz::optional<bytecode::value_pos_t> ret_pos
)
{
	using namespace bytecode;

	if (!ret_pos.has_value())
	{
		return;
	}

	switch (this->value.index())
	{
	case integer_number:
	{
		auto val = this->value.get<integer_number>();
		if (val <= std::numeric_limits<int32_t>::max())
		{
			auto int32_val = static_cast<int32_t>(val);
			out.push_back(mov{
				*ret_pos,
				value_pos_t(register_value{._int32 = int32_val}),
				type_kind::int32
			});
		}
		else
		{
			assert(false);
		}
		break;
	}
	case floating_point_number:
	{
		auto val = this->value.get<floating_point_number>();
		out.push_back(mov{
			*ret_pos,
			value_pos_t(register_value{._float64 = val}),
			type_kind::float64
		});
		break;
	}
	// TODO
	case string:
		assert(false);
		break;
	case character:
		assert(false);
		break;
	case bool_true:
		out.push_back(mov{
			*ret_pos,
			value_pos_t(register_value{._uint8 = 1}),
			type_kind::uint8
		});
		break;
	case bool_false:
		out.push_back(mov{
			*ret_pos,
			value_pos_t(register_value{._uint8 = 0}),
			type_kind::uint8
		});
		break;
	case null:
	{
		out.push_back(mov{
			*ret_pos,
			value_pos_t(register_value{._ptr = nullptr}),
			type_kind::ptr
		});
		break;
	}
	}
}

void expr_tuple::resolve(void)
{
	for (auto &elem : this->elems)
	{
		elem.resolve();
	}
}

void expr_tuple::emit_bytecode(
	bz::vector<bytecode::instruction> & /* out */,
	bz::optional<bytecode::value_pos_t> /* ret_pos */
)
{
	// TODO
	assert(false);
}

void expr_unary_op::resolve(void)
{
	this->expr.resolve();
}

void expr_unary_op::emit_bytecode(
	bz::vector<bytecode::instruction> & /* out */,
	bz::optional<bytecode::value_pos_t> /* ret_pos */
)
{
	// TODO
	assert(false);
}

void expr_binary_op::resolve(void)
{
	this->lhs.resolve();
	this->rhs.resolve();
}

void expr_binary_op::emit_bytecode(
	bz::vector<bytecode::instruction> &out,
	bz::optional<bytecode::value_pos_t> ret_pos
)
{
	// TODO
	assert(false);
}

void expr_function_call::resolve(void)
{
	if (
		this->called.kind() != expression::index<expr_identifier>
		|| context.is_variable(this->called.get<expr_identifier_ptr>()->identifier->value)
	)
	{
		this->called.resolve();
	}
	for (auto &p : this->params)
	{
		p.resolve();
	}
}

void expr_function_call::emit_bytecode(
	bz::vector<bytecode::instruction> & /* out */,
	bz::optional<bytecode::value_pos_t> /* ret_pos */
)
{
	// TODO
	assert(false);
}



expr_literal::expr_literal(src_file::token_pos stream)
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

static typespec get_literal_type(expr_literal_ptr const &literal)
{
	switch (literal->value.index())
	{
	case expr_literal::integer_number:
		return make_ts_base_type(int32_);
	case expr_literal::floating_point_number:
		return make_ts_base_type(float64_);
	case expr_literal::string:
		return make_ts_base_type(str_);
	case expr_literal::character:
		return make_ts_base_type(char_);
	case expr_literal::bool_true:
		return make_ts_base_type(bool_);
	case expr_literal::bool_false:
		return make_ts_base_type(bool_);
	case expr_literal::null:
		return make_ts_base_type(null_t_);
	default:
		assert(false);
		return typespec();
	}
}

static typespec get_tuple_type(expr_tuple_ptr const &tuple)
{
	bz::vector<typespec> types = {};

	for (auto &expr : tuple->elems)
	{
		assert(expr.expr_type.kind() != typespec::null);
		types.push_back(expr.expr_type);
	}

	return make_ts_tuple(std::move(types));
}

void expression::resolve(void)
{
	if (this->kind() == index<expr_unresolved>)
	{
		auto &unresolved_expr = this->get<expr_unresolved_ptr>();

		auto begin = unresolved_expr->expr.begin;
		auto end   = unresolved_expr->expr.end;

		auto expr = parse_expression(begin, end);
		expr.resolve();
		this->assign(std::move(expr));
	}

	switch (this->kind())
	{
	case index<expr_identifier>:
	{
		auto &id = this->get<expr_identifier_ptr>();
		id->resolve();
		this->is_lvalue = true;
		this->expr_type = remove_lvalue_reference(context.get_identifier_type(id->identifier));
		break;
	}

	case index<expr_literal>:
	{
		auto &literal = this->get<expr_literal_ptr>();
		literal->resolve();
		this->is_lvalue = false;
		this->expr_type = get_literal_type(literal);
		break;
	}

	case index<expr_tuple>:
	{
		auto &tuple = this->get<expr_tuple_ptr>();
		tuple->resolve();
		this->is_lvalue = false;
		this->expr_type = get_tuple_type(tuple);
		break;
	}

	case index<expr_unary_op>:
	{
		auto &unary_op = this->get<expr_unary_op_ptr>();
		unary_op->resolve();
		auto expr_t = context.get_operator_type(*unary_op);
		this->is_lvalue = expr_t.kind() == typespec::index<ts_reference>;
		this->expr_type = remove_lvalue_reference(std::move(expr_t));
		break;
	}

	case index<expr_binary_op>:
	{
		auto &binary_op = this->get<expr_binary_op_ptr>();
		binary_op->resolve();
		auto expr_t = context.get_operator_type(*binary_op);
		this->is_lvalue = expr_t.kind() == typespec::index<ts_reference>;
		this->expr_type = remove_lvalue_reference(std::move(expr_t));
		break;
	}

	case index<expr_function_call>:
	{
		auto &fn_call = this->get<expr_function_call_ptr>();
		fn_call->resolve();
		auto expr_t = context.get_function_call_type(*fn_call);
		this->is_lvalue = expr_t.kind() == typespec::index<ts_reference>;
		this->expr_type = remove_lvalue_reference(std::move(expr_t));
		break;
	}

	default:
		assert(false);
		break;
	}
}

void expression::emit_bytecode(
	bz::vector<bytecode::instruction> &out,
	bz::optional<bytecode::value_pos_t> ret_pos
)
{
	switch (this->kind())
	{
	case index<expr_identifier>:
		this->get<expr_identifier_ptr>()->emit_bytecode(out, ret_pos);
		break;
	case index<expr_literal>:
		this->get<expr_literal_ptr>()->emit_bytecode(out, ret_pos);
		break;
	case index<expr_tuple>:
	case index<expr_unary_op>:
	case index<expr_binary_op>:
	case index<expr_function_call>:
	default:
		assert(false);
		break;
	}
}

} // namespace ast
