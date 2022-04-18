#include "match_to_type.h"
#include "match_common.h"
#include "match_expression.h"
#include "statement_resolver.h"
#include "parse/consteval.h"

namespace resolve
{

int match_level_compare(match_level_t const &lhs, match_level_t const &rhs)
{
	if (lhs.is_null() && rhs.is_null())
	{
		return 0;
	}
	else if (lhs.is_null())
	{
		return -1;
	}
	else if (rhs.is_null())
	{
		return 1;
	}

	if (lhs.index() != rhs.index())
	{
		return 0;
	}
	else if (lhs.is<int>())
	{
		auto const lhs_int = lhs.get<int>();
		auto const rhs_int = rhs.get<int>();
		return lhs_int < rhs_int ? -1 : lhs_int == rhs_int ? 0 : 1;
	}
	else
	{
		auto const &lhs_vec = lhs.get<bz::vector<match_level_t>>();
		auto const &rhs_vec = rhs.get<bz::vector<match_level_t>>();
		bz_assert(lhs_vec.size() == rhs_vec.size());
		bool has_less_than = false;
		for (auto const &[lhs_val, rhs_val] : bz::zip(lhs_vec, rhs_vec))
		{
			auto const cmp_res = match_level_compare(lhs_val, rhs_val);
			if (cmp_res < 0)
			{
				has_less_than = true;
			}
			else if (cmp_res > 0 && has_less_than)
			{
				// ambiguous
				return 0;
			}
			else if (cmp_res > 0)
			{
				return 1;
			}
		}
		return has_less_than ? -1 : 0;
	}
	bz_unreachable;
	return 1;
}

bool operator < (match_level_t const &lhs, match_level_t const &rhs)
{
	return match_level_compare(lhs, rhs) < 0;
}

match_level_t operator + (match_level_t lhs, int rhs)
{
	if (lhs.is<int>())
	{
		lhs.get<int>() += rhs;
	}
	else if (lhs.is<bz::vector<match_level_t>>())
	{
		for (auto &val : lhs.get<bz::vector<match_level_t>>())
		{
			val = std::move(val) + rhs;
		}
	}
	return lhs;
}

static match_level_t get_strict_typename_match_level(
	ast::typespec_view dest,
	ast::typespec_view source
)
{
	if (!ast::is_complete(source))
	{
		return match_level_t{};
	}

	int result = 0;
	while (dest.kind() == source.kind() && dest.is_safe_blind_get() && source.is_safe_blind_get())
	{
		dest = dest.blind_get();
		source = source.blind_get();
		++result;
	}

	if (dest.is<ast::ts_typename>())
	{
		return result;
	}
	else if (dest.kind() != source.kind())
	{
		return match_level_t{};
	}
	else if (dest.is<ast::ts_array>())
	{
		if (dest.get<ast::ts_array>().size != source.get<ast::ts_array>().size)
		{
			return match_level_t{};
		}
		else
		{
			return get_strict_typename_match_level(dest.get<ast::ts_array>().elem_type, source.get<ast::ts_array>().elem_type) + result;
		}
	}
	else if (dest.is<ast::ts_array_slice>())
	{
		return get_strict_typename_match_level(dest.get<ast::ts_array_slice>().elem_type, source.get<ast::ts_array_slice>().elem_type) + result;
	}
	else if (dest.is<ast::ts_tuple>())
	{
		auto const &dest_types = dest.get<ast::ts_tuple>().types;
		auto const &source_types = source.get<ast::ts_tuple>().types;
		if (dest_types.size() != source_types.size())
		{
			return match_level_t{};
		}

		match_level_t result{};
		auto &result_vec = result.emplace<bz::vector<match_level_t>>();
		result_vec.reserve(dest_types.size());
		for (auto const i : bz::iota(0, dest_types.size()))
		{
			if (!dest_types[i].is_typename() && dest_types[i] != source_types[i])
			{
				result.clear();
				break;
			}
			else
			{
				result_vec.push_back(get_strict_typename_match_level(dest_types[i], source_types[i]));
			}
		}
		return result;
	}
	else
	{
		bz_unreachable;
	}
}

static match_level_t get_strict_type_match_level(
	ast::typespec_view dest,
	ast::typespec_view source,
	bool accept_void
)
{
	bz_assert(ast::is_complete(source));
	int result = 0;
	while (dest.kind() == source.kind() && dest.is_safe_blind_get() && source.is_safe_blind_get())
	{
		dest = dest.blind_get();
		source = source.blind_get();
		++result;
	}
	bz_assert(!dest.is<ast::ts_unresolved>());
	bz_assert(!source.is<ast::ts_unresolved>());

	if (dest.is<ast::ts_auto>() && !source.is<ast::ts_const>())
	{
		bz_assert(!source.is<ast::ts_consteval>());
		return result + 1;
	}
	else if (dest == source)
	{
		return result + 3;
	}
	else if (accept_void && dest.is<ast::ts_void>() && !source.is<ast::ts_const>())
	{
		return result;
	}
	else if (dest.is<ast::ts_tuple>() && source.is<ast::ts_tuple>())
	{
		auto const &source_tuple_types = source.get<ast::ts_tuple>().types;
		bz::vector<ast::typespec> dest_tuple_types_variadic;
		auto const &dest_tuple_types_ref = dest.get<ast::ts_tuple>().types;
		if (dest_tuple_types_ref.not_empty() && dest_tuple_types_ref.back().is<ast::ts_variadic>())
		{
			dest_tuple_types_variadic = dest_tuple_types_ref;
			expand_variadic_tuple_type(dest_tuple_types_variadic, source_tuple_types.size());
		}
		auto const &dest_tuple_types = dest_tuple_types_ref.not_empty() && dest_tuple_types_ref.back().is<ast::ts_variadic>()
			? dest_tuple_types_variadic
			: dest_tuple_types_ref;
		if (dest_tuple_types.size() == source_tuple_types.size())
		{
			match_level_t result = bz::vector<match_level_t>{};
			auto &result_vec = result.get<bz::vector<match_level_t>>();
			auto good = true;
			for (auto const &[source_elem_t, dest_elem_t] : bz::zip(source_tuple_types, dest_tuple_types))
			{
				result_vec.push_back(get_strict_type_match_level(dest_elem_t, source_elem_t, false));
				good &= result_vec.back().not_null();
			}
			if (!good)
			{
				result.clear();
			}
			return result;
		}
		else
		{
			return match_level_t{};
		}
	}
	else if (
		dest.is<ast::ts_base_type>() && dest.get<ast::ts_base_type>().info->is_generic()
		&& source.is<ast::ts_base_type>() && source.get<ast::ts_base_type>().info->is_generic_instantiation()
		&& source.get<ast::ts_base_type>().info->generic_parent == dest.get<ast::ts_base_type>().info
	)
	{
		return result + 2;
	}
	else if (dest.is<ast::ts_array_slice>() && source.is<ast::ts_array_slice>())
	{
		return get_strict_type_match_level(
			dest.get<ast::ts_array_slice>().elem_type,
			source.get<ast::ts_array_slice>().elem_type,
			false
		) + result;
	}
	else
	{
		return match_level_t{};
	}
}

static match_level_t get_type_match_level(
	ast::typespec_view expr_type,
	ast::typespec_view dest,
	ast::expression_type_kind expr_type_kind,
	ctx::parse_context &context
)
{
	auto const expr_type_without_const = ast::remove_const_or_consteval(expr_type);

	if (dest.is<ast::ts_pointer>())
	{
		if (expr_type_without_const.is<ast::ts_pointer>())
		{
			auto const inner_dest = dest.get<ast::ts_pointer>();
			auto const inner_expr_type = expr_type_without_const.get<ast::ts_pointer>();
			if (inner_dest.is<ast::ts_const>())
			{
				if (inner_expr_type.is<ast::ts_const>())
				{
					return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), inner_expr_type.get<ast::ts_const>(), true) + 2;
				}
				else
				{
					return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), inner_expr_type, true) + 1;
				}
			}
			else
			{
				return get_strict_type_match_level(inner_dest, inner_expr_type, true) + 2;
			}
		}
		// special case for null
		else if (
			expr_type_without_const.is<ast::ts_base_type>()
			&& expr_type_without_const.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_
		)
		{
			if (ast::is_complete(dest))
			{
				// a conversion takes place, so its match level is 0
				return 0;
			}
			else
			{
				return match_level_t{};
			}
		}
	}
	else if (dest.is<ast::ts_lvalue_reference>())
	{
		if (expr_type_kind != ast::expression_type_kind::lvalue && expr_type_kind != ast::expression_type_kind::lvalue_reference)
		{
			return match_level_t{};
		}

		auto const inner_dest = dest.get<ast::ts_lvalue_reference>();
		if (inner_dest.is<ast::ts_const>())
		{
			if (expr_type.is<ast::ts_const>())
			{
				return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), expr_type_without_const, false) + 5;
			}
			else
			{
				return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), expr_type_without_const, false) + 2;
			}
		}
		else
		{
			return get_strict_type_match_level(inner_dest, expr_type, false) + 5;
		}
	}
	else if (dest.is<ast::ts_move_reference>())
	{
		if (expr_type_kind != ast::expression_type_kind::rvalue && expr_type_kind != ast::expression_type_kind::moved_lvalue)
		{
			return match_level_t{};
		}

		auto const inner_dest = dest.get<ast::ts_move_reference>();
		return get_strict_type_match_level(inner_dest, expr_type_without_const, false) + 5;
	}
	else if (dest.is<ast::ts_auto_reference>())
	{
		auto const inner_dest = dest.get<ast::ts_auto_reference>();
		if (inner_dest.is<ast::ts_const>())
		{
			if (expr_type.is<ast::ts_const>())
			{
				return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), expr_type_without_const, false) + 4;
			}
			else
			{
				return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), expr_type_without_const, false) + 1;
			}
		}
		else
		{
			return get_strict_type_match_level(inner_dest, expr_type, false) + 4;
		}
	}
	else if (dest.is<ast::ts_auto_reference_const>())
	{
		auto const inner_dest = dest.get<ast::ts_auto_reference_const>();
		bz_assert(!inner_dest.is<ast::ts_const>());
		return get_strict_type_match_level(inner_dest, expr_type_without_const, false) + 3;
	}

	// only implicit type conversions are left
	// + 2 needs to be added everywhere, because it didn't match reference and reference const qualifier
	if (
		dest.is<ast::ts_array_slice>()
		&& (expr_type_without_const.is<ast::ts_array>() || (expr_type_without_const.is<ast::ts_array_slice>()))
	)
	{
		auto const dest_elem_t = dest.get<ast::ts_array_slice>().elem_type.as_typespec_view();
		auto const expr_elem_t = expr_type_without_const.is<ast::ts_array>()
			? expr_type_without_const.get<ast::ts_array>().elem_type.as_typespec_view()
			: expr_type_without_const.get<ast::ts_array_slice>().elem_type.as_typespec_view();
		auto const is_const_expr_elem_t = expr_type_without_const.is<ast::ts_array>()
			? expr_type.is<ast::ts_const>()
			: expr_elem_t.is<ast::ts_const>();
		auto const expr_elem_t_without_const = ast::remove_const_or_consteval(expr_elem_t);
		int const is_slice = expr_type_without_const.is<ast::ts_array_slice>() ? 1 : 0;
		if (dest_elem_t.is<ast::ts_const>())
		{
			if (is_const_expr_elem_t)
			{
				return get_strict_type_match_level(dest_elem_t.get<ast::ts_const>(), expr_elem_t_without_const, false) + (2 + is_slice);
			}
			else
			{
				return get_strict_type_match_level(dest_elem_t.get<ast::ts_const>(), expr_elem_t_without_const, false) + (1 + is_slice);
			}
		}
		else
		{
			if (is_const_expr_elem_t)
			{
				return match_level_t{};
			}
			else
			{
				return get_strict_type_match_level(dest_elem_t, expr_elem_t_without_const, false) + (2 + is_slice);
			}
		}
	}
	else if (dest.is<ast::ts_tuple>() && expr_type_without_const.is<ast::ts_tuple>())
	{
		auto const &expr_tuple_types = expr_type_without_const.get<ast::ts_tuple>().types;
		bz::vector<ast::typespec> dest_tuple_types_variadic;
		auto const &dest_tuple_types_ref = dest.get<ast::ts_tuple>().types;
		if (dest_tuple_types_ref.not_empty() && dest_tuple_types_ref.back().is<ast::ts_variadic>())
		{
			dest_tuple_types_variadic = dest_tuple_types_ref;
			expand_variadic_tuple_type(dest_tuple_types_variadic, expr_tuple_types.size());
		}
		auto const &dest_tuple_types = dest_tuple_types_ref.not_empty() && dest_tuple_types_ref.back().is<ast::ts_variadic>()
			? dest_tuple_types_variadic
			: dest_tuple_types_ref;
		if (dest_tuple_types.size() == expr_tuple_types.size())
		{
			match_level_t result = bz::vector<match_level_t>{};
			auto &result_vec = result.get<bz::vector<match_level_t>>();
			bool good = true;
			for (auto const &[expr_elem_t, dest_elem_t] : bz::zip(expr_tuple_types, dest_tuple_types))
			{
				if (expr_elem_t.is<ast::ts_lvalue_reference>())
				{
					result_vec.push_back(get_type_match_level(
						expr_elem_t.get<ast::ts_lvalue_reference>(),
						dest_elem_t,
						ast::expression_type_kind::lvalue_reference,
						context
					));
				}
				else
				{
					result_vec.push_back(get_type_match_level(expr_elem_t, dest_elem_t, expr_type_kind, context));
				}
				good &= result_vec.back().not_null();
			}
			if (!good)
			{
				result.clear();
			}
			return result;
		}
	}
	else if (dest.is<ast::ts_auto>() || (dest.is<ast::ts_base_type>() && dest.get<ast::ts_base_type>().info->is_generic()))
	{
		return get_strict_type_match_level(dest, expr_type_without_const, false);
	}
	else if (dest == expr_type_without_const)
	{
		return 2;
	}
	return match_level_t{};
}

static match_level_t get_if_expr_type_match_level(
	ast::typespec_view dest,
	ast::expr_if &if_expr,
	ctx::parse_context &context
)
{
	if (if_expr.then_block.is_noreturn() && !if_expr.else_block.is_noreturn())
	{
		return get_type_match_level(dest, if_expr.else_block, context);
	}
	else if (!if_expr.then_block.is_noreturn() && if_expr.else_block.is_noreturn())
	{
		return get_type_match_level(dest, if_expr.then_block, context);
	}
	else
	{
		auto then_result = get_type_match_level(dest, if_expr.then_block, context);
		auto else_result = get_type_match_level(dest, if_expr.else_block, context);
		if (then_result.is_null() || else_result.is_null())
		{
			return match_level_t{};
		}
		else if (ast::is_complete(dest))
		{
			match_level_t result;
			auto &vec = result.emplace<bz::vector<match_level_t>>();
			vec.reserve(2);
			vec.push_back(std::move(then_result));
			vec.push_back(std::move(else_result));
			return result;
		}

		ast::typespec then_matched_type = dest;
		ast::typespec else_matched_type = dest;
		match_expression_to_type(if_expr.then_block, then_matched_type, context);
		match_expression_to_type(if_expr.else_block, else_matched_type, context);

		if (then_matched_type == else_matched_type)
		{
			match_level_t result;
			auto &vec = result.emplace<bz::vector<match_level_t>>();
			vec.reserve(2);
			vec.push_back(std::move(then_result));
			vec.push_back(std::move(else_result));
			return result;
		}
		else
		{
			return match_level_t{};
		}
	}
}

static match_level_t get_switch_expr_type_match_level(
	ast::typespec_view dest,
	ast::expr_switch &switch_expr,
	ctx::parse_context &context
)
{
	auto case_match_levels = switch_expr.cases
		.filter([](auto const &case_expr) { return !case_expr.expr.is_noreturn(); })
		.transform([&dest, &context](auto &case_expr) {
			return get_type_match_level(dest, case_expr.expr, context);
		})
		.collect(switch_expr.cases.size() + 1);
	if (switch_expr.default_case.not_null() && !switch_expr.default_case.is_noreturn())
	{
		case_match_levels.push_back(get_type_match_level(dest, switch_expr.default_case, context));
	}
	bz_assert(case_match_levels.not_empty());
	auto const is_error = case_match_levels.is_any([](auto const &match_level) { return match_level.is_null(); });
	if (is_error)
	{
		return match_level_t{};
	}
	else if (ast::is_complete(dest) || case_match_levels.size() == 1)
	{
		return match_level_t(std::move(case_match_levels));
	}
	else
	{
		ast::typespec dest_copy = dest;
		auto valild_case_expr_range = switch_expr.cases
			.filter([](auto const &case_expr) { return !case_expr.expr.is_noreturn(); });
		bz_assert(!valild_case_expr_range.at_end());
		match_expression_to_type(valild_case_expr_range.front().expr, dest_copy, context);
		++valild_case_expr_range;
		for (auto &[_, case_expr] : valild_case_expr_range)
		{
			ast::typespec dest_copy_copy = dest;
			match_expression_to_type(case_expr, dest_copy_copy, context);
			if (dest_copy_copy != dest_copy)
			{
				return match_level_t{};
			}
		}
		if (switch_expr.default_case.not_null() && !switch_expr.default_case.is_noreturn())
		{
			ast::typespec dest_copy_copy = dest;
			match_expression_to_type(switch_expr.default_case, dest_copy_copy, context);
			if (dest_copy_copy != dest_copy)
			{
				return match_level_t{};
			}
		}
		return match_level_t(std::move(case_match_levels));
	}
}

match_level_t get_type_match_level(
	ast::typespec_view dest,
	ast::expression &expr,
	ctx::parse_context &context
)
{
	// six base cases:
	// *T
	//     -> if expr is of pointer type strict match U to T
	//     -> else expr is some base type, try implicitly casting it
	//     -> special case for *void
	// *const T
	//     -> same as before, but U doesn't have to be const (no need to strict match), +1 match level if U is const
	// &T
	//     -> expr must be an lvalue
	//     -> strict match type of expr to T
	//     -> +1 match level because it matched the refererence
	// &const T
	//     -> expr must be an lvalue
	//     -> type of expr doesn't need to be const, +1 match level if U is const
	//     -> +1 match level because it matched the refererence
	// T
	//     -> if type of expr is T, then there's nothing to do
	//     -> else try to implicitly cast expr to T
	// const T -> match to T (no need to worry about const)
	dest = ast::remove_const_or_consteval(dest);

	if (expr.is_if_expr())
	{
		return get_if_expr_type_match_level(dest, expr.get_if_expr(), context);
	}
	else if (expr.is_switch_expr())
	{
		return get_switch_expr_type_match_level(dest, expr.get_switch_expr(), context);
	}
	else if (expr.is_typename())
	{
		if (!dest.is_typename())
		{
			return match_level_t{};
		}

		return get_strict_typename_match_level(dest, expr.get_typename());
	}
	else if (expr.is_tuple())
	{
		if (dest.is<ast::ts_auto_reference>() || dest.is<ast::ts_auto_reference_const>())
		{
			dest = dest.blind_get();
			dest = ast::remove_const_or_consteval(dest);
		}

		if (dest.is<ast::ts_auto>())
		{
			auto &tuple_expr = expr.get_tuple();
			match_level_t result = bz::vector<match_level_t>{};
			auto &result_vec = result.get<bz::vector<match_level_t>>();
			for (auto &elem : tuple_expr.elems)
			{
				result_vec.push_back(get_type_match_level(dest, elem, context));
			}
			return result;
		}
		else if (dest.is<ast::ts_tuple>())
		{
			auto &expr_tuple_elems = expr.get_tuple().elems;
			bz::vector<ast::typespec> dest_tuple_types_variadic;
			auto const &dest_tuple_types_ref = dest.get<ast::ts_tuple>().types;
			if (dest_tuple_types_ref.not_empty() && dest_tuple_types_ref.back().is<ast::ts_variadic>())
			{
				dest_tuple_types_variadic = dest_tuple_types_ref;
				expand_variadic_tuple_type(dest_tuple_types_variadic, expr_tuple_elems.size());
			}
			auto const &dest_tuple_types = dest_tuple_types_ref.not_empty() && dest_tuple_types_ref.back().is<ast::ts_variadic>()
				? dest_tuple_types_variadic
				: dest_tuple_types_ref;
			if (dest_tuple_types.size() == expr_tuple_elems.size())
			{
				match_level_t result = bz::vector<match_level_t>{};
				auto &result_vec = result.get<bz::vector<match_level_t>>();
				bool good = true;
				for (auto const &[elem, type] : bz::zip(expr_tuple_elems, dest_tuple_types))
				{
					result_vec.push_back(get_type_match_level(type, elem, context));
					good &= result_vec.back().not_null();
				}
				if (!good)
				{
					result.clear();
				}
				return result;
			}
			else
			{
				return {};
			}
		}
		else
		{
			return {};
		}
	}

	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	auto result = get_type_match_level(expr_type, dest, expr_type_kind, context);
	if (result.is_null() && is_implicitly_convertible(dest, expr, context))
	{
		return 1;
	}
	else
	{
		return result;
	}
}

match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	bz::array_view<ast::expression> params,
	ctx::parse_context &context,
	lex::src_tokens src_tokens
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

	match_level_t result = bz::vector<match_level_t>{};
	auto &result_vec = result.get<bz::vector<match_level_t>>();
	result_vec.reserve(params.size());

	bool good = true;
	auto const add_to_result = [&](match_level_t match)
	{
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
		add_to_result(get_type_match_level(params_it->get_type(), *call_it, context));
	}
	if (params_it != params_end)
	{
		bz_assert(params_it + 1 == params_end);
		auto const param_type = params_it->get_type().get<ast::ts_variadic>();
		auto const call_end = params.end();
		for (; call_it != call_end; ++call_it)
		{
			add_to_result(get_type_match_level(param_type, *call_it, context));
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
	ast::expression &expr,
	ctx::parse_context &context,
	lex::src_tokens src_tokens
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

	return get_type_match_level(func_body.params[0].get_type(), expr, context);
}

match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ast::expression &lhs,
	ast::expression &rhs,
	ctx::parse_context &context,
	lex::src_tokens src_tokens
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
	auto &result_vec = result.get<bz::vector<match_level_t>>();
	result_vec.push_back(get_type_match_level(func_body.params[0].get_type(), lhs, context));
	result_vec.push_back(get_type_match_level(func_body.params[1].get_type(), rhs, context));
	if (result_vec[0].is_null() || result_vec[1].is_null())
	{
		result.clear();
	}
	return result;
}

} // namespace resolve
