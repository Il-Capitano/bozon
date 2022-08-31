#include "attribute_resolver.h"
#include "ctx/global_context.h"
#include "expression_resolver.h"
#include "match_expression.h"
#include "consteval.h"
#include "parse/expression_parser.h"

namespace resolve
{

static void report_unknown_attribute(ast::attribute const &attribute, ctx::parse_context &context)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '@{}'", attribute.name->value)
	);
}

bool apply_funcs_t::operator () (ast::decl_function &func_decl, ast::attribute &attribute, ctx::parse_context &context) const
{
	if (this->apply_to_func != nullptr)
	{
		return this->apply_to_func(func_decl, attribute, context);
	}
	else if (this->apply_to_func_body != nullptr)
	{
		return this->apply_to_func_body(func_decl.body, attribute, context);
	}
	else
	{
		report_unknown_attribute(attribute, context);
		return false;
	}
}

bool apply_funcs_t::operator () (ast::decl_operator &op_decl, ast::attribute &attribute, ctx::parse_context &context) const
{
	if (this->apply_to_op != nullptr)
	{
		return this->apply_to_op(op_decl, attribute, context);
	}
	else if (this->apply_to_func_body != nullptr)
	{
		return this->apply_to_func_body(op_decl.body, attribute, context);
	}
	else
	{
		report_unknown_attribute(attribute, context);
		return false;
	}
}

bool apply_funcs_t::operator () (ast::decl_variable &var_decl, ast::attribute &attribute, ctx::parse_context &context) const
{
	if (this->apply_to_var != nullptr)
	{
		return this->apply_to_var(var_decl, attribute, context);
	}
	else
	{
		report_unknown_attribute(attribute, context);
		return false;
	}
}

static bool apply_builtin(
	ast::decl_function &func_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (!context.global_ctx.add_builtin_function(&func_decl))
	{
		context.report_error(func_decl.body.src_tokens, bz::format("invalid function for '@{}'", attribute.name->value));
		return false;
	}
	else
	{
		func_decl.body.flags |= ast::function_body::intrinsic;
		return true;
	}
}

static bool apply_builtin(
	ast::decl_operator &op_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (!context.global_ctx.add_builtin_operator(&op_decl))
	{
		context.report_error(op_decl.body.src_tokens, bz::format("invalid operator for '@{}'", attribute.name->value));
		return false;
	}
	else
	{
		op_decl.body.flags |= ast::function_body::intrinsic;
		op_decl.body.flags |= ast::function_body::builtin_operator;
		return true;
	}
}

static bool apply_builtin_assign(
	ast::decl_operator &op_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (op_decl.op->kind != lex::token::assign)
	{
		context.report_error(op_decl.body.src_tokens, bz::format("invalid operator for '@{}'", attribute.name->value));
		return false;
	}
	else if (apply_builtin(op_decl, attribute, context))
	{
		op_decl.body.flags |= ast::function_body::builtin_assign;
		return true;
	}
	else
	{
		return false;
	}
}

static bool apply_comptime_error_checking(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	auto const kind = attribute.args[0]
		.get_constant_value()
		.get<ast::constant_value::string>().as_string_view();

	if (!context.global_ctx.add_comptime_checking_function(kind, &func_body))
	{
		context.report_error(attribute.args[0], bz::format("invalid kind '{}' for '@{}'", kind, attribute.name->value));
		return false;
	}
	else
	{
		func_body.flags |= ast::function_body::no_comptime_checking;
		return true;
	}
}

static bool apply_comptime_error_checking(
	ast::decl_variable &var_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	auto const kind = attribute.args[0]
		.get_constant_value()
		.get<ast::constant_value::string>().as_string_view();

	if (!context.global_ctx.add_comptime_checking_variable(kind, &var_decl))
	{
		context.report_error(attribute.args[0], bz::format("invalid kind '{}' for '@{}'", kind, attribute.name->value));
		return false;
	}
	else
	{
		return true;
	}
}

static bool apply_no_comptime_checking(
	ast::function_body &func_body,
	ast::attribute &,
	ctx::parse_context &
)
{
	func_body.flags |= ast::function_body::no_comptime_checking;
	for (auto const &specialization : func_body.generic_specializations)
	{
		specialization->flags |= ast::function_body::no_comptime_checking;
	}
	return true;
}

static bool apply_no_runtime_emit(
	ast::decl_variable &var_decl,
	ast::attribute &,
	ctx::parse_context &
)
{
	var_decl.flags |= ast::decl_variable::no_runtime_emit;
	return true;
}

static bool apply_symbol_name(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (func_body.is_generic())
	{
		context.report_error(attribute.name, bz::format("'@{}' cannot be applied to generic functions", attribute.name->value));
		return false;
	}
	else
	{
		auto const symbol_name = attribute.args[0]
			.get_constant_value()
			.get<ast::constant_value::string>().as_string_view();

		func_body.symbol_name = symbol_name;
		func_body.flags |= ast::function_body::external_linkage;
		return true;
	}
}

static bool apply_symbol_name(
	ast::decl_variable &var_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (!var_decl.is_global())
	{
		context.report_error(attribute.name, bz::format("'@{}' cannot be applied to local variables", attribute.name->value));
		return false;
	}
	else
	{
		auto const symbol_name = attribute.args[0]
			.get_constant_value()
			.get<ast::constant_value::string>().as_string_view();

		var_decl.symbol_name = symbol_name;
		var_decl.flags |= ast::decl_variable::external_linkage;
		return true;
	}
}

static bool apply_maybe_unused(
	ast::decl_variable &var_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	var_decl.flags |= ast::decl_variable::maybe_unused;
	for (auto &decl : var_decl.tuple_decls)
	{
		apply_maybe_unused(decl, attribute, context);
	}
	return true;
}

bz::vector<attribute_info_t> make_attribute_infos(bz::array_view<ast::type_info> builtin_type_infos){
	constexpr size_t N = 7;
	bz::vector<attribute_info_t> result;
	result.reserve(N);

	auto const str_type = ast::make_base_type_typespec({}, &builtin_type_infos[ast::type_info::str_]);

	result.push_back({
		"__builtin",
		{},
		{ &apply_builtin, &apply_builtin, nullptr, nullptr }
	});
	result.push_back({
		"__builtin_assign",
		{},
		{ nullptr, &apply_builtin_assign, nullptr, nullptr }
	});
	result.push_back({
		"__comptime_error_checking",
		{ str_type },
		{ nullptr, nullptr, &apply_comptime_error_checking, &apply_comptime_error_checking }
	});
	result.push_back({
		"__no_comptime_checking",
		{},
		{ nullptr, nullptr, &apply_no_comptime_checking, nullptr }
	});
	result.push_back({
		"__no_runtime_emit",
		{},
		{ nullptr, nullptr, nullptr, &apply_no_runtime_emit }
	});

	result.push_back({
		"symbol_name",
		{ str_type },
		{ nullptr, nullptr, &apply_symbol_name, &apply_symbol_name }
	});
	result.push_back({
		"maybe_unused",
		{},
		{ nullptr, nullptr, nullptr, &apply_maybe_unused }
	});

	bz_assert(result.size() == N);
	return result;
}

static bool resolve_attribute(
	ast::attribute &attribute,
	attribute_info_t &attribute_info,
	ctx::parse_context &context
)
{
	if (attribute.args.size() != 0)
	{
		// attributes have already been resolved
		return true;
	}
	auto [stream, end] = attribute.arg_tokens;
	bool good = true;
	if (stream != end)
	{
		attribute.args = parse::parse_expression_comma_list(stream, end, context);
		if (stream != end)
		{
			context.report_error({ stream, stream, end });
		}

		if (attribute.args.size() != attribute_info.arg_types.size())
		{
			context.report_error(
				lex::src_tokens::from_range(attribute.arg_tokens),
				bz::format(
					"'@{}' expects {} arguments, but {} were provided",
					attribute_info.name, attribute_info.arg_types.size(), attribute.args.size()
				)
			);
			good = false;
		}
		else
		{
			for (auto const &[arg, arg_type] : bz::zip(attribute.args, attribute_info.arg_types))
			{
				resolve_expression(arg, context);
				match_expression_to_type(arg, arg_type, context);
				resolve::consteval_try(arg, context);
				if (arg.not_error() && !arg.is_constant())
				{
					good = false;
					context.report_error(
						arg,
						"attribute argument must be a constant expression",
						resolve::get_consteval_fail_notes(arg)
					);
				}
			}
		}
	}
	return good;
}

void resolve_attributes(
	ast::decl_function &func_decl,
	ctx::parse_context &context
)
{
	for (auto &attribute : func_decl.attributes)
	{
		auto const attribute_info = context.global_ctx.get_builtin_attribute(attribute.name->value);
		if (attribute_info == nullptr)
		{
			report_unknown_attribute(attribute, context);
		}
		else
		{
			auto const good = resolve_attribute(attribute, *attribute_info, context);
			if (good)
			{
				attribute_info->apply_funcs(func_decl, attribute, context);
			}
		}
	}
}

void resolve_attributes(
	ast::decl_operator &op_decl,
	ctx::parse_context &context
)
{
	for (auto &attribute : op_decl.attributes)
	{
		auto const attribute_info = context.global_ctx.get_builtin_attribute(attribute.name->value);
		if (attribute_info == nullptr)
		{
			report_unknown_attribute(attribute, context);
		}
		else
		{
			auto const good = resolve_attribute(attribute, *attribute_info, context);
			if (good)
			{
				attribute_info->apply_funcs(op_decl, attribute, context);
			}
		}
	}
}

void resolve_attributes(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	for (auto &attribute : var_decl.attributes)
	{
		auto const attribute_info = context.global_ctx.get_builtin_attribute(attribute.name->value);
		if (attribute_info == nullptr)
		{
			report_unknown_attribute(attribute, context);
		}
		else
		{
			auto const good = resolve_attribute(attribute, *attribute_info, context);
			if (good)
			{
				attribute_info->apply_funcs(var_decl, attribute, context);
			}
		}
	}
}

} // namespace resolve
