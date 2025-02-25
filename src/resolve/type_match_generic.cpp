#include "type_match_generic.h"

namespace resolve
{

template<type_match_function_kind kind>
static match_context_t<kind> change_expr(
	match_context_t<kind> const &match_context,
	typename match_context_t<kind>::expr_ref_type new_expr
)
{
	if constexpr (kind == type_match_function_kind::can_match)
	{
		return match_context_t<type_match_function_kind::can_match>{
			.expr = new_expr,
			.dest = match_context.dest,
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_level)
	{
		return match_context_t<type_match_function_kind::match_level>{
			.expr = new_expr,
			.dest = match_context.dest,
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::matched_type)
	{
		return match_context_t<type_match_function_kind::matched_type>{
			.expr = new_expr,
			.dest = match_context.dest,
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_expression)
	{
		return match_context_t<type_match_function_kind::match_expression>{
			.expr = new_expr,
			.dest_container = match_context.dest_container,
			.dest = match_context.dest,
			.context = match_context.context,
		};
	}
	else
	{
		static_assert(bz::meta::always_false<match_context_t<kind>>);
	}
}

template<type_match_function_kind kind, typename DestType>
static match_context_t<kind> change_dest_and_dest_container(
	match_context_t<kind> const &match_context,
	DestType &&new_dest
)
{
	if constexpr (kind == type_match_function_kind::can_match)
	{
		return match_context_t<type_match_function_kind::can_match>{
			.expr = match_context.expr,
			.dest = std::forward<DestType>(new_dest),
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_level)
	{
		return match_context_t<type_match_function_kind::match_level>{
			.expr = match_context.expr,
			.dest = std::forward<DestType>(new_dest),
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::matched_type)
	{
		return match_context_t<type_match_function_kind::matched_type>{
			.expr = match_context.expr,
			.dest = std::forward<DestType>(new_dest),
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_expression)
	{
		match_context.dest_container = std::forward<DestType>(new_dest);
		return match_context_t<type_match_function_kind::match_expression>{
			.expr = match_context.expr,
			.dest_container = match_context.dest_container,
			.dest = match_context.dest_container,
			.context = match_context.context,
		};
	}
	else
	{
		static_assert(bz::meta::always_false<match_context_t<kind>>);
	}
}

template<type_match_function_kind kind>
static match_context_t<kind> change_dest(
	match_context_t<kind> const &match_context,
	ast::typespec_view new_dest
)
{
	if constexpr (kind == type_match_function_kind::can_match)
	{
		return match_context_t<type_match_function_kind::can_match>{
			.expr = match_context.expr,
			.dest = new_dest,
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_level)
	{
		return match_context_t<type_match_function_kind::match_level>{
			.expr = match_context.expr,
			.dest = new_dest,
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::matched_type)
	{
		return match_context_t<type_match_function_kind::matched_type>{
			.expr = match_context.expr,
			.dest = new_dest,
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_expression)
	{
		return match_context_t<type_match_function_kind::match_expression>{
			.expr = match_context.expr,
			.dest_container = match_context.dest_container,
			.dest = new_dest,
			.context = match_context.context,
		};
	}
	else
	{
		static_assert(bz::meta::always_false<match_context_t<kind>>);
	}
}


template<type_match_function_kind kind>
static strict_match_context_t<kind> make_strict_match_context(
	match_context_t<kind> const &match_context,
	ast::typespec_view source,
	ast::typespec_view dest,
	ast::typespec_view original_dest,
	reference_match_kind reference_match,
	type_match_kind base_type_match
)
{
	if constexpr (kind == type_match_function_kind::can_match)
	{
		return strict_match_context_t<type_match_function_kind::can_match>{
			.source = source,
			.dest = dest,
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_level)
	{
		return strict_match_context_t<type_match_function_kind::match_level>{
			.source = source,
			.dest = dest,
			.reference_match = reference_match,
			.base_type_match = base_type_match,
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::matched_type)
	{
		return strict_match_context_t<type_match_function_kind::matched_type>{
			.source = source,
			.dest = dest,
			.original_dest = original_dest,
			.context = match_context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_expression)
	{
		return strict_match_context_t<type_match_function_kind::match_expression>{
			.expr = match_context.expr,
			.original_dest_container = match_context.dest_container,
			.dest_container = match_context.dest_container,
			.source = source,
			.dest = dest,
			.context = match_context.context,
		};
	}
	else
	{
		static_assert(bz::meta::always_false<match_context_t<kind>>);
	}
}

template<type_match_function_kind kind>
static strict_match_context_t<kind> change_inner_dest(
	strict_match_context_t<kind> const &context,
	ast::typespec_view new_dest
)
{
	if constexpr (kind == type_match_function_kind::can_match)
	{
		return strict_match_context_t<type_match_function_kind::can_match>{
			.source = context.source,
			.dest = new_dest,
			.context = context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_level)
	{
		return strict_match_context_t<type_match_function_kind::match_level>{
			.source = context.source,
			.dest = new_dest,
			.reference_match = context.reference_match,
			.base_type_match = context.base_type_match,
			.context = context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::matched_type)
	{
		return strict_match_context_t<type_match_function_kind::matched_type>{
			.source = context.source,
			.dest = new_dest,
			.original_dest = context.original_dest,
			.context = context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_expression)
	{
		return strict_match_context_t<type_match_function_kind::match_expression>{
			.expr = context.expr,
			.original_dest_container = context.original_dest_container,
			.dest_container = context.dest_container,
			.source = context.source,
			.dest = new_dest,
			.context = context.context,
		};
	}
	else
	{
		static_assert(bz::meta::always_false<strict_match_context_t<kind>>);
	}
}

template<type_match_function_kind kind>
static strict_match_context_t<kind> change_inner_dest_and_source(
	strict_match_context_t<kind> const &context,
	ast::typespec_view new_dest,
	ast::typespec_view new_source
)
{
	if constexpr (kind == type_match_function_kind::can_match)
	{
		return strict_match_context_t<type_match_function_kind::can_match>{
			.source = new_source,
			.dest = new_dest,
			.context = context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_level)
	{
		return strict_match_context_t<type_match_function_kind::match_level>{
			.source = new_source,
			.dest = new_dest,
			.reference_match = context.reference_match,
			.base_type_match = context.base_type_match,
			.context = context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::matched_type)
	{
		return strict_match_context_t<type_match_function_kind::matched_type>{
			.source = new_source,
			.dest = new_dest,
			.original_dest = context.original_dest,
			.context = context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_expression)
	{
		return strict_match_context_t<type_match_function_kind::match_expression>{
			.expr = context.expr,
			.original_dest_container = context.original_dest_container,
			.dest_container = context.dest_container,
			.source = new_source,
			.dest = new_dest,
			.context = context.context,
		};
	}
	else
	{
		static_assert(bz::meta::always_false<strict_match_context_t<kind>>);
	}
}

template<type_match_function_kind kind, typename DestType>
static strict_match_context_t<kind> change_dest_and_source(
	strict_match_context_t<kind> const &context,
	DestType &&new_dest,
	ast::typespec_view new_source
)
{
	if constexpr (kind == type_match_function_kind::can_match)
	{
		return strict_match_context_t<type_match_function_kind::can_match>{
			.source = new_source,
			.dest = new_dest,
			.context = context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_level)
	{
		return strict_match_context_t<type_match_function_kind::match_level>{
			.source = new_source,
			.dest = new_dest,
			.reference_match = context.reference_match,
			.base_type_match = context.base_type_match,
			.context = context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::matched_type)
	{
		return strict_match_context_t<type_match_function_kind::matched_type>{
			.source = new_source,
			.dest = new_dest,
			.original_dest = new_dest,
			.context = context.context,
		};
	}
	else if constexpr (kind == type_match_function_kind::match_expression)
	{
		return strict_match_context_t<type_match_function_kind::match_expression>{
			.expr = context.expr,
			.original_dest_container = context.original_dest_container,
			.dest_container = new_dest,
			.source = new_source,
			.dest = new_dest,
			.context = context.context,
		};
	}
	else
	{
		static_assert(bz::meta::always_false<strict_match_context_t<kind>>);
	}
}


template<type_match_function_kind kind>
static match_function_result_t<kind> generic_type_match_if_expr_complete_type(match_context_t<kind> const &match_context)
{
	bz_assert(match_context.expr.is_if_expr());
	auto &if_expr = match_context.expr.get_if_expr();
	bz_assert(ast::is_complete(match_context.dest));

	if constexpr (kind == type_match_function_kind::can_match)
	{
		return generic_type_match(change_expr(match_context, if_expr.then_block))
			&& generic_type_match(change_expr(match_context, if_expr.else_block));
	}
	else if constexpr (kind == type_match_function_kind::match_level)
	{
		auto result = match_level_t();
		auto &result_vec = result.emplace_multi();
		result_vec.reserve(2);
		result_vec.push_back(generic_type_match(change_expr(match_context, if_expr.then_block)));
		if (result_vec[0].is_null())
		{
			result.clear();
			return result;
		}

		result_vec.push_back(generic_type_match(change_expr(match_context, if_expr.else_block)));
		if (result_vec[1].is_null())
		{
			result.clear();
		}

		return result;
	}
	else if constexpr (kind == type_match_function_kind::matched_type)
	{
		auto const can_match_context = match_context_t<type_match_function_kind::can_match>{
			.expr = match_context.expr,
			.dest = match_context.dest,
			.context = match_context.context,
		};
		if (generic_type_match_if_expr_complete_type(can_match_context))
		{
			return match_context.dest;
		}
		else
		{
			return ast::typespec();
		}
	}
	else if constexpr (kind == type_match_function_kind::match_expression)
	{
		auto const is_then_good = generic_type_match(change_expr(match_context, if_expr.then_block));
		auto const is_else_good = generic_type_match(change_expr(match_context, if_expr.else_block));
		return is_then_good && is_else_good;
	}
	else
	{
		static_assert(bz::meta::always_false<match_context_t<kind>>);
	}
}

template<type_match_function_kind kind>
static match_function_result_t<kind> generic_type_match_if_expr(match_context_t<kind> const &match_context)
{
	auto &if_expr = match_context.expr.get_if_expr();

	if (if_expr.else_block.is_null())
	{
		if constexpr (match_context_t<kind>::report_errors)
		{
			match_context.context.report_error(
				match_context.expr.src_tokens,
				bz::format("unable to match if expression to type '{}', because the expression is missing the else branch", match_context.dest_container)
			);
		}
		return match_function_result_t<kind>();
	}

	auto const is_then_valid = !if_expr.then_block.is_noreturn();
	auto const is_else_valid = !if_expr.else_block.is_noreturn();

	if (is_then_valid && !is_else_valid)
	{
		return generic_type_match(change_expr(match_context, if_expr.then_block));
	}
	else if (!is_then_valid && is_else_valid)
	{
		return generic_type_match(change_expr(match_context, if_expr.else_block));
	}

	if (ast::is_complete(match_context.dest))
	{
		return generic_type_match_if_expr_complete_type(match_context);
	}

	auto then_matched_type = generic_type_match(match_context_t<type_match_function_kind::matched_type>{
		.expr = if_expr.then_block,
		.dest = match_context.dest,
		.context = match_context.context,
	});
	auto else_matched_type = generic_type_match(match_context_t<type_match_function_kind::matched_type>{
		.expr = if_expr.else_block,
		.dest = match_context.dest,
		.context = match_context.context,
	});

	if (then_matched_type.is_empty() || else_matched_type.is_empty())
	{
		if constexpr (kind == type_match_function_kind::match_expression)
		{
			if (then_matched_type.is_empty())
			{
				generic_type_match(change_expr(match_context, if_expr.then_block));
			}
			if (else_matched_type.is_empty())
			{
				generic_type_match(change_expr(match_context, if_expr.else_block));
			}
		}
		return match_function_result_t<kind>();
	}
	else if (then_matched_type == else_matched_type)
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return true;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			auto result = match_level_t();
			auto &result_vec = result.emplace_multi();
			result_vec.reserve(2);
			auto const new_context = change_dest_and_dest_container(match_context, std::move(then_matched_type));
			result_vec.push_back(generic_type_match(change_expr(new_context, if_expr.then_block)));
			result_vec.push_back(generic_type_match(change_expr(new_context, if_expr.else_block)));
			bz_assert(result_vec[0].not_null() && result_vec[1].not_null());
			return result;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			return then_matched_type;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto const new_context = change_dest_and_dest_container(match_context, std::move(then_matched_type));
			auto const then_match_result = generic_type_match(change_expr(new_context, if_expr.then_block));
			auto const else_match_result = generic_type_match(change_expr(new_context, if_expr.else_block));
			return then_match_result && else_match_result;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else
	{
		auto const can_then_match = generic_type_match(match_context_t<type_match_function_kind::can_match>{
			.expr = if_expr.then_block,
			.dest = else_matched_type,
			.context = match_context.context,
		});
		auto const can_else_match = generic_type_match(match_context_t<type_match_function_kind::can_match>{
			.expr = if_expr.else_block,
			.dest = then_matched_type,
			.context = match_context.context,
		});
		if (!can_then_match && can_else_match)
		{
			return generic_type_match_if_expr_complete_type(change_dest_and_dest_container(match_context, std::move(then_matched_type)));
		}
		else if (can_then_match && !can_else_match)
		{
			return generic_type_match_if_expr_complete_type(change_dest_and_dest_container(match_context, std::move(else_matched_type)));
		}
		else
		{
			// ambiguous
			if constexpr (match_context_t<kind>::report_errors)
			{
				match_context.context.report_error(
					match_context.expr.src_tokens,
					bz::format("matching the two branches of the if expression to type '{}' is ambiguous", match_context.dest_container),
					{
						match_context.context.make_note(
							if_expr.then_block,
							bz::format("resulting type from matching the then branch is '{}'", then_matched_type)
						),
						match_context.context.make_note(
							if_expr.else_block,
							bz::format("resulting type from matching the else branch is '{}'", else_matched_type)
						)
					}
				);
			}
			return match_function_result_t<kind>();
		}
	}
}

template<type_match_function_kind kind>
static match_function_result_t<kind> generic_type_match_switch_expr_complete_type(match_context_t<kind> const &match_context)
{
	bz_assert(match_context.expr.is_switch_expr());
	auto &switch_expr = match_context.expr.get_switch_expr();
	bz_assert(ast::is_complete(match_context.dest));

	if constexpr (kind == type_match_function_kind::can_match)
	{
		return switch_expr.cases
			.is_all([&](auto const &case_) {
				return !case_.expr.is_noreturn() && generic_type_match(change_expr(match_context, case_.expr));
			})
			&& (
				switch_expr.default_case.is_null()
				|| switch_expr.default_case.is_noreturn()
				|| generic_type_match(change_expr(match_context, switch_expr.default_case))
			);
	}
	else if constexpr (kind == type_match_function_kind::match_level)
	{
		auto result = match_level_t();
		auto &result_vec = result.emplace_multi();
		result_vec.reserve(switch_expr.cases.size() + (switch_expr.default_case.not_null() ? 1 : 0));
		for (auto const &[_, expr] : switch_expr.cases)
		{
			if (!expr.is_noreturn())
			{
				result_vec.push_back(generic_type_match(change_expr(match_context, expr)));
				if (result_vec.back().is_null())
				{
					result.clear();
					return result;
				}
			}
		}

		if (switch_expr.default_case.not_null() && !switch_expr.default_case.is_noreturn())
		{
			result_vec.push_back(generic_type_match(change_expr(match_context, switch_expr.default_case)));
			if (result_vec.back().is_null())
			{
				result.clear();
				return result;
			}
		}

		return result;
	}
	else if constexpr (kind == type_match_function_kind::matched_type)
	{
		auto const can_match_context = match_context_t<type_match_function_kind::can_match>{
			.expr = match_context.expr,
			.dest = match_context.dest,
			.context = match_context.context,
		};
		if (generic_type_match_switch_expr_complete_type(can_match_context))
		{
			return match_context.dest;
		}
		else
		{
			return ast::typespec();
		}
	}
	else if constexpr (kind == type_match_function_kind::match_expression)
	{
		auto const are_cases_good = switch_expr.cases
			.filter([](auto const &case_) { return !case_.expr.is_noreturn(); })
			.transform([&](auto &case_) { return generic_type_match(change_expr(match_context, case_.expr)); })
			.reduce(true, [](auto const lhs, auto const rhs) { return lhs && rhs; });
		auto const is_default_case_good = switch_expr.default_case.is_null()
			|| switch_expr.default_case.is_noreturn()
			|| generic_type_match(change_expr(match_context, switch_expr.default_case));
		return are_cases_good && is_default_case_good;
	}
	else
	{
		static_assert(bz::meta::always_false<match_context_t<kind>>);
	}
}

template<type_match_function_kind kind>
static match_function_result_t<kind> generic_type_match_switch_expr(match_context_t<kind> const &match_context)
{
	auto &switch_expr = match_context.expr.get_switch_expr();

	auto const valid_case_count = switch_expr.cases
		.filter([](auto const &case_) { return !case_.expr.is_noreturn(); })
		.count();
	auto const is_default_valid = switch_expr.default_case.not_null() && !switch_expr.default_case.is_noreturn();

	if (valid_case_count == 0 && is_default_valid)
	{
		return generic_type_match(change_expr(match_context, switch_expr.default_case));
	}
	else if (valid_case_count == 1 && !is_default_valid)
	{
		auto &valid_case_expr = switch_expr.cases
			.filter([](auto const &case_) { return !case_.expr.is_noreturn(); })
			.front().expr;
		return generic_type_match(change_expr(match_context, valid_case_expr));
	}

	if (ast::is_complete(match_context.dest))
	{
		return generic_type_match_switch_expr_complete_type(match_context);
	}

	bool any_failed_matches = false;
	lex::src_tokens first_match_src_tokens = {};
	ast::typespec matched_type;

	auto const check_expr_matched_type = [&](typename match_context_t<kind>::expr_ref_type expr) -> bool {
		if (matched_type.is_empty())
		{
			matched_type = generic_type_match(match_context_t<type_match_function_kind::matched_type>{
				.expr = expr,
				.dest = match_context.dest,
				.context = match_context.context,
			});
			first_match_src_tokens = expr.src_tokens;
			return kind == type_match_function_kind::match_expression || !matched_type.is_empty();
		}
		else
		{
			auto const case_matched_type = generic_type_match(match_context_t<type_match_function_kind::matched_type>{
				.expr = expr,
				.dest = match_context.dest,
				.context = match_context.context,
			});
			auto const is_equal = case_matched_type == matched_type;
			if (!is_equal && !case_matched_type.is_empty())
			{
				if constexpr (match_context_t<kind>::report_errors)
				{
					match_context.context.report_error(
						match_context.expr.src_tokens,
						bz::format("different types deduced for different cases in switch expression while matching to type '{}'", match_context.dest_container),
						{
							match_context.context.make_note(first_match_src_tokens, bz::format("type was first deduced as '{}'", matched_type)),
							match_context.context.make_note(expr.src_tokens, bz::format("type was later deduced as '{}'", case_matched_type)),
						}
					);
					return false;
				}
			}
			else if constexpr (kind == type_match_function_kind::match_expression)
			{
				if (case_matched_type.is_empty())
				{
					// this reports the match errors
					[[maybe_unused]] auto const good = generic_type_match(change_expr(match_context, expr));
					bz_assert(!good);
					any_failed_matches = true;
					return true;
				}
			}
			return is_equal;
		}
	};

	for (auto &[_, expr] : switch_expr.cases)
	{
		if (expr.is_noreturn())
		{
			continue;
		}
		auto const good = check_expr_matched_type(expr);
		if (!good)
		{
			return match_function_result_t<kind>();
		}
	}
	if (is_default_valid)
	{
		auto const good = check_expr_matched_type(switch_expr.default_case);
		if (!good)
		{
			return match_function_result_t<kind>();
		}
	}

	if constexpr (kind == type_match_function_kind::can_match)
	{
		return true;
	}
	else if constexpr (kind == type_match_function_kind::match_level)
	{
		auto result = match_level_t();
		auto &result_vec = result.emplace_multi();
		result_vec.reserve(valid_case_count + static_cast<size_t>(is_default_valid));

		auto const new_context = change_dest(match_context, matched_type);
		for (auto const &[_, expr] : switch_expr.cases)
		{
			if (expr.is_noreturn())
			{
				continue;
			}

			result_vec.push_back(generic_type_match(change_expr(new_context, expr)));
		}
		if (is_default_valid)
		{
			result_vec.push_back(generic_type_match(change_expr(new_context, switch_expr.default_case)));
		}
		bz_assert(result_vec.is_all([](auto const &branch) { return branch.not_null(); }));
		return result;
	}
	else if constexpr (kind == type_match_function_kind::matched_type)
	{
		return matched_type;
	}
	else if constexpr (kind == type_match_function_kind::match_expression)
	{
		if (any_failed_matches)
		{
			return false;
		}

		bz_assert(matched_type.not_empty());
		auto const new_context = change_dest_and_dest_container(match_context, std::move(matched_type));
		bool all_good = true;
		for (auto &[_, expr] : switch_expr.cases)
		{
			if (expr.is_noreturn())
			{
				continue;
			}

			all_good &= generic_type_match(change_expr(new_context, expr));
		}
		if (is_default_valid)
		{
			all_good &= generic_type_match(change_expr(new_context, switch_expr.default_case));
		}
		return all_good;
	}
	else
	{
		static_assert(bz::meta::always_false<match_context_t<kind>>);
	}
}

template<type_match_function_kind kind>
static match_function_result_t<kind> generic_type_match_typename_strict_match(
	lex::src_tokens const &src_tokens,
	ast::typespec_view source,
	ast::typespec_view dest,
	ast::typespec_view original_source,
	ast::typespec_view original_dest,
	ctx::parse_context &context
)
{
	static_assert(kind != type_match_function_kind::matched_type);

	uint16_t modifier_match_level = 0;
	while (source.is_safe_blind_get() && source.modifier_kind() == dest.modifier_kind())
	{
		source = source.blind_get();
		dest = dest.blind_get();
		modifier_match_level += 1;
	}

	if (dest.is<ast::ts_typename>())
	{
		if constexpr (kind == type_match_function_kind::match_level)
		{
			return single_match_t{
				modifier_match_level,
				reference_match_kind::reference_exact,
				type_match_kind::direct_match,
			};
		}
		else
		{
			return true;
		}
	}
	else if (!dest.same_kind_as(source))
	{
		if constexpr (match_context_t<kind>::report_errors)
		{
			context.report_error(
				src_tokens,
				bz::format("unable to match type '{}' to typename type '{}'", original_source, original_dest)
			);
		}
		return match_function_result_t<kind>();
	}
	else if (dest.is<ast::ts_array>())
	{
		auto const dest_size = dest.get<ast::ts_array>().size;
		auto const source_size = source.get<ast::ts_array>().size;
		if (dest_size == 0)
		{
			modifier_match_level += 1;
		}
		else if (dest_size != source_size)
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				context.report_error(
					src_tokens,
					bz::format("unable to match type '{}' to typename type '{}'", original_source, original_dest),
					{
						context.make_note(
							src_tokens,
							bz::format("mismatched array sizes: {} and {}", source_size, dest_size)
						)
					}
				);
			}
			return match_function_result_t<kind>();
		}
		else
		{
			modifier_match_level += 2;
		}

		if constexpr (kind == type_match_function_kind::match_level)
		{
			return generic_type_match_typename_strict_match<kind>(
				src_tokens,
				source.get<ast::ts_array>().elem_type,
				dest.get<ast::ts_array>().elem_type,
				original_source,
				original_dest,
				context
			) + modifier_match_level;
		}
		else
		{
			return generic_type_match_typename_strict_match<kind>(
				src_tokens,
				source.get<ast::ts_array>().elem_type,
				dest.get<ast::ts_array>().elem_type,
				original_source,
				original_dest,
				context
			);
		}
	}
	else if (dest.is<ast::ts_array_slice>())
	{
		modifier_match_level += 1;
		if constexpr (kind == type_match_function_kind::match_level)
		{
			return generic_type_match_typename_strict_match<kind>(
				src_tokens,
				source.get<ast::ts_array_slice>().elem_type,
				dest.get<ast::ts_array_slice>().elem_type,
				original_source,
				original_dest,
				context
			) + modifier_match_level;
		}
		else
		{
			return generic_type_match_typename_strict_match<kind>(
				src_tokens,
				source.get<ast::ts_array_slice>().elem_type,
				dest.get<ast::ts_array_slice>().elem_type,
				original_source,
				original_dest,
				context
			);
		}
	}
	else if (dest.is<ast::ts_tuple>())
	{
		auto const &dest_types = dest.get<ast::ts_tuple>().types;
		auto const &source_types = source.get<ast::ts_tuple>().types;

		auto const is_variadic = dest_types.not_empty() && dest_types.back().is<ast::ts_variadic>();
		if (
			(is_variadic && source_types.size() < dest_types.size() - 1)
			|| (!is_variadic && dest_types.size() != source_types.size())
		)
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				context.report_error(
					src_tokens,
					bz::format("unable to match type '{}' to typename type '{}'", original_source, original_dest),
					{
						context.make_note(
							src_tokens,
							bz::format("mismatched tuple element counts: {} and {}", source_types.size(), dest_types.size())
						)
					}
				);
			}
			return match_function_result_t<kind>();
		}

		auto const non_variadic_count = dest_types.size() - static_cast<size_t>(is_variadic);

		if constexpr (kind == type_match_function_kind::match_level)
		{
			auto result = match_level_t();
			auto &result_vec = result.emplace_multi();
			result_vec.reserve(source_types.size());

			for (auto const i : bz::iota(0, non_variadic_count))
			{
				result_vec.push_back(generic_type_match_typename_strict_match<kind>(
					src_tokens,
					source_types[i],
					dest_types[i],
					original_source,
					original_dest,
					context
				));
				if (result_vec.back().is_null())
				{
					result.clear();
					return result;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, source_types.size()))
			{
				result_vec.push_back(generic_type_match_typename_strict_match<kind>(
					src_tokens,
					source_types[i],
					dest_types.back().get<ast::ts_variadic>(),
					original_source,
					original_dest,
					context
				));
				if (result_vec.back().is_null())
				{
					result.clear();
					return result;
				}
			}

			result += modifier_match_level;
			return result;
		}
		else
		{
			for (auto const i : bz::iota(0, non_variadic_count))
			{
				auto const good = generic_type_match_typename_strict_match<kind>(
					src_tokens,
					source_types[i],
					dest_types[i],
					original_source,
					original_dest,
					context
				);
				if (!good)
				{
					return false;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, source_types.size()))
			{
				auto const good = generic_type_match_typename_strict_match<kind>(
					src_tokens,
					source_types[i],
					dest_types.back().get<ast::ts_variadic>(),
					original_source,
					original_dest,
					context
				);
				if (!good)
				{
					return false;
				}
			}
			return true;
		}
	}
	else if (dest.is<ast::ts_base_type>())
	{
		if (source != dest)
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				context.report_error(
					src_tokens,
					bz::format("unable to match type '{}' to typename type '{}'", original_source, original_dest),
					{
						context.make_note(
							src_tokens,
							bz::format("mismatched types: '{}' and '{}'", source, dest)
						)
					}
				);
			}
			return match_function_result_t<kind>();
		}

		if constexpr (kind == type_match_function_kind::match_level)
		{
			return single_match_t{
				modifier_match_level,
				reference_match_kind::reference_exact,
				type_match_kind::direct_match,
			};
		}
		else
		{
			return true;
		}
	}
	else
	{
		if constexpr (match_context_t<kind>::report_errors)
		{
			context.report_error(
				src_tokens,
				bz::format("unable to match type '{}' to typename type '{}'", original_source, original_dest)
			);
		}
		return match_function_result_t<kind>();
	}
}

template<type_match_function_kind kind>
static match_function_result_t<kind> generic_type_match_typename(match_context_t<kind> const &match_context)
{
	bz_assert(match_context.expr.is_typename());
	ast::typespec_view source = match_context.expr.get_typename();
	ast::typespec_view dest = match_context.dest;

	if (!dest.is_typename())
	{
		if constexpr (match_context_t<kind>::report_errors)
		{
			match_context.context.report_error(
				match_context.expr.src_tokens,
				bz::format("unable to match type '{}' to non-typename type '{}'", source, dest)
			);
		}
		return match_function_result_t<kind>();
	}
	else if (!ast::is_complete(source))
	{
		if constexpr (match_context_t<kind>::report_errors)
		{
			match_context.context.report_error(
				match_context.expr.src_tokens,
				bz::format("unable to match incomplete type '{}' to typename type '{}'", source, dest)
			);
		}
		return match_function_result_t<kind>();
	}

	if constexpr (kind == type_match_function_kind::matched_type)
	{
		auto const can_match = generic_type_match_typename_strict_match<type_match_function_kind::can_match>(
			match_context.expr.src_tokens,
			source,
			dest,
			source,
			dest,
			match_context.context
		);
		if (can_match)
		{
			return dest;
		}
		else
		{
			return ast::typespec();
		}
	}
	else
	{
		return generic_type_match_typename_strict_match<kind>(
			match_context.expr.src_tokens,
			source,
			dest,
			source,
			dest,
			match_context.context
		);
	}
}

template<type_match_function_kind kind>
static match_function_result_t<kind> generic_type_match_tuple(match_context_t<kind> const &match_context)
{
	auto &&expr = match_context.expr;
	bz_assert(!expr.is_constant());
	auto &tuple_expr = expr.get_tuple();

	ast::typespec_view dest = match_context.dest;

	auto const original_dest = dest;
	if (dest.is<ast::ts_auto_reference>() || dest.is<ast::ts_auto_reference_mut>() || dest.is<ast::ts_move_reference>())
	{
		dest = dest.blind_get();
	}
	dest = dest.remove_any_mut();

	if (dest.is<ast::ts_tuple>())
	{
		auto const dest_types = dest.get<ast::ts_tuple>().types;

		auto const is_variadic = dest_types.not_empty() && dest_types.back().is<ast::ts_variadic>();
		if (
			(is_variadic && tuple_expr.elems.size() < dest_types.size() - 1)
			|| (!is_variadic && tuple_expr.elems.size() != dest_types.size())
		)
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				match_context.context.report_error(
					expr.src_tokens,
					bz::format("unable to match tuple expression to tuple type '{}'", original_dest),
					{
						match_context.context.make_note(
							expr.src_tokens,
							bz::format("mismatched tuple element counts: {} and {}", tuple_expr.elems.size(), dest_types.size())
						)
					}
				);
			}
			return match_function_result_t<kind>();
		}

		auto const non_variadic_count = dest_types.size() - static_cast<size_t>(is_variadic);

		if constexpr (kind == type_match_function_kind::can_match)
		{
			for (auto const i : bz::iota(0, non_variadic_count))
			{
				auto const good = generic_type_match(
					change_expr(change_dest(match_context, dest_types[i]), tuple_expr.elems[i])
				);
				if (!good)
				{
					return false;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, tuple_expr.elems.size()))
			{
				auto const good = generic_type_match(
					change_expr(change_dest(match_context, dest_types.back().get<ast::ts_variadic>()), tuple_expr.elems[i])
				);
				if (!good)
				{
					return false;
				}
			}

			return true;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			auto result = match_level_t();
			auto &result_vec = result.emplace_multi();
			result_vec.reserve(tuple_expr.elems.size());

			for (auto const i : bz::iota(0, non_variadic_count))
			{
				result_vec.push_back(generic_type_match(
					change_expr(change_dest(match_context, dest_types[i]), tuple_expr.elems[i])
				));
				if (result_vec.back().is_null())
				{
					result.clear();
					return result;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, tuple_expr.elems.size()))
			{
				result_vec.push_back(generic_type_match(
					change_expr(change_dest(match_context, dest_types.back().get<ast::ts_variadic>()), tuple_expr.elems[i])
				));
				if (result_vec.back().is_null())
				{
					result.clear();
					return result;
				}
			}

			result += 3; // 3, because otherwise array types would have priority
			return result;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = original_dest.remove_any_reference();
			bz_assert(result.terminator->is<ast::ts_tuple>());
			auto &result_vec = result.terminator->get<ast::ts_tuple>().types;
			result_vec.clear();
			result_vec.reserve(tuple_expr.elems.size());

			for (auto const i : bz::iota(0, non_variadic_count))
			{
				result_vec.push_back(generic_type_match(
					change_expr(change_dest(match_context, dest_types[i]), tuple_expr.elems[i])
				));
				if (result_vec.back().is_empty())
				{
					result.clear();
					return result;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, tuple_expr.elems.size()))
			{
				result_vec.push_back(generic_type_match(
					change_expr(change_dest(match_context, dest_types.back().get<ast::ts_variadic>()), tuple_expr.elems[i])
				));
				if (result_vec.back().is_empty())
				{
					result.clear();
					return result;
				}
			}

			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			bz_assert(match_context.dest_container.terminator->template is<ast::ts_tuple>());
			auto &dest_types_container = match_context.dest_container.terminator->template get<ast::ts_tuple>().types;
			// save dest in case we have an error
			auto dest_types_copy = dest_types_container;
			if (is_variadic)
			{
				expand_variadic_tuple_type(dest_types_container, tuple_expr.elems.size());
			}
			bz_assert(dest_types_container.size() == tuple_expr.elems.size());

			bool good = true;
			for (auto const i : bz::iota(0, dest_types_container.size()))
			{
				good &= generic_type_match(match_context_t<kind>{
					.expr = tuple_expr.elems[i],
					.dest_container = dest_types_container[i],
					.dest = dest_types_container[i],
					.context = match_context.context,
				});
			}

			if (!good)
			{
				// restore original dest
				dest_types_container = std::move(dest_types_copy);
			}
			return good;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (dest.is<ast::ts_array>())
	{
		auto const &dest_array_t = dest.get<ast::ts_array>();
		if (dest_array_t.size != 0 && dest_array_t.size != tuple_expr.elems.size())
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				match_context.context.report_error(
					expr.src_tokens,
					bz::format("unable to match tuple expression to array type '{}'", original_dest),
					{
						match_context.context.make_note(
							expr.src_tokens,
							bz::format("mismatched element counts: {} and {}", tuple_expr.elems.size(), dest_array_t.size)
						)
					}
				);
			}
			return match_function_result_t<kind>();
		}
		else if (tuple_expr.elems.empty())
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				match_context.context.report_error(
					expr.src_tokens,
					bz::format("unable to match empty tuple expression to array type '{}'", original_dest)
				);
			}
			return match_function_result_t<kind>();
		}

		if constexpr (kind == type_match_function_kind::can_match)
		{
			if (ast::is_complete(dest_array_t.elem_type))
			{
				auto const new_match_context = change_dest(match_context, dest_array_t.elem_type);
				return tuple_expr.elems.is_all([&](auto const &elem) {
					return generic_type_match(change_expr(new_match_context, elem));
				});
			}
			else
			{
				auto const matched_elem_type = generic_type_match(match_context_t<type_match_function_kind::matched_type>{
					.expr = tuple_expr.elems[0],
					.dest = dest_array_t.elem_type,
					.context = match_context.context,
				});
				if (matched_elem_type.is_empty())
				{
					return false;
				}

				auto const new_match_context = change_dest(match_context, matched_elem_type);
				return tuple_expr.elems.slice(1).is_all([&](auto const &elem) {
					return generic_type_match(change_expr(new_match_context, elem));
				});
			}
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			auto result = match_level_t();
			auto &result_vec = result.emplace_multi();
			result_vec.reserve(tuple_expr.elems.size());

			if (ast::is_complete(dest_array_t.elem_type))
			{
				auto const new_match_context = change_dest(match_context, dest_array_t.elem_type);
				for (auto const &elem : tuple_expr.elems)
				{
					result_vec.push_back(generic_type_match(change_expr(new_match_context, elem)));
					if (result_vec.back().is_null())
					{
						result.clear();
						return result;
					}
				}
			}
			else
			{
				auto const matched_elem_type = generic_type_match(match_context_t<type_match_function_kind::matched_type>{
					.expr = tuple_expr.elems[0],
					.dest = dest_array_t.elem_type,
					.context = match_context.context,
				});
				if (matched_elem_type.is_empty())
				{
					result.clear();
					return result;
				}

				auto const new_match_context = change_dest(match_context, matched_elem_type);
				for (auto const &elem : tuple_expr.elems)
				{
					result_vec.push_back(generic_type_match(change_expr(new_match_context, elem)));
					if (result_vec.back().is_null())
					{
						result.clear();
						return result;
					}
				}
			}

			if (dest_array_t.size == 0)
			{
				result += 1;
			}
			else
			{
				result += 2;
			}

			return result;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = original_dest.remove_any_reference();
			auto &result_array_t = result.terminator->get<ast::ts_array>();

			if (ast::is_complete(result_array_t.elem_type))
			{
				auto const can_all_match = tuple_expr.elems
					.is_all([&](auto const &elem) {
						return generic_type_match(match_context_t<type_match_function_kind::can_match>{
							.expr = elem,
							.dest = result_array_t.elem_type,
							.context = match_context.context,
						});
					});
				if (!can_all_match)
				{
					result.clear();
				}
				else
				{
					result_array_t.size = tuple_expr.elems.size();
				}

				return result;
			}
			else
			{
				result_array_t.elem_type = generic_type_match(change_dest(match_context, result_array_t.elem_type));
				if (result_array_t.elem_type.is_empty())
				{
					result.clear();
					return result;
				}

				auto const can_all_match = tuple_expr.elems.slice(1)
					.is_all([&](auto const &elem) {
						return generic_type_match(match_context_t<type_match_function_kind::can_match>{
							.expr = elem,
							.dest = result_array_t.elem_type,
							.context = match_context.context,
						});
					});
				if (!can_all_match)
				{
					result.clear();
				}
				else
				{
					result_array_t.size = tuple_expr.elems.size();
				}

				return result;
			}
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			bz::optional<ast::typespec> generic_matched_elem_type;
			auto &dest_array_t = match_context.dest_container.terminator->template get<ast::ts_array>();
			if (!ast::is_complete(dest_array_t.elem_type))
			{
				generic_matched_elem_type = generic_type_match(match_context_t<type_match_function_kind::matched_type>{
					.expr = tuple_expr.elems[0],
					.dest = dest_array_t.elem_type,
					.context = match_context.context,
				});
			}

			auto &matched_elem_type = generic_matched_elem_type.has_value()
				? generic_matched_elem_type.get()
				: dest_array_t.elem_type;

			if (matched_elem_type.is_empty())
			{
				// try to match the first element in order to provide a meaningful error
				auto &dest_array_t = match_context.dest_container.terminator->template get<ast::ts_array>();
				[[maybe_unused]] auto const first_elem_good = generic_type_match(match_context_t<type_match_function_kind::match_expression>{
					.expr = tuple_expr.elems[0],
					.dest_container = dest_array_t.elem_type,
					.dest = dest_array_t.elem_type,
					.context = match_context.context,
				});
				bz_assert(!first_elem_good);
				return false;
			}

			bool good = true;
			for (auto &elem : tuple_expr.elems)
			{
				good &= generic_type_match(match_context_t<type_match_function_kind::match_expression>{
					.expr = elem,
					.dest_container = matched_elem_type,
					.dest = matched_elem_type,
					.context = match_context.context,
				});
			}

			if (!good)
			{
				return false;
			}

			if (generic_matched_elem_type.has_value())
			{
				dest_array_t.elem_type = std::move(generic_matched_elem_type.get());
				generic_matched_elem_type.clear();
			}
			dest_array_t.size = tuple_expr.elems.size();

			ast::typespec_view array_dest = match_context.dest_container.remove_any_mut_reference();
			bz_assert(array_dest.is<ast::ts_array>());
			expr = ast::make_dynamic_expression(
				expr.src_tokens,
				ast::expression_type_kind::rvalue, array_dest,
				ast::make_expr_aggregate_init(array_dest, std::move(tuple_expr.elems)),
				ast::destruct_operation()
			);
			return true;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (dest.is<ast::ts_auto>())
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return tuple_expr.elems.is_all([&](auto const &elem) {
				return generic_type_match(change_expr(match_context, elem));
			});
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			auto result = match_level_t();
			auto &result_vec = result.emplace_multi();
			result_vec.reserve(tuple_expr.elems.size());

			for (auto const &elem : tuple_expr.elems)
			{
				result_vec.push_back(generic_type_match(change_expr(match_context, elem)));
				if (result_vec.back().is_null())
				{
					result.clear();
					return result;
				}
			}

			return result;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = original_dest.remove_any_reference();
			auto &result_vec = result.terminator->emplace<ast::ts_tuple>().types;
			result_vec.reserve(tuple_expr.elems.size());

			auto const new_match_context = change_dest(match_context, dest);
			for (auto const &elem : tuple_expr.elems)
			{
				result_vec.push_back(generic_type_match(change_expr(new_match_context, elem)));
				if (result_vec.back().is_empty())
				{
					result.clear();
					return result;
				}
			}

			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto const auto_pos = dest.src_tokens.pivot;
			auto &dest_types = match_context.dest_container.terminator->template emplace<ast::ts_tuple>().types;
			dest_types.reserve(tuple_expr.elems.size());

			bool good = true;
			for (auto &elem : tuple_expr.elems)
			{
				dest_types.push_back(ast::make_auto_typespec(auto_pos));
				good &= generic_type_match(match_context_t<type_match_function_kind::match_expression>{
					.expr = elem,
					.dest_container = dest_types.back(),
					.dest = dest_types.back(),
					.context = match_context.context,
				});
			}

			return good;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else
	{
		if constexpr (match_context_t<kind>::report_errors)
		{
			match_context.context.report_error(
				expr.src_tokens,
				bz::format("unable to match tuple expression to type '{}'", original_dest)
			);
		}
		return match_function_result_t<kind>();
	}
}

template<type_match_function_kind kind>
static match_function_result_t<kind> generic_type_match_strict_match(
	strict_match_context_t<kind> const &match_context,
	bool accept_void,
	bool propagate_const,
	bool top_level
)
{
	ast::typespec_view source = match_context.source;
	ast::typespec_view dest = match_context.dest;
	uint16_t modifier_match_level = 0;
	type_match_kind match_kind = type_match_kind::none;
	if constexpr (kind == type_match_function_kind::match_level)
	{
		match_kind = match_context.base_type_match;
	}
	while (true)
	{
		if (dest.is<ast::ts_auto_reference>() || dest.is<ast::ts_auto_reference_mut>())
		{
			break;
		}

		// remove muts and optionals from pointers if there are any
		{
			auto const dest_is_mut = dest.is<ast::ts_mut>();
			auto const source_is_mut = source.is<ast::ts_mut>();

			if (
				(dest_is_mut && !source_is_mut)
				|| (!propagate_const && !dest_is_mut && source_is_mut)
			)
			{
				if constexpr (match_context_t<kind>::report_errors)
				{
					if (&match_context.original_dest_container != &match_context.dest_container)
					{
						match_context.context.report_error(
							match_context.expr.src_tokens,
							bz::format("unable to match type '{}' to '{}'", match_context.source, match_context.dest),
							{
								match_context.context.make_note(
									match_context.expr.src_tokens,
									bz::format("mismatched mutability of types '{}' and '{}'", source, dest)
								),
								match_context.context.make_note(
									match_context.expr.src_tokens,
									bz::format(
										"while matching expression of type '{}' to '{}'",
										match_context.expr.get_expr_type(), match_context.original_dest_container
									)
								)
							}
						);
					}
					else
					{
						match_context.context.report_error(
							match_context.expr.src_tokens,
							bz::format(
								"unable to match value of type '{}' to '{}'",
								match_context.expr.get_expr_type(), match_context.dest_container
							),
							{
								match_context.context.make_note(
									match_context.expr.src_tokens,
									bz::format("mismatched mutability of types '{}' and '{}'", source, dest)
								),
							}
						);
					}
				}
				return match_function_result_t<kind>();
			}

			if (top_level)
			{
				top_level = false;
			}
			else
			{
				propagate_const &= !dest_is_mut;
				modifier_match_level += static_cast<uint16_t>(dest_is_mut == source_is_mut);
			}

			if (dest_is_mut)
			{
				dest = dest.blind_get();
			}
			if (source_is_mut)
			{
				source = source.blind_get();
			}

			if (
				propagate_const
				&& (
					(dest.is_optional_pointer() && source.is<ast::ts_pointer>())
					|| (dest.is_optional_reference() && source.is<ast::ts_lvalue_reference>())
					|| (dest.is_optional_function() && source.is<ast::ts_function>())
				)
			)
			{
				dest = dest.blind_get();
				modifier_match_level += 1;
				if constexpr (kind == type_match_function_kind::match_level)
				{
					match_kind = std::max(match_kind, type_match_kind::implicit_conversion);
				}
			}
		}

		if (dest.is<ast::ts_optional>() && source.is<ast::ts_optional>())
		{
			dest = dest.blind_get();
			source = source.blind_get();
			modifier_match_level += 1;
		}
		if (dest.is<ast::ts_pointer>() && source.is<ast::ts_pointer>())
		{
			dest = dest.blind_get();
			source = source.blind_get();
			modifier_match_level += 1;
		}
		else if (dest.is<ast::ts_lvalue_reference>() && source.is<ast::ts_lvalue_reference>())
		{
			dest = dest.blind_get();
			source = source.blind_get();
			modifier_match_level += 1;
		}
		else
		{
			break;
		}
	}

	if (dest.is<ast::ts_auto_reference>())
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return generic_type_match_strict_match(
				change_inner_dest_and_source(
					match_context,
					dest.get<ast::ts_auto_reference>(),
					source.remove_reference()
				),
				!source.is<ast::ts_lvalue_reference>(),
				propagate_const && source.is<ast::ts_lvalue_reference>(),
				false
			);
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			if (source.is_reference())
			{
				modifier_match_level += 1;
			}
			auto result = generic_type_match_strict_match(
				change_inner_dest_and_source(
					match_context,
					dest.get<ast::ts_auto_reference>(),
					source.remove_reference()
				),
				!source.is<ast::ts_lvalue_reference>(),
				propagate_const && source.is<ast::ts_lvalue_reference>(),
				false
			);
			result += modifier_match_level;
			return result;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			auto const can_match = generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::can_match>{
					.source = source.remove_reference(),
					.dest = dest.get<ast::ts_auto_reference>(),
					.context = match_context.context,
				},
				!source.is<ast::ts_lvalue_reference>(),
				propagate_const && source.is<ast::ts_lvalue_reference>(),
				false
			);
			if (!can_match)
			{
				static_assert(!match_context_t<kind>::report_errors);
				return ast::typespec();
			}

			ast::typespec result = match_context.original_dest;
			auto const index = dest.modifiers.begin() - match_context.original_dest.modifiers.begin();
			auto const result_dest_view = ast::typespec_view{
				.src_tokens = dest.src_tokens,
				.modifiers = result.modifiers.slice(index),
				.terminator = result.terminator.get(),
			};
			result.copy_from(result_dest_view, source);
			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto const can_match = generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::can_match>{
					.source = source.remove_reference(),
					.dest = dest.get<ast::ts_auto_reference>(),
					.context = match_context.context,
				},
				!source.is<ast::ts_lvalue_reference>(),
				propagate_const && source.is<ast::ts_lvalue_reference>(),
				false
			);
			if (!can_match)
			{
				static_assert(match_context_t<kind>::report_errors);
				bz::vector<ctx::source_highlight> notes;
				if (&match_context.original_dest_container != &match_context.dest_container)
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"while matching expression of type '{}' to '{}'",
							match_context.expr.get_expr_type(), match_context.original_dest_container
						)
					));
				}
				match_context.context.report_error(
					match_context.expr.src_tokens,
					bz::format("unable to match type '{}' to '{}'", match_context.source, match_context.dest),
					std::move(notes)
				);
				return false;
			}

			match_context.dest_container.copy_from(dest, source);
			return true;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (dest.is<ast::ts_auto_reference_mut>())
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return generic_type_match_strict_match(
				change_inner_dest_and_source(
					match_context,
					dest.get<ast::ts_auto_reference_mut>(),
					source.remove_mut_reference()
				),
				!source.is<ast::ts_lvalue_reference>(),
				propagate_const
				&& source.is<ast::ts_lvalue_reference>()
				&& !source.get<ast::ts_lvalue_reference>().is<ast::ts_mut>(),
				source.is<ast::ts_lvalue_reference>()
				&& !source.get<ast::ts_lvalue_reference>().is<ast::ts_mut>()
			);
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			if (source.is_reference())
			{
				modifier_match_level += 1;
			}
			auto result = generic_type_match_strict_match(
				change_inner_dest_and_source(
					match_context,
					dest.get<ast::ts_auto_reference_mut>(),
					source.remove_mut_reference()
				),
				!source.is<ast::ts_lvalue_reference>(),
				propagate_const
				&& source.is<ast::ts_lvalue_reference>()
				&& !source.get<ast::ts_lvalue_reference>().is<ast::ts_mut>(),
				source.is<ast::ts_lvalue_reference>()
				&& !source.get<ast::ts_lvalue_reference>().is<ast::ts_mut>()
			);
			result += modifier_match_level;
			return result;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			auto const can_match = generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::can_match>{
					.source = source.remove_mut_reference(),
					.dest = dest.get<ast::ts_auto_reference_mut>(),
					.context = match_context.context,
				},
				!source.is<ast::ts_lvalue_reference>(),
				propagate_const && source.is<ast::ts_lvalue_reference>(),
				false
			);
			if (!can_match)
			{
				static_assert(!match_context_t<kind>::report_errors);
				return ast::typespec();
			}

			ast::typespec result = match_context.original_dest;
			auto const index = dest.modifiers.begin() - match_context.original_dest.modifiers.begin();
			auto const result_dest_view = ast::typespec_view{
				.src_tokens = dest.src_tokens,
				.modifiers = result.modifiers.slice(index),
				.terminator = result.terminator.get(),
			};
			result.copy_from(result_dest_view, source);
			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto const can_match = generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::can_match>{
					.source = source.remove_mut_reference(),
					.dest = dest.get<ast::ts_auto_reference_mut>(),
					.context = match_context.context,
				},
				!source.is<ast::ts_lvalue_reference>(),
				propagate_const && source.is<ast::ts_lvalue_reference>(),
				false
			);
			if (!can_match)
			{
				static_assert(match_context_t<kind>::report_errors);
				bz::vector<ctx::source_highlight> notes;
				if (&match_context.original_dest_container != &match_context.dest_container)
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"while matching expression of type '{}' to '{}'",
							match_context.expr.get_expr_type(), match_context.original_dest_container
						)
					));
				}
				match_context.context.report_error(
					match_context.expr.src_tokens,
					bz::format("unable to match type '{}' to '{}'", match_context.source, match_context.dest),
					std::move(notes)
				);
				return false;
			}

			match_context.dest_container.copy_from(dest, source);
			return true;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (source.not_empty() && dest.is<ast::ts_auto>())
	{
		bz_assert(!source.is<ast::ts_move_reference>());
		if (source.is<ast::ts_lvalue_reference>())
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				bz::vector<ctx::source_highlight> notes;
				if (source.modifiers.size() < match_context.source.modifiers.size() && dest.modifiers.size() < match_context.dest.modifiers.size())
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"while matching expression of type '{}' to '{}'",
							match_context.source, match_context.dest
						)
					));
				}
				if (&match_context.original_dest_container != &match_context.dest_container)
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"while matching expression of type '{}' to '{}'",
							match_context.expr.get_expr_type(), match_context.original_dest_container
						)
					));
				}
				match_context.context.report_error(
					match_context.expr.src_tokens,
					bz::format("unable to match lvalue reference type '{}' to '{}'", source, dest),
					std::move(notes)
				);
			}
			return match_function_result_t<kind>();
		}

		bz_assert(!source.is<ast::ts_mut>());
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return true;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			return single_match_t{
				.modifier_match_level = modifier_match_level,
				.reference_match = match_context.reference_match,
				.type_match = std::max(match_kind, type_match_kind::direct_match)
			};
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = match_context.original_dest;
			bz_assert(result.terminator->is<ast::ts_auto>());
			auto const result_auto_view = ast::typespec_view{ result.src_tokens, {}, result.terminator.get() };
			bz_assert(result_auto_view.is<ast::ts_auto>());
			result.copy_from(result_auto_view, source);
			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			match_context.dest_container.copy_from(dest, source);
			return true;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (dest == source)
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return true;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			return single_match_t{
				.modifier_match_level = modifier_match_level,
				.reference_match = match_context.reference_match,
				.type_match = std::max(match_kind, type_match_kind::exact_match)
			};
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			return match_context.original_dest;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			return true;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (dest.is<ast::ts_function>() && source.is<ast::ts_function>())
	{
		auto const &dest_fn = dest.get<ast::ts_function>();
		auto const &source_fn = source.get<ast::ts_function>();

		auto const is_variadic = dest_fn.param_types.not_empty() && dest_fn.param_types.back().is<ast::ts_variadic>();
		if (
			(is_variadic && source_fn.param_types.size() < dest_fn.param_types.size() - 1)
			|| (!is_variadic && source_fn.param_types.size() != dest_fn.param_types.size())
		)
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				bz::vector<ctx::source_highlight> notes;
				if (source.modifiers.size() != match_context.source.modifiers.size())
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"mismatched parameter counts {} and {} with function types '{}' and '{}'",
							source_fn.param_types.size(), dest_fn.param_types.size(), source, dest
						)
					));
				}
				else
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"mismatched parameter counts: {} and {}",
							source_fn.param_types.size(), dest_fn.param_types.size()
						)
					));
				}
				if (&match_context.original_dest_container != &match_context.dest_container)
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"while matching expression of type '{}' to '{}'",
							match_context.expr.get_expr_type(), match_context.original_dest_container
						)
					));
				}
				match_context.context.report_error(
					match_context.expr.src_tokens,
					bz::format(
						"unable to match type '{}' to '{}'",
						match_context.source, match_context.dest
					),
					std::move(notes)
				);
			}
			return match_function_result_t<kind>();
		}
		if (dest_fn.cc != source_fn.cc)
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				bz::vector<ctx::source_highlight> notes;
				if (source.modifiers.size() != match_context.source.modifiers.size())
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"mismatched calling conventions '{}' and '{}' with function types '{}' and '{}'",
							abi::to_string(source_fn.cc), abi::to_string(dest_fn.cc), source, dest
						)
					));
				}
				else
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"mismatched calling conventions: '{}' and '{}'",
							abi::to_string(source_fn.cc), abi::to_string(dest_fn.cc)
						)
					));
				}
				if (&match_context.original_dest_container != &match_context.dest_container)
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"while matching expression of type '{}' to '{}'",
							match_context.expr.get_expr_type(), match_context.original_dest_container
						)
					));
				}
				match_context.context.report_error(
					match_context.expr.src_tokens,
					bz::format(
						"unable to match type '{}' to '{}'",
						match_context.source, match_context.dest
					),
					std::move(notes)
				);
			}
			return match_function_result_t<kind>();
		}

		auto const non_variadic_count = dest_fn.param_types.size() - static_cast<size_t>(is_variadic);

		if constexpr (kind == type_match_function_kind::can_match)
		{
			for (auto const i : bz::iota(0, non_variadic_count))
			{
				auto const is_good = generic_type_match_strict_match(
					change_dest_and_source(
						match_context,
						dest_fn.param_types[i],
						source_fn.param_types[i]
					),
					false, false, true
				);
				if (!is_good)
				{
					return false;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, source_fn.param_types.size()))
			{
				auto const is_good = generic_type_match_strict_match(
					change_dest_and_source(
						match_context,
						dest_fn.param_types.back().get<ast::ts_variadic>(),
						source_fn.param_types[i]
					),
					false, false, true
				);
				if (!is_good)
				{
					return false;
				}
			}

			auto const does_return_type_match = generic_type_match_strict_match(
				change_dest_and_source(match_context, dest_fn.return_type, source_fn.return_type),
				true, false, true
			);
			return does_return_type_match;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			auto result = match_level_t();
			auto &result_vec = result.emplace_multi();
			result_vec.reserve(source_fn.param_types.size() + 1);

			for (auto const i : bz::iota(0, non_variadic_count))
			{
				result_vec.push_back(generic_type_match_strict_match(
					change_dest_and_source(
						match_context,
						dest_fn.param_types[i],
						source_fn.param_types[i]
					),
					false, false, true
				));
				if (result_vec.back().is_null())
				{
					result.clear();
					return result;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, source_fn.param_types.size()))
			{
				result_vec.push_back(generic_type_match_strict_match(
					change_dest_and_source(
						match_context,
						dest_fn.param_types.back().get<ast::ts_variadic>(),
						source_fn.param_types[i]
					),
					false, false, true
				));
				if (result_vec.back().is_null())
				{
					result.clear();
					return result;
				}
			}

			result_vec.push_back(generic_type_match_strict_match(
				change_dest_and_source(match_context, dest_fn.return_type, source_fn.return_type),
				true, false, true
			));
			if (result_vec.back().is_null())
			{
				result.clear();
				return result;
			}

			result += modifier_match_level;
			return result;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = match_context.original_dest;
			bz_assert(result.terminator->is<ast::ts_function>());
			auto &result_fn = result.terminator->get<ast::ts_function>();
			result_fn.param_types.clear();
			result_fn.param_types.reserve(source_fn.param_types.size());

			for (auto const i : bz::iota(0, non_variadic_count))
			{
				result_fn.param_types.push_back(generic_type_match_strict_match(
					change_dest_and_source(
						match_context,
						dest_fn.param_types[i],
						source_fn.param_types[i]
					),
					false, false, true
				));
				if (result_fn.param_types.back().is_empty())
				{
					result.clear();
					return result;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, source_fn.param_types.size()))
			{
				result_fn.param_types.push_back(generic_type_match_strict_match(
					change_dest_and_source(
						match_context,
						dest_fn.param_types.back().get<ast::ts_variadic>(),
						source_fn.param_types[i]
					),
					false, false, true
				));
				if (result_fn.param_types.back().is_empty())
				{
					result.clear();
					return result;
				}
			}

			result_fn.return_type = generic_type_match_strict_match(
				change_dest_and_source(match_context, dest_fn.return_type, source_fn.return_type),
				true, false, true
			);
			if (result_fn.return_type.is_empty())
			{
				result.clear();
				return result;
			}

			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			bz_assert(match_context.dest_container.terminator->template is<ast::ts_function>());
			ast::ts_function &dest_container_fn = match_context.dest_container.terminator->template get<ast::ts_function>();
			if (is_variadic)
			{
				expand_variadic_tuple_type(dest_container_fn.param_types, source_fn.param_types.size());
			}
			bz_assert(dest_container_fn.param_types.size() == source_fn.param_types.size());

			bool good = true;
			for (auto const i : bz::iota(0, dest_container_fn.param_types.size()))
			{
				good &= generic_type_match_strict_match(
					change_dest_and_source(
						match_context,
						dest_container_fn.param_types[i],
						source_fn.param_types[i]
					),
					false, false, true
				);
			}

			good &= generic_type_match_strict_match(
				change_dest_and_source(match_context, dest_container_fn.return_type, source_fn.return_type),
				true, false, true
			);

			return good;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (propagate_const && dest.is_optional_pointer() && source.is<ast::ts_pointer>())
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return true;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			return single_match_t{
				.modifier_match_level = modifier_match_level,
				.reference_match = match_context.reference_match,
				.type_match = std::max(match_kind, type_match_kind::implicit_conversion)
			};
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = match_context.original_dest;
			bz_assert(result.terminator->is<ast::ts_auto>());
			auto const result_auto_view = ast::typespec_view{ result.src_tokens, {}, result.terminator.get() };
			bz_assert(result_auto_view.is<ast::ts_auto>());
			result.copy_from(result_auto_view, source);
			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			match_context.dest_container.copy_from(dest, source);
			return true;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (accept_void && dest.is<ast::ts_void>())
	{
		bz_assert(!source.is<ast::ts_mut>());
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return true;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			return single_match_t{
				.modifier_match_level = modifier_match_level,
				.reference_match = match_context.reference_match,
				.type_match = std::max(match_kind, type_match_kind::implicit_conversion)
			};
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			return match_context.original_dest;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto const src_tokens = match_context.expr.src_tokens;
			match_context.expr = match_context.context.make_cast_expression(
				src_tokens,
				std::move(match_context.expr),
				match_context.dest_container
			);
			return true;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (
		dest.is<ast::ts_base_type>() && dest.get<ast::ts_base_type>().info->is_generic()
		&& source.is<ast::ts_base_type>() && source.get<ast::ts_base_type>().info->is_generic_instantiation()
		&& source.get<ast::ts_base_type>().info->generic_parent == dest.get<ast::ts_base_type>().info
	)
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return true;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			return single_match_t{
				.modifier_match_level = modifier_match_level,
				.reference_match = match_context.reference_match,
				.type_match = std::max(match_kind, type_match_kind::generic_match)
			};
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = match_context.original_dest;
			bz_assert(result.terminator->is<ast::ts_base_type>());
			result.terminator->get<ast::ts_base_type>() = source.get<ast::ts_base_type>();
			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			bz_assert(match_context.dest_container.terminator->template is<ast::ts_base_type>());
			match_context.dest_container.terminator->template get<ast::ts_base_type>() = source.get<ast::ts_base_type>();
			return true;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (dest.is<ast::ts_tuple>() && source.is<ast::ts_tuple>())
	{
		auto const source_types = source.get<ast::ts_tuple>().types.as_array_view();
		auto const dest_types = dest.get<ast::ts_tuple>().types.as_array_view();

		auto const is_variadic = dest_types.not_empty() && dest_types.back().is<ast::ts_variadic>();
		if (
			(is_variadic && source_types.size() < dest_types.size() - 1)
			|| (!is_variadic && source_types.size() != dest_types.size())
		)
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				bz::vector<ctx::source_highlight> notes;
				if (dest.modifiers.size() != match_context.dest.modifiers.size())
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"mismatched tuple element counts {} and {} with types '{}' and '{}'",
							source_types.size(), dest_types.size(), source, dest
						)
					));
				}
				else
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format("mismatched tuple element counts: {} and {}", source_types.size(), dest_types.size())
					));
				}
				if (&match_context.original_dest_container != &match_context.dest_container)
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"while matching expression of type '{}' to '{}'",
							match_context.expr.get_expr_type(), match_context.original_dest_container
						)
					));
				}
				match_context.context.report_error(
					match_context.expr.src_tokens,
					bz::format("unable to match type '{}' to '{}'", match_context.source, match_context.dest),
					std::move(notes)
				);
			}
			return match_function_result_t<kind>();
		}

		auto const non_variadic_count = dest_types.size() - static_cast<size_t>(is_variadic);

		if constexpr (kind == type_match_function_kind::can_match)
		{
			for (auto const i : bz::iota(0, non_variadic_count))
			{
				auto const is_good = generic_type_match_strict_match(
					strict_match_context_t<type_match_function_kind::can_match>{
						.source = source_types[i],
						.dest = dest_types[i],
						.context = match_context.context,
					},
					false, propagate_const, false
				);
				if (!is_good)
				{
					return false;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, source_types.size()))
			{
				auto const is_good = generic_type_match_strict_match(
					strict_match_context_t<type_match_function_kind::can_match>{
						.source = source_types[i],
						.dest = dest_types.back().get<ast::ts_variadic>(),
						.context = match_context.context,
					},
					false, propagate_const, false
				);
				if (!is_good)
				{
					return false;
				}
			}

			return true;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			auto result = match_level_t();
			auto &result_vec = result.emplace_multi();
			result_vec.reserve(source_types.size());

			for (auto const i : bz::iota(0, non_variadic_count))
			{
				result_vec.push_back(generic_type_match_strict_match(
					strict_match_context_t<type_match_function_kind::match_level>{
						.source = source_types[i],
						.dest = dest_types[i],
						.reference_match = match_context.reference_match,
						.base_type_match = match_context.base_type_match,
						.context = match_context.context,
					},
					false, propagate_const, false
				));
				if (result_vec.back().is_null())
				{
					result.clear();
					return result;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, source_types.size()))
			{
				result_vec.push_back(generic_type_match_strict_match(
					strict_match_context_t<type_match_function_kind::match_level>{
						.source = source_types[i],
						.dest = dest_types.back().get<ast::ts_variadic>(),
						.reference_match = match_context.reference_match,
						.base_type_match = match_context.base_type_match,
						.context = match_context.context,
					},
					false, propagate_const, false
				));
				if (result_vec.back().is_null())
				{
					result.clear();
					return result;
				}
			}

			result += modifier_match_level;
			return result;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = match_context.original_dest;
			bz_assert(result.terminator->is<ast::ts_tuple>());
			auto &result_vec = result.terminator->get<ast::ts_tuple>().types;
			result_vec.clear();
			result_vec.reserve(source_types.size());

			for (auto const i : bz::iota(0, non_variadic_count))
			{
				result_vec.push_back(generic_type_match_strict_match(
					strict_match_context_t<type_match_function_kind::matched_type>{
						.source = source_types[i],
						.dest = dest_types[i],
						.original_dest = dest_types[i],
						.context = match_context.context,
					},
					false, propagate_const, false
				));
				if (result_vec.back().is_empty())
				{
					result.clear();
					return result;
				}
			}

			for (auto const i : bz::iota(non_variadic_count, source_types.size()))
			{
				result_vec.push_back(generic_type_match_strict_match(
					strict_match_context_t<type_match_function_kind::matched_type>{
						.source = source_types[i],
						.dest = dest_types.back().get<ast::ts_variadic>(),
						.original_dest = dest_types.back().get<ast::ts_variadic>(),
						.context = match_context.context,
					},
					false, propagate_const, false
				));
				if (result_vec.back().is_empty())
				{
					result.clear();
					return result;
				}
			}

			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			bz_assert(match_context.dest_container.terminator->template is<ast::ts_tuple>());
			auto &dest_types = match_context.dest_container.terminator->template get<ast::ts_tuple>().types;
			if (is_variadic)
			{
				expand_variadic_tuple_type(dest_types, source_types.size());
			}
			bz_assert(dest_types.size() == source_types.size());

			bool good = true;
			for (auto const i : bz::iota(0, dest_types.size()))
			{
				good &= generic_type_match_strict_match(
					strict_match_context_t<type_match_function_kind::match_expression>{
						.expr = match_context.expr,
						.original_dest_container = match_context.original_dest_container,
						.dest_container = dest_types[i],
						.source = source_types[i],
						.dest = dest_types[i],
						.context = match_context.context,
					},
					false, propagate_const, false
				);
			}

			return good;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (dest.is<ast::ts_array_slice>() && source.is<ast::ts_array_slice>())
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::can_match>{
					.source = source.get<ast::ts_array_slice>().elem_type,
					.dest = dest.get<ast::ts_array_slice>().elem_type,
					.context = match_context.context,
				},
				false, propagate_const, false
			);
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			modifier_match_level += 1;
			return generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::match_level>{
					.source = source.get<ast::ts_array_slice>().elem_type,
					.dest = dest.get<ast::ts_array_slice>().elem_type,
					.reference_match = match_context.reference_match,
					.base_type_match = match_context.base_type_match,
					.context = match_context.context,
				},
				false, propagate_const, false
			) + modifier_match_level;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = match_context.original_dest;
			bz_assert(result.terminator->is<ast::ts_array_slice>());
			auto &array_slice = result.terminator->get<ast::ts_array_slice>();
			array_slice.elem_type = generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::matched_type>{
					.source = source.get<ast::ts_array_slice>().elem_type,
					.dest = dest.get<ast::ts_array_slice>().elem_type,
					.original_dest = dest.get<ast::ts_array_slice>().elem_type,
					.context = match_context.context,
				},
				false, propagate_const, false
			);
			if (array_slice.elem_type.is_empty())
			{
				result.clear();
			}
			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			bz_assert(match_context.dest_container.terminator->template is<ast::ts_array_slice>());
			auto &dest_container = match_context.dest_container.terminator->template get<ast::ts_array_slice>().elem_type;
			return generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::match_expression>{
					.expr = match_context.expr,
					.original_dest_container = match_context.original_dest_container,
					.dest_container = dest_container,
					.source = source.get<ast::ts_array_slice>().elem_type,
					.dest = dest_container,
					.context = match_context.context,
				},
				false, propagate_const, false
			);
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (dest.is<ast::ts_array>() && source.is<ast::ts_array>())
	{
		auto const &dest_array_type = dest.get<ast::ts_array>();
		auto const &source_array_type = source.get<ast::ts_array>();

		if (dest_array_type.size == 0)
		{
			modifier_match_level += 1;
		}
		else if (dest_array_type.size != source_array_type.size)
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				bz::vector<ctx::source_highlight> notes;
				if (source.modifiers.size() != match_context.source.modifiers.size())
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"mismatched array sizes {} and {} with types '{}' and '{}'",
							source_array_type.size, dest_array_type.size, source, dest
						)
					));
				}
				else
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format("mismatched array sizes: {} and {}", source_array_type.size, dest_array_type.size)
					));
				}
				if (&match_context.original_dest_container != &match_context.dest_container)
				{
					notes.push_back(match_context.context.make_note(
						match_context.expr.src_tokens,
						bz::format(
							"while matching expression of type '{}' to '{}'",
							match_context.expr.get_expr_type(), match_context.original_dest_container
						)
					));
				}
				match_context.context.report_error(
					match_context.expr.src_tokens,
					bz::format(
						"unable to match type '{}' to '{}'",
						match_context.source, match_context.dest
					),
					std::move(notes)
				);
			}
			return match_function_result_t<kind>();
		}
		else
		{
			modifier_match_level += 2;
		}

		if constexpr (kind == type_match_function_kind::can_match)
		{
			return generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::can_match>{
					.source = source.get<ast::ts_array>().elem_type,
					.dest = dest.get<ast::ts_array>().elem_type,
					.context = match_context.context,
				},
				false, propagate_const, true
			);
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			return generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::match_level>{
					.source = source.get<ast::ts_array>().elem_type,
					.dest = dest.get<ast::ts_array>().elem_type,
					.reference_match = match_context.reference_match,
					.base_type_match = match_context.base_type_match,
					.context = match_context.context,
				},
				false, propagate_const, true
			) + modifier_match_level;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = match_context.original_dest;
			bz_assert(result.terminator->is<ast::ts_array>());
			auto &array = result.terminator->get<ast::ts_array>();
			array.size = source_array_type.size;
			array.elem_type = generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::matched_type>{
					.source = source.get<ast::ts_array>().elem_type,
					.dest = dest.get<ast::ts_array>().elem_type,
					.original_dest = dest.get<ast::ts_array>().elem_type,
					.context = match_context.context,
				},
				false, propagate_const, true
			);
			if (array.elem_type.is_empty())
			{
				result.clear();
			}
			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			bz_assert(match_context.dest_container.terminator->template is<ast::ts_array>());
			auto &dest_array = match_context.dest_container.terminator->template get<ast::ts_array>();
			auto const good = generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::match_expression>{
					.expr = match_context.expr,
					.original_dest_container = match_context.original_dest_container,
					.dest_container = dest_array.elem_type,
					.source = source.get<ast::ts_array>().elem_type,
					.dest = dest_array.elem_type,
					.context = match_context.context,
				},
				false, propagate_const, true
			);
			if (good)
			{
				dest_array.size = source_array_type.size;
			}
			return good;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else
	{
		static_assert(ast::typespec_types::size() == 19);
		if constexpr (match_context_t<kind>::report_errors)
		{
			bz::vector<ctx::source_highlight> notes;
			if (source.modifiers.size() != match_context.source.modifiers.size())
			{
				notes.push_back(match_context.context.make_note(
					match_context.expr.src_tokens,
					bz::format("mismatched types '{}' and '{}'", source, dest)
				));
			}
			if (&match_context.original_dest_container != &match_context.dest_container)
			{
				notes.push_back(match_context.context.make_note(
					match_context.expr.src_tokens,
					bz::format(
						"while matching expression of type '{}' to '{}'",
						match_context.expr.get_expr_type(), match_context.original_dest_container
					)
				));
			}
			match_context.context.report_error(
				match_context.expr.src_tokens,
				bz::format("unable to match type '{}' to '{}'", match_context.source, match_context.dest),
				std::move(notes)
			);
		}
		return match_function_result_t<kind>();
	}
}

template<type_match_function_kind kind>
static match_function_result_t<kind> generic_type_match_base_case(
	match_context_t<kind> const &match_context,
	bz::optional<reference_match_kind> parent_reference_kind = {}
)
{
	auto &&expr = match_context.expr;
	bz_assert(!expr.is_tuple() || !expr.is_constant());

	auto const [expr_type_temp, expr_type_kind] = expr.get_expr_type_and_kind();
	ast::typespec_view const expr_type = expr_type_temp;
	auto const bare_expr_type = expr_type.remove_mut_reference();

	auto const original_dest = match_context.dest;
	ast::typespec_view dest = original_dest.remove_any_mut();

	if (dest.is_reference())
	{
		bz_assert(!parent_reference_kind.has_value());
		if (expr_type_kind != ast::expression_type_kind::lvalue && !expr_type.is_reference())
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				match_context.context.report_error(
					expr.src_tokens,
					bz::format(
						"unable to match temporary value of type '{}' to a reference type '{}'",
						expr_type, original_dest
					)
				);
			}
			return match_function_result_t<kind>();
		}

		auto const inner_dest = dest.get_reference();
		auto const inner_source = expr_type.is_reference() ? expr_type.get_reference() : expr_type;

		auto const reference_kind = inner_dest.is<ast::ts_mut>() == inner_source.is<ast::ts_mut>()
			? reference_match_kind::reference_exact
			: reference_match_kind::reference_remove_mut;
		return generic_type_match_strict_match(
			make_strict_match_context(
				match_context,
				inner_source,
				inner_dest,
				original_dest,
				reference_kind,
				type_match_kind::exact_match
			),
			false, true, false
		);
	}
	else if (dest.is<ast::ts_move_reference>())
	{
		bz_assert(!expr_type.is<ast::ts_move_reference>());
		bz_assert(!parent_reference_kind.has_value());
		if (!ast::is_rvalue(expr_type_kind) || expr_type.is_reference())
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				if (expr_type.is_reference())
				{
					match_context.context.report_error(
						expr.src_tokens,
						bz::format(
							"unable to match value of type '{}' to a move reference '{}'",
							expr_type, original_dest
						)
					);
				}
				else
				{
					bz_assert(expr_type_kind == ast::expression_type_kind::lvalue);
					match_context.context.report_error(
						expr.src_tokens,
						bz::format(
							"unable to match lvalue expression of type '{}' to a move reference '{}'",
							expr_type, original_dest
						)
					);
				}
			}
			return match_function_result_t<kind>();
		}

		auto const inner_dest = dest.get<ast::ts_move_reference>();
		// we don't care about the mutability here, because it's either a temporary, or a moved lvalue, which cannot be used after
		bz_assert(bare_expr_type == expr_type);
		auto const reference_kind = reference_match_kind::reference_exact;

		// only add mut to expr_type if we have to
		if (inner_dest.is<ast::ts_mut>())
		{
			ast::typespec expr_type_copy = expr_type;
			expr_type_copy.add_layer<ast::ts_mut>();
			return generic_type_match_strict_match(
				make_strict_match_context(
					match_context,
					expr_type_copy,
					inner_dest,
					original_dest,
					reference_kind,
					type_match_kind::exact_match
				),
				false, true, false
			);
		}
		else
		{
			return generic_type_match_strict_match(
				make_strict_match_context(
					match_context,
					expr_type,
					inner_dest,
					original_dest,
					reference_kind,
					type_match_kind::exact_match
				),
				false, true, false
			);
		}
	}
	else if (dest.is<ast::ts_auto_reference>())
	{
		bz_assert(!parent_reference_kind.has_value());
		auto const is_reference_result = expr_type_kind == ast::expression_type_kind::lvalue || expr_type.is_reference();

		auto inner_dest = dest.get<ast::ts_auto_reference>();
		auto const inner_source = expr_type.remove_reference();

		auto const reference_kind = inner_dest.is<ast::ts_mut>() == inner_source.is<ast::ts_mut>()
			? reference_match_kind::reference_exact
			: reference_match_kind::reference_remove_mut;

		if constexpr (kind == type_match_function_kind::matched_type)
		{
			if (is_reference_result)
			{
				ast::typespec result = generic_type_match_strict_match(
					make_strict_match_context(
						match_context,
						inner_source,
						inner_dest,
						inner_dest,
						reference_kind,
						type_match_kind::exact_match
					),
					false, true, false
				);
				if (!result.is_empty())
				{
					result.add_layer<ast::ts_lvalue_reference>();
				}
				return result;
			}
			else
			{
				return generic_type_match_base_case(change_dest(match_context, inner_dest));
			}
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			bz_assert(
				(
					match_context.dest_container.modifiers.not_empty()
					&& match_context.dest_container.modifiers[0].template is<ast::ts_auto_reference>()
				) || (
					match_context.dest_container.modifiers.size() >= 2
					&& match_context.dest_container.modifiers[0].template is<ast::ts_optional>()
					&& match_context.dest_container.modifiers[1].template is<ast::ts_auto_reference>()
				)
			);
			if (is_reference_result)
			{
				auto const good = generic_type_match_strict_match(
					make_strict_match_context(
						match_context,
						inner_source,
						inner_dest,
						original_dest,
						reference_kind,
						type_match_kind::exact_match
					),
					false, true, false
				);
				if (good)
				{
					if (match_context.dest_container.modifiers[0].template is<ast::ts_optional>())
					{
						match_context.dest_container.modifiers[1].template emplace<ast::ts_lvalue_reference>();
					}
					else
					{
						match_context.dest_container.modifiers[0].template emplace<ast::ts_lvalue_reference>();
					}
				}
				return good;
			}
			else
			{
				auto const good = generic_type_match_base_case(change_dest(match_context, inner_dest));
				if (good)
				{
					auto const is_optional_ref = match_context.dest_container.modifiers[0].template is<ast::ts_optional>();
					match_context.dest_container.remove_layer();
					if (is_optional_ref)
					{
						match_context.dest_container.modifiers[0].template emplace<ast::ts_optional>();
					}
				}
				return good;
			}
		}
		else
		{
			if (is_reference_result)
			{
				return generic_type_match_strict_match(
					make_strict_match_context(
						match_context,
						inner_source,
						inner_dest,
						original_dest,
						reference_kind,
						type_match_kind::exact_match
					),
					false, true, false
				);
			}
			else
			{
				return generic_type_match_base_case(
					change_dest(match_context, inner_dest),
					reference_kind // needed for match_level
				);
			}
		}
	}
	else if (dest.is<ast::ts_auto_reference_mut>())
	{
		bz_assert(!parent_reference_kind.has_value());
		auto const is_reference_result = expr_type_kind == ast::expression_type_kind::lvalue || expr_type.is_reference();
		auto inner_dest = dest.get<ast::ts_auto_reference_mut>();
		auto const inner_source = bare_expr_type;

		auto const expr_is_mut = expr_type.is_mut_reference() || expr_type.is<ast::ts_mut>();

		if constexpr (kind == type_match_function_kind::matched_type)
		{
			if (is_reference_result)
			{
				ast::typespec result = generic_type_match_strict_match(
					make_strict_match_context(
						match_context,
						inner_source,
						inner_dest,
						inner_dest,
						reference_match_kind::auto_reference_const,
						type_match_kind::exact_match
					),
					false, !expr_is_mut, true
				);
				if (!result.is_empty())
				{
					if (expr_is_mut)
					{
						result.add_layer<ast::ts_mut>();
					}
					result.add_layer<ast::ts_lvalue_reference>();
				}
				return result;
			}
			else
			{
				return generic_type_match_base_case(change_dest(match_context, inner_dest));
			}
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			bz_assert(
				(
					match_context.dest_container.modifiers.not_empty()
					&& match_context.dest_container.modifiers[0].template is<ast::ts_auto_reference_mut>()
				) || (
					match_context.dest_container.modifiers.size() >= 2
					&& match_context.dest_container.modifiers[0].template is<ast::ts_optional>()
					&& match_context.dest_container.modifiers[1].template is<ast::ts_auto_reference_mut>()
				)
			);
			if (is_reference_result)
			{
				auto const good = generic_type_match_strict_match(
					make_strict_match_context(
						match_context,
						inner_source,
						inner_dest,
						original_dest,
						reference_match_kind::auto_reference_const,
						type_match_kind::exact_match
					),
					false, !expr_is_mut, true
				);
				if (good)
				{
					if (expr_is_mut)
					{
						if (match_context.dest_container.modifiers[0].template is<ast::ts_optional>())
						{
							match_context.dest_container.modifiers[1].template emplace<ast::ts_mut>();
							match_context.dest_container.modifiers[0].template emplace<ast::ts_lvalue_reference>();
							match_context.dest_container.template add_layer<ast::ts_optional>();
						}
						else
						{
							match_context.dest_container.modifiers[0].template emplace<ast::ts_mut>();
							match_context.dest_container.template add_layer<ast::ts_lvalue_reference>();
						}
					}
					else
					{
						if (match_context.dest_container.modifiers[0].template is<ast::ts_optional>())
						{
							match_context.dest_container.modifiers[1].template emplace<ast::ts_lvalue_reference>();
						}
						else
						{
							match_context.dest_container.modifiers[0].template emplace<ast::ts_lvalue_reference>();
						}
					}
				}
				return good;
			}
			else
			{
				auto const good = generic_type_match_base_case(change_dest(match_context, inner_dest));
				if (good)
				{
					auto const is_optional_ref = match_context.dest_container.modifiers[0].template is<ast::ts_optional>();
					match_context.dest_container.remove_layer();
					if (is_optional_ref)
					{
						match_context.dest_container.modifiers[0].template emplace<ast::ts_optional>();
					}
				}
				return good;
			}
		}
		else
		{
			if (is_reference_result)
			{
				return generic_type_match_strict_match(
					make_strict_match_context(
						match_context,
						inner_source,
						inner_dest,
						original_dest,
						reference_match_kind::auto_reference_const,
						type_match_kind::exact_match
					),
					false, !expr_is_mut, true
				);
			}
			else
			{
				return generic_type_match_base_case(
					change_dest(match_context, inner_dest),
					reference_match_kind::auto_reference_const // needed for match_level
				);
			}
		}
	}
	else if (
		dest.is<ast::ts_auto>()
		|| (dest.is<ast::ts_base_type>() && dest.get<ast::ts_base_type>().info->is_generic())
		|| (
			dest.same_kind_as(bare_expr_type)
			&& (
				dest.is<ast::ts_pointer>()
				|| dest.is<ast::ts_optional>()
				|| dest.is<ast::ts_array_slice>()
				|| dest.is<ast::ts_array>()
				|| dest.is<ast::ts_tuple>()
				|| dest.is<ast::ts_function>()
			)
		)
		|| (bare_expr_type.is<ast::ts_pointer>() && dest.is_optional_pointer())
		|| (bare_expr_type.is<ast::ts_function>() && dest.is_optional_function())
	)
	{
		auto const accept_void = dest.is<ast::ts_pointer>() || dest.is_optional_pointer();
		auto const reference_kind = parent_reference_kind.has_value()
			? parent_reference_kind.get()
			: get_reference_match_kind_from_expr_kind(expr_type, expr_type_kind);
		return generic_type_match_strict_match(
			make_strict_match_context(
				match_context,
				bare_expr_type,
				dest,
				original_dest,
				reference_kind,
				type_match_kind::exact_match
			),
			accept_void, true, true
		);
	}
	else if (
		dest.is<ast::ts_optional>()
		&& !(
			bare_expr_type.is<ast::ts_base_type>()
			&& bare_expr_type.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_
		)
	)
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return generic_type_match_base_case(change_dest(match_context, dest.get<ast::ts_optional>()));
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			auto result = generic_type_match_base_case(change_dest(match_context, dest.get<ast::ts_optional>()), parent_reference_kind);
			if (result.not_null())
			{
				bz_assert(result.is_single());
				auto &single = result.get_single();
				single.type_match = std::max(single.type_match, type_match_kind::implicit_conversion);
			}
			return result;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			auto result = generic_type_match_base_case(change_dest(match_context, dest.get<ast::ts_optional>()));
			if (result.not_empty())
			{
				result.template add_layer<ast::ts_optional>();
			}
			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto const is_good = generic_type_match(change_dest(match_context, dest.get<ast::ts_optional>()));
			if (is_good && !match_context.dest_container.remove_any_mut_reference().is_optional_pointer())
			{
				expr = match_context.context.make_optional_cast_expression(std::move(expr));
			}
			return is_good;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (dest.is<ast::ts_array_slice>() && bare_expr_type.is<ast::ts_array>())
	{
		auto const reference_kind = parent_reference_kind.has_value()
			? parent_reference_kind.get()
			: get_reference_match_kind_from_expr_kind(expr_type, expr_type_kind);
		auto const dest_elem_t = dest.get<ast::ts_array_slice>().elem_type.as_typespec_view();
		auto const expr_elem_t = bare_expr_type.get<ast::ts_array>().elem_type.as_typespec_view();
		auto const is_mut_dest_elem_t = dest_elem_t.is<ast::ts_mut>();
		auto const is_mut_expr_elem_t = expr_type.is<ast::ts_mut>() || expr_type.is_mut_reference();

		if (!is_mut_expr_elem_t && is_mut_dest_elem_t)
		{
			if constexpr (match_context_t<kind>::report_errors)
			{
				match_context.context.report_error(
					expr.src_tokens,
					bz::format("unable to match expression of type '{}' to mutable array slice type '{}'", expr_type, original_dest)
				);
			}
			return match_function_result_t<kind>();
		}

		if constexpr (kind == type_match_function_kind::can_match)
		{
			return generic_type_match_strict_match(
				make_strict_match_context(
					match_context,
					expr_elem_t,
					dest_elem_t.remove_any_mut(),
					dest_elem_t.remove_any_mut(),
					reference_kind,
					type_match_kind::implicit_conversion
				),
				false, !is_mut_dest_elem_t, true
			);
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			auto result = generic_type_match_strict_match(
				make_strict_match_context(
					match_context,
					expr_elem_t,
					dest_elem_t.remove_any_mut(),
					dest_elem_t.remove_any_mut(),
					reference_kind,
					type_match_kind::implicit_conversion
				),
				false, !is_mut_dest_elem_t, true
			);

			if (is_mut_dest_elem_t == is_mut_expr_elem_t)
			{
				result += 1;
			}

			return result;
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			ast::typespec result = original_dest;
			bz_assert(result.terminator->is<ast::ts_array_slice>());
			auto &result_slice_t = result.terminator->get<ast::ts_array_slice>();
			result_slice_t.elem_type = generic_type_match_strict_match(
				make_strict_match_context(
					match_context,
					expr_elem_t,
					dest_elem_t.remove_any_mut(),
					dest_elem_t.remove_any_mut(),
					reference_kind,
					type_match_kind::implicit_conversion
				),
				false, !is_mut_dest_elem_t, true
			);
			if (result_slice_t.elem_type.is_empty())
			{
				result.clear();
			}
			else if (dest_elem_t.is<ast::ts_mut>())
			{
				result_slice_t.elem_type.add_layer<ast::ts_mut>();
			}
			return result;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			bz_assert(match_context.dest_container.terminator->template is<ast::ts_array_slice>());
			auto const good = generic_type_match_strict_match(
				strict_match_context_t<type_match_function_kind::match_expression>{
					.expr = match_context.expr,
					.original_dest_container = match_context.dest_container,
					.dest_container = match_context.dest_container.terminator->template get<ast::ts_array_slice>().elem_type,
					.source = expr_elem_t,
					.dest = dest_elem_t.remove_any_mut(),
					.context = match_context.context,
				},
				false, !is_mut_dest_elem_t, true
			);

			if (!good)
			{
				return false;
			}
			else
			{
				auto const src_tokens = expr.src_tokens;
				expr = match_context.context.make_cast_expression(src_tokens, std::move(expr), dest);
				return true;
			}
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (dest == bare_expr_type)
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return true;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			auto const reference_kind = parent_reference_kind.has_value()
				? parent_reference_kind.get()
				: get_reference_match_kind_from_expr_kind(expr_type, expr_type_kind);
			return single_match_t{
				.modifier_match_level = 0,
				.reference_match = reference_kind,
				.type_match = type_match_kind::exact_match
			};
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			return original_dest;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			return true;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else if (is_implicitly_convertible(dest, expr, match_context.context))
	{
		if constexpr (kind == type_match_function_kind::can_match)
		{
			return true;
		}
		else if constexpr (kind == type_match_function_kind::match_level)
		{
			auto const reference_kind = parent_reference_kind.has_value()
				? parent_reference_kind.get()
				: get_reference_match_kind_from_expr_kind(expr_type, expr_type_kind);
			return single_match_t{
				.modifier_match_level = 0,
				.reference_match = reference_kind,
				.type_match = expr.is_integer_literal() ? type_match_kind::implicit_literal_conversion : type_match_kind::implicit_conversion
			};
		}
		else if constexpr (kind == type_match_function_kind::matched_type)
		{
			return original_dest;
		}
		else if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto const src_tokens = expr.src_tokens;
			expr = match_context.context.make_cast_expression(src_tokens, std::move(expr), original_dest);
			return true;
		}
		else
		{
			static_assert(bz::meta::always_false<match_context_t<kind>>);
		}
	}
	else
	{
		static_assert(ast::typespec_types::size() == 19);
		if constexpr (match_context_t<kind>::report_errors)
		{
			if (expr.is_enum_literal())
			{
				match_context.context.report_error(
					expr.src_tokens,
					bz::format("unable to match enum literal '.{}' to type '{}'", expr.get_enum_literal().id->value, original_dest)
				);
			}
			else
			{
				match_context.context.report_error(
					expr.src_tokens,
					bz::format("unable to match expression of type '{}' to '{}'", expr_type, original_dest)
				);
			}
		}
		return match_function_result_t<kind>();
	}
}

template<type_match_function_kind kind>
match_function_result_t<kind> generic_type_match(match_context_t<kind> const &match_context)
{
	auto &&expr = match_context.expr;
	if (!expr.is_constant_or_dynamic())
	{
		return match_function_result_t<kind>();
	}
	else if (
		auto const compound_expr = expr.get_expr().template get_if<ast::expr_compound>();
		compound_expr != nullptr && compound_expr->final_expr.not_null()
	)
	{
		auto result = generic_type_match(change_expr(match_context, compound_expr->final_expr));
		if constexpr (kind == type_match_function_kind::match_expression)
		{
			if (result)
			{
				auto const [expr_type, expr_type_kind] = compound_expr->final_expr.get_expr_type_and_kind();
				expr.set_type(expr_type);
				expr.set_type_kind(expr_type_kind);
			}
		}
		return result;
	}
	else if (expr.is_if_expr())
	{
		if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto good = generic_type_match_if_expr(match_context);
			if (good)
			{
				expr.set_type(match_context.dest_container.remove_any_mut());
				expr.set_type_kind(ast::expression_type_kind::rvalue);
			}
			return good;
		}
		else
		{
			return generic_type_match_if_expr(match_context);
		}
	}
	else if (expr.is_switch_expr())
	{
		if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto const good = generic_type_match_switch_expr(match_context);
			if (good)
			{
				expr.set_type(match_context.dest_container.remove_any_mut());
				expr.set_type_kind(ast::expression_type_kind::rvalue);
			}
			return good;
		}
		else
		{
			return generic_type_match_switch_expr(match_context);
		}
	}
	else if (expr.is_typename())
	{
		return generic_type_match_typename(match_context);
	}
	else if (expr.is_tuple())
	{
		if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto const good = generic_type_match_tuple(match_context);
			if (!good)
			{
				return false;
			}

			if (
				match_context.dest_container.template is<ast::ts_auto_reference>()
				|| match_context.dest_container.template is<ast::ts_auto_reference_mut>()
			)
			{
				match_context.dest_container.remove_layer();
			}

			ast::typespec_view const dest = match_context.dest_container;
			expr.set_type(dest.remove_any_mut_reference());
			expr.set_type_kind(ast::expression_type_kind::rvalue);

			bz_assert(!dest.is<ast::ts_lvalue_reference>());
			if (dest.is<ast::ts_move_reference>())
			{
				match_context.context.add_self_destruction(expr);
				auto const src_tokens = expr.src_tokens;
				expr = ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue_reference,
					dest.get<ast::ts_move_reference>(),
					ast::make_expr_take_move_reference(std::move(expr)),
					ast::destruct_operation()
				);
			}

			return true;
		}
		else
		{
			return generic_type_match_tuple(match_context);
		}
	}
	else
	{
		if constexpr (kind == type_match_function_kind::match_expression)
		{
			auto const dest_is_optional = match_context.dest.is_optional();
			auto const good = generic_type_match_base_case(match_context);
			if (!good)
			{
				return false;
			}

			auto const [expr_type_temp, expr_type_kind] = expr.get_expr_type_and_kind();
			ast::typespec_view const dest = match_context.dest_container;

			auto const bare_dest = dest.remove_any_reference();
			auto const bare_dest_without_mut = bare_dest.remove_any_mut();

			if (
				(bare_dest_without_mut.is<ast::ts_pointer>() || bare_dest_without_mut.is_optional_pointer())
				&& bare_dest != expr_type_temp.remove_mut_reference()
			)
			{
				auto const is_dest_void = bare_dest.terminator_kind() == ast::terminator_typespec_node_t::index_of<ast::ts_void>;
				auto const is_expr_void = expr_type_temp.terminator_kind() == ast::terminator_typespec_node_t::index_of<ast::ts_void>;
				if (is_dest_void == is_expr_void)
				{
					if (expr_type_temp.is_reference() && !dest.is_any_reference())
					{
						expr = match_context.context.make_copy_construction(std::move(expr));
					}
					expr.set_type(bare_dest);
				}
				else
				{
					bz_assert(!expr_type_temp.is_empty());
					ast::typespec cast_dest_type = expr_type_temp;
					cast_dest_type.terminator->template emplace<ast::ts_void>();
					expr = match_context.context.make_cast_expression(expr.src_tokens, std::move(expr), std::move(cast_dest_type));
					bz_assert(!expr.get_expr_type().is_reference());
					expr.set_type(bare_dest);
				}
			}

			// expr.set_type(...) invalidates expr_type_temp
			auto const expr_type = expr.get_expr_type();

			if (dest.is_optional_reference() && dest == expr_type)
			{
				if (expr_type_kind == ast::expression_type_kind::lvalue || expr_type.is_reference())
				{
					expr = match_context.context.make_copy_construction(std::move(expr));
				}
				else if (
					expr_type_kind == ast::expression_type_kind::rvalue_reference
					|| expr_type_kind == ast::expression_type_kind::moved_lvalue
				)
				{
					expr = match_context.context.make_move_construction(std::move(expr));
				}
			}
			else if (
				(dest.is<ast::ts_lvalue_reference>() || (!dest_is_optional && dest.is_optional_reference()))
				&& !expr_type.is_reference()
			)
			{
				auto const src_tokens = expr.src_tokens;
				expr = ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, dest.remove_optional(),
					ast::make_expr_take_reference(std::move(expr)),
					ast::destruct_operation()
				);
			}
			else if (dest.is<ast::ts_move_reference>() && ast::is_rvalue_or_literal(expr_type_kind))
			{
				match_context.context.add_self_destruction(expr);
				auto const src_tokens = expr.src_tokens;
				expr = ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue_reference, dest,
					ast::make_expr_take_move_reference(std::move(expr)),
					ast::destruct_operation()
				);
			}
			else if (!dest.is_any_reference() && (!dest.is_optional_reference() || expr_type_kind == ast::expression_type_kind::lvalue))
			{
				if (expr_type_kind == ast::expression_type_kind::lvalue || expr_type.is_reference())
				{
					expr = match_context.context.make_copy_construction(std::move(expr));
				}
				else if (
					expr_type_kind == ast::expression_type_kind::rvalue_reference
					|| expr_type_kind == ast::expression_type_kind::moved_lvalue
				)
				{
					expr = match_context.context.make_move_construction(std::move(expr));
				}
			}

			return true;
		}
		else
		{
			return generic_type_match_base_case(match_context);
		}
	}
}

template match_function_result_t<type_match_function_kind::can_match> generic_type_match(
	match_context_t<type_match_function_kind::can_match> const &match_context
);

template match_function_result_t<type_match_function_kind::match_level> generic_type_match(
	match_context_t<type_match_function_kind::match_level> const &match_context
);

template match_function_result_t<type_match_function_kind::matched_type> generic_type_match(
	match_context_t<type_match_function_kind::matched_type> const &match_context
);

template match_function_result_t<type_match_function_kind::match_expression> generic_type_match(
	match_context_t<type_match_function_kind::match_expression> const &match_context
);

} // namespace resolve
