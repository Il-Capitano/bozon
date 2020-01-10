#include "expression.h"
#include "../context.h"

namespace ast
{

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

void expr_literal::resolve(void)
{}

void expr_tuple::resolve(void)
{
	for (auto &elem : this->elems)
	{
		elem.resolve();
	}
}

void expr_unary_op::resolve(void)
{
	this->expr.resolve();
}

void expr_binary_op::resolve(void)
{
	this->lhs.resolve();
	this->rhs.resolve();
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
		this->value.emplace<string>(stream->value);
		break;

	case token::character_literal:
		assert(stream->value.length() == 1);
		this->value.emplace<character>(stream->value[0]);
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

} // namespace ast
