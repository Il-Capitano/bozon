#include "match_to_type.h"
#include "match_common.h"
#include "statement_resolver.h"
#include "type_match_generic.h"

namespace resolve
{

// returns -2 if lhs < rhs
// returns  2 if lhs > rhs
// returns -1 if lhs < rhs only by implicit literal conversions
// returns  1 if lhs > rhs only by implicit literal conversions
// returns  0 otherwise
int match_level_compare(match_level_t const &lhs, match_level_t const &rhs)
{
	if (lhs.is_null() && rhs.is_null())
	{
		return 0;
	}
	else if (lhs.is_null())
	{
		return -2;
	}
	else if (rhs.is_null())
	{
		return 2;
	}

	if (lhs.index() != rhs.index())
	{
		return 0;
	}
	else if (lhs.is_single())
	{
		auto const [lhs_level, lhs_ref_kind, lhs_type_kind] = lhs.get_single();
		auto const [rhs_level, rhs_ref_kind, rhs_type_kind] = rhs.get_single();

		if (lhs_type_kind != rhs_type_kind)
		{
			if (lhs_type_kind == type_match_kind::exact_match && rhs_type_kind == type_match_kind::implicit_literal_conversion)
			{
				return 1;
			}
			else if (lhs_type_kind == type_match_kind::implicit_literal_conversion && rhs_type_kind == type_match_kind::exact_match)
			{
				return -1;
			}
			else
			{
				return lhs_type_kind > rhs_type_kind ? -2 : 2;
			}
		}

		if (lhs_level != rhs_level)
		{
			return lhs_level < rhs_level ? -2 : 2;
		}

		if (lhs_ref_kind != rhs_ref_kind)
		{
			return lhs_ref_kind > rhs_ref_kind ? -2 : 2;
		}

		return 0;
	}
	else
	{
		auto const &lhs_vec = lhs.get_multi();
		auto const &rhs_vec = rhs.get_multi();
		bz_assert(lhs_vec.size() == rhs_vec.size());
		bool is_ambiguous_by_implicit_literal_conversions = false;
		int current_result = 0;
		for (auto const &[lhs_val, rhs_val] : bz::zip(lhs_vec, rhs_vec))
		{
			auto const cmp_res = match_level_compare(lhs_val, rhs_val);
			if (cmp_res == 0 || (std::abs(current_result) == 2 && std::abs(cmp_res) == 1))
			{
				// nothing
			}
			else if (current_result == 0 || (std::abs(current_result) == 1 && std::abs(cmp_res) == 2))
			{
				current_result = cmp_res;
			}
			else if (current_result != cmp_res && std::abs(current_result) == 1 && std::abs(cmp_res) == 1)
			{
				is_ambiguous_by_implicit_literal_conversions = true;
				current_result = 0;
			}
			else if (current_result != cmp_res)
			{
				// ambiguous
				return 0;
			}
		}

		if (is_ambiguous_by_implicit_literal_conversions && std::abs(current_result) == 1)
		{
			return 0;
		}
		else
		{
			return current_result;
		}
	}
	bz_unreachable;
	return 1;
}

bool operator < (match_level_t const &lhs, match_level_t const &rhs)
{
	return match_level_compare(lhs, rhs) < 0;
}

match_level_t &operator += (match_level_t &lhs, uint16_t rhs)
{
	if (rhs == 0)
	{
		// nothing
	}
	else if (lhs.is_single())
	{
		lhs.get_single().modifier_match_level += rhs;
	}
	else if (lhs.is_multi())
	{
		for (auto &val : lhs.get_multi())
		{
			val += rhs;
		}
	}

	return lhs;
}

match_level_t operator + (match_level_t lhs, uint16_t rhs)
{
	lhs += rhs;
	return lhs;
}

match_level_t get_type_match_level(
	ast::typespec_view dest,
	ast::expression const &expr,
	ctx::parse_context &context
)
{
	return generic_type_match(match_context_t<type_match_function_kind::match_level>{
		.expr = expr,
		.dest = dest,
		.context = context,
	});
}

// the logic is duplicated in 'match_expression.cpp' and needs to be in sync
static ast::typespec get_type_for_tuple_decomposition(
	ast::decl_variable const &var_decl,
	bool is_outer_variadic,
	ast::expression const *expr,
	ast::typespec_view expr_type
)
{
	bz_assert(var_decl.tuple_decls.not_empty());
	if (is_outer_variadic && var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>())
	{
		return ast::typespec();
	}

	if (expr != nullptr && expr->is_tuple())
	{
		auto const &tuple_expr = expr->get_tuple();
		if (
			var_decl.tuple_decls.size() != tuple_expr.elems.size()
			&& !(
				var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>()
				&& var_decl.tuple_decls.size() - 1 <= tuple_expr.elems.size()
			)
		)
		{
			return ast::typespec();
		}

		ast::arena_vector<ast::typespec> result_types;
		result_types.reserve(var_decl.tuple_decls.size());
		for (auto const i : bz::iota(0, var_decl.tuple_decls.size()))
		{
			if (var_decl.tuple_decls[i].tuple_decls.not_empty())
			{
				result_types.push_back(get_type_for_tuple_decomposition(
					var_decl.tuple_decls[i],
					is_outer_variadic || var_decl.tuple_decls[i].get_type().is<ast::ts_variadic>(),
					&tuple_expr.elems[i],
					tuple_expr.elems[i].get_expr_type().remove_any_mut_reference()
				));
				if (result_types.back().is_empty())
				{
					return ast::typespec();
				}
			}
			else
			{
				result_types.push_back(var_decl.tuple_decls[i].get_type());
			}
		}

		ast::typespec result = var_decl.get_type();
		result.terminator->emplace<ast::ts_tuple>(std::move(result_types));
		return result;
	}
	else if (expr_type.is<ast::ts_tuple>())
	{
		auto const &tuple_type = expr_type.get<ast::ts_tuple>();
		if (
			var_decl.tuple_decls.size() != tuple_type.types.size()
			&& !(
				var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>()
				&& var_decl.tuple_decls.size() - 1 <= tuple_type.types.size()
			)
		)
		{
			return ast::typespec();
		}

		ast::arena_vector<ast::typespec> result_types;
		result_types.reserve(var_decl.tuple_decls.size());
		for (auto const i : bz::iota(0, var_decl.tuple_decls.size()))
		{
			if (var_decl.tuple_decls[i].tuple_decls.not_empty())
			{
				result_types.push_back(get_type_for_tuple_decomposition(
					var_decl.tuple_decls[i],
					is_outer_variadic || var_decl.tuple_decls[i].get_type().is<ast::ts_variadic>(),
					nullptr,
					tuple_type.types[i]
				));
				if (result_types.back().is_empty())
				{
					return ast::typespec();
				}
			}
			else
			{
				result_types.push_back(var_decl.tuple_decls[i].get_type());
			}
		}

		ast::typespec result = var_decl.get_type();
		result.terminator->emplace<ast::ts_tuple>(std::move(result_types));
		return result;
	}
	else if (expr_type.is<ast::ts_array>())
	{
		auto const &array_type = expr_type.get<ast::ts_array>();
		if (
			var_decl.tuple_decls.size() != array_type.size
			&& !(
				var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>()
				&& var_decl.tuple_decls.size() - 1 <= array_type.size
			)
		)
		{
			return ast::typespec();
		}

		ast::typespec first_decl_type_container;

		auto const first_decl_type = [&]() -> ast::typespec_view {
			if (var_decl.tuple_decls[0].tuple_decls.empty())
			{
				return var_decl.tuple_decls[0].get_type().remove_variadic();
			}
			else
			{
				first_decl_type_container = get_type_for_tuple_decomposition(
					var_decl.tuple_decls[0],
					is_outer_variadic || var_decl.tuple_decls[0].get_type().is_variadic(),
					nullptr,
					array_type.elem_type
				);
				return first_decl_type_container;
			}
		}();
		if (first_decl_type.is_empty())
		{
			return ast::typespec();
		}

		for (auto const &inner_decl : var_decl.tuple_decls.slice(1))
		{
			if (inner_decl.tuple_decls.empty())
			{
				auto const inner_type = inner_decl.get_type().remove_variadic();
				if (first_decl_type != inner_type)
				{
					return ast::typespec();
				}
			}
			else
			{
				auto const inner_type = get_type_for_tuple_decomposition(
					inner_decl,
					is_outer_variadic || inner_decl.get_type().is_variadic(),
					nullptr,
					array_type.elem_type
				);
				if (first_decl_type != inner_type)
				{
					return ast::typespec();
				}
			}
		}

		ast::typespec result = var_decl.get_type();
		if (first_decl_type_container.not_empty())
		{
			result.terminator->emplace<ast::ts_array>(array_type.size, std::move(first_decl_type_container));
		}
		else
		{
			result.terminator->emplace<ast::ts_array>(array_type.size, first_decl_type);
		}
		return result;
	}
	else
	{
		return ast::typespec();
	}
}

match_level_t get_type_match_level(
	ast::decl_variable const &var_decl,
	ast::expression const &expr,
	ctx::parse_context &context
)
{
	if (var_decl.tuple_decls.empty())
	{
		if (var_decl.get_type().is<ast::ts_variadic>())
		{
			return get_type_match_level(var_decl.get_type().get<ast::ts_variadic>(), expr, context);
		}
		else
		{
			return get_type_match_level(var_decl.get_type(), expr, context);
		}
	}

	auto const match_type = get_type_for_tuple_decomposition(
		var_decl,
		var_decl.get_type().is<ast::ts_variadic>(),
		&expr, expr.get_expr_type().remove_any_mut_reference()
	);
	if (match_type.is_empty())
	{
		return {};
	}

	return get_type_match_level(
		match_type,
		expr,
		context
	);
}

match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	bz::array_view<ast::expression const> params,
	ctx::parse_context &context,
	lex::src_tokens const &src_tokens
)
{
	if (func_body.state < ast::resolve_state::parameters)
	{
		context.add_to_resolve_queue(src_tokens, func_body);
		resolve_function_parameters(func_stmt, func_body, context);
		context.pop_resolve_queue();
	}

	if (func_body.state < ast::resolve_state::parameters)
	{
		return match_level_t{};
	}

	if (
		func_body.params.size() != params.size()
		&& !(
			!func_body.params.empty()
			&& func_body.params.back().get_type().is<ast::ts_variadic>()
			&& func_body.params.size() - 1 <= params.size()
		)
	)
	{
		return match_level_t{};
	}

	match_level_t result{};
	auto &result_vec = result.emplace_multi();
	result_vec.reserve(params.size());

	bool good = true;
	auto const add_to_result = [&](match_level_t match) {
		result_vec.push_back(std::move(match));
		good &= match.not_null();
	};

	auto       params_it  = func_body.params.begin();
	auto const params_end = func_body.params.end();
	auto call_it  = params.begin();
	for (; params_it != params_end; ++call_it, ++params_it)
	{
		if (params_it->get_type().is<ast::ts_variadic>())
		{
			break;
		}
		add_to_result(get_type_match_level(*params_it, *call_it, context));
	}
	if (params_it != params_end)
	{
		bz_assert(params_it + 1 == params_end);
		bz_assert(params_it->get_type().is<ast::ts_variadic>());
		auto const call_end = params.end();
		for (; call_it != call_end; ++call_it)
		{
			add_to_result(get_type_match_level(*params_it, *call_it, context));
		}
	}

	if (!good)
	{
		result.clear();
	}
	return result;
}

match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ast::expression const &expr,
	ctx::parse_context &context,
	lex::src_tokens const &src_tokens
)
{
	if (func_body.params.size() != 1)
	{
		return match_level_t{};
	}


	if (func_body.state < ast::resolve_state::parameters)
	{
		context.add_to_resolve_queue(src_tokens, func_body);
		resolve_function_parameters(func_stmt, func_body, context);
		context.pop_resolve_queue();
	}

	if (func_body.state < ast::resolve_state::parameters)
	{
		return match_level_t{};
	}

	return get_type_match_level(func_body.params[0], expr, context);
}

match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ast::expression const &lhs,
	ast::expression const &rhs,
	ctx::parse_context &context,
	lex::src_tokens const &src_tokens
)
{
	if (func_body.params.size() != 2)
	{
		return match_level_t{};
	}


	if (func_body.state < ast::resolve_state::parameters)
	{
		context.add_to_resolve_queue(src_tokens, func_body);
		resolve_function_parameters(func_stmt, func_body, context);
		context.pop_resolve_queue();
	}

	if (func_body.state < ast::resolve_state::parameters)
	{
		return match_level_t{};
	}

	match_level_t result = bz::vector<match_level_t>{};
	auto &result_vec = result.get_multi();
	result_vec.push_back(get_type_match_level(func_body.params[0], lhs, context));
	result_vec.push_back(get_type_match_level(func_body.params[1], rhs, context));
	if (result_vec[0].is_null() || result_vec[1].is_null())
	{
		result.clear();
	}
	return result;
}

} // namespace resolve
