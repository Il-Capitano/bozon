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

bool apply_funcs_t::operator () (ast::decl_type_alias &alias_decl, ast::attribute &attribute, ctx::parse_context &context) const
{
	if (this->apply_to_type_alias != nullptr)
	{
		return this->apply_to_type_alias(alias_decl, attribute, context);
	}
	else
	{
		report_unknown_attribute(attribute, context);
		return false;
	}
}

bool apply_funcs_t::operator () (ast::type_info &info, ast::attribute &attribute, ctx::parse_context &context) const
{
	if (this->apply_to_type_info != nullptr)
	{
		return this->apply_to_type_info(info, attribute, context);
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

	func_decl.body.flags |= ast::function_body::intrinsic;
	return true;
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

	op_decl.body.flags |= ast::function_body::intrinsic;
	op_decl.body.flags |= ast::function_body::builtin_operator;
	return true;
}

static bool apply_builtin(
	ast::decl_type_alias &alias_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (!context.global_ctx.add_builtin_type_alias(&alias_decl))
	{
		context.report_error(alias_decl.src_tokens, bz::format("invalid type alias for '@{}'", attribute.name->value));
		return false;
	}
	else if (alias_decl.id.values.back() == "isize")
	{
		auto const info = context.global_ctx.get_isize_type_info_for_builtin_alias();
		alias_decl.alias_expr = context.type_as_expression(
			alias_decl.alias_expr.src_tokens,
			ast::make_base_type_typespec(alias_decl.alias_expr.src_tokens, info)
		);
		return true;
	}
	else if (alias_decl.id.values.back() == "usize")
	{
		auto const info = context.global_ctx.get_usize_type_info_for_builtin_alias();
		alias_decl.alias_expr = context.type_as_expression(
			alias_decl.alias_expr.src_tokens,
			ast::make_base_type_typespec(alias_decl.alias_expr.src_tokens, info)
		);
		return true;
	}
	else
	{
		return false;
	}
}

static ast::type_info::decl_function_ptr make_builtin_default_constructor(ast::type_info *info)
{
	auto result = ast::make_ast_unique<ast::decl_function>();
	result->body.return_type = make_base_type_typespec({}, info);
	switch (info->kind)
	{
		case ast::type_info::i8_:
			result->body.intrinsic_kind = ast::function_body::i8_default_constructor;
			break;
		case ast::type_info::i16_:
			result->body.intrinsic_kind = ast::function_body::i16_default_constructor;
			break;
		case ast::type_info::i32_:
			result->body.intrinsic_kind = ast::function_body::i32_default_constructor;
			break;
		case ast::type_info::i64_:
			result->body.intrinsic_kind = ast::function_body::i64_default_constructor;
			break;
		case ast::type_info::u8_:
			result->body.intrinsic_kind = ast::function_body::u8_default_constructor;
			break;
		case ast::type_info::u16_:
			result->body.intrinsic_kind = ast::function_body::u16_default_constructor;
			break;
		case ast::type_info::u32_:
			result->body.intrinsic_kind = ast::function_body::u32_default_constructor;
			break;
		case ast::type_info::u64_:
			result->body.intrinsic_kind = ast::function_body::u64_default_constructor;
			break;
		case ast::type_info::f32_:
			result->body.intrinsic_kind = ast::function_body::f32_default_constructor;
			break;
		case ast::type_info::f64_:
			result->body.intrinsic_kind = ast::function_body::f64_default_constructor;
			break;
		case ast::type_info::char_:
			result->body.intrinsic_kind = ast::function_body::char_default_constructor;
			break;
		case ast::type_info::bool_:
			result->body.intrinsic_kind = ast::function_body::bool_default_constructor;
			break;
		default:
			bz_unreachable;
	}
	result->body.flags = ast::function_body::intrinsic
		| ast::function_body::constructor
		| ast::function_body::default_constructor
		| ast::function_body::default_default_constructor;
	result->body.constructor_or_destructor_of = info;
	result->body.state = ast::resolve_state::symbol;
	result->body.symbol_name = result->body.get_symbol_name();
	return result;
}

static bool apply_builtin(
	ast::type_info &info,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (!context.global_ctx.add_builtin_type_info(&info))
	{
		context.report_error(info.src_tokens, bz::format("invalid type for '@{}'", attribute.name->value));
		return false;
	}

	if (info.kind != ast::type_info::str_ && info.kind != ast::type_info::null_t_)
	{
		info.flags = ast::type_info::default_constructible
			| ast::type_info::copy_constructible
			| ast::type_info::trivially_copy_constructible
			| ast::type_info::move_constructible
			| ast::type_info::trivially_move_constructible
			| ast::type_info::trivially_destructible
			| ast::type_info::trivially_move_destructible
			| ast::type_info::trivial
			| ast::type_info::trivially_relocatable;
		info.symbol_name = bz::format("builtin.{}", info.type_name.format_as_unqualified());
		switch (info.kind)
		{
		case ast::type_info::i8_:
		case ast::type_info::u8_:
			info.prototype = context.global_ctx.type_prototype_set->get_builtin_type(ast::builtin_type_kind::i8);
			break;
		case ast::type_info::i16_:
		case ast::type_info::u16_:
			info.prototype = context.global_ctx.type_prototype_set->get_builtin_type(ast::builtin_type_kind::i16);
			break;
		case ast::type_info::i32_:
		case ast::type_info::u32_:
		case ast::type_info::char_:
			info.prototype = context.global_ctx.type_prototype_set->get_builtin_type(ast::builtin_type_kind::i32);
			break;
		case ast::type_info::i64_:
		case ast::type_info::u64_:
			info.prototype = context.global_ctx.type_prototype_set->get_builtin_type(ast::builtin_type_kind::i64);
			break;
		case ast::type_info::f32_:
			info.prototype = context.global_ctx.type_prototype_set->get_builtin_type(ast::builtin_type_kind::f32);
			break;
		case ast::type_info::f64_:
			info.prototype = context.global_ctx.type_prototype_set->get_builtin_type(ast::builtin_type_kind::f64);
			break;
		case ast::type_info::bool_:
			info.prototype = context.global_ctx.type_prototype_set->get_builtin_type(ast::builtin_type_kind::i1);
			break;
		default:
			bz_unreachable;
		}

		info.default_default_constructor = make_builtin_default_constructor(&info);
		info.constructors.push_back(info.default_default_constructor.get());
	}

	return true;
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
			.get_string();

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
			.get_string();

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

static bool apply_overload_priority(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &
)
{
	auto const priority = attribute.args[0]
		.get_constant_value()
		.get_sint();

	func_body.overload_priority = priority;
	return true;
}

bz::vector<attribute_info_t> make_attribute_infos(bz::array_view<ast::type_info * const> builtin_type_infos)
{
	constexpr size_t N = 3;
	bz::vector<attribute_info_t> result;
	result.reserve(N);

	bz_assert(builtin_type_infos[ast::type_info::i64_] != nullptr);
	bz_assert(builtin_type_infos[ast::type_info::str_] != nullptr);
	auto const int64_type = ast::make_base_type_typespec({}, builtin_type_infos[ast::type_info::i64_]);
	auto const str_type = ast::make_base_type_typespec({}, builtin_type_infos[ast::type_info::str_]);

	result.push_back({
		"symbol_name",
		{ str_type },
		{ nullptr, nullptr, &apply_symbol_name, &apply_symbol_name, nullptr, nullptr }
	});
	result.push_back({
		"maybe_unused",
		{},
		{ nullptr, nullptr, nullptr, &apply_maybe_unused, nullptr, nullptr }
	});
	result.push_back({
		"overload_priority",
		{ int64_type },
		{ nullptr, nullptr, &apply_overload_priority, nullptr, nullptr, nullptr }
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
					context.report_error(arg, "attribute argument must be a constant expression");
				}
			}
		}
	}
	return good;
}

void resolve_attributes(ast::decl_function &func_decl, ctx::parse_context &context)
{
	for (auto &attribute : func_decl.attributes)
	{
		if (attribute.name->value == "__builtin")
		{
			apply_builtin(func_decl, attribute, context);
		}
		else if (auto const attribute_info = context.global_ctx.get_builtin_attribute(attribute.name->value))
		{
			auto const good = resolve_attribute(attribute, *attribute_info, context);
			if (good)
			{
				attribute_info->apply_funcs(func_decl, attribute, context);
			}
		}
		else
		{
			report_unknown_attribute(attribute, context);
		}
	}
}

void resolve_attributes(ast::decl_operator &op_decl, ctx::parse_context &context)
{
	for (auto &attribute : op_decl.attributes)
	{
		if (attribute.name->value == "__builtin")
		{
			apply_builtin(op_decl, attribute, context);
		}
		else if (attribute.name->value == "__builtin_assign")
		{
			apply_builtin_assign(op_decl, attribute, context);
		}
		else if (auto const attribute_info = context.global_ctx.get_builtin_attribute(attribute.name->value))
		{
			auto const good = resolve_attribute(attribute, *attribute_info, context);
			if (good)
			{
				attribute_info->apply_funcs(op_decl, attribute, context);
			}
		}
		else
		{
			report_unknown_attribute(attribute, context);
		}
	}
}

void resolve_attributes(ast::decl_variable &var_decl, ctx::parse_context &context)
{
	for (auto &attribute : var_decl.attributes)
	{
		if (auto const attribute_info = context.global_ctx.get_builtin_attribute(attribute.name->value))
		{
			auto const good = resolve_attribute(attribute, *attribute_info, context);
			if (good)
			{
				attribute_info->apply_funcs(var_decl, attribute, context);
			}
		}
		else
		{
			report_unknown_attribute(attribute, context);
		}
	}
}

void resolve_attributes(ast::decl_type_alias &alias_decl, ctx::parse_context &context)
{
	for (auto &attribute : alias_decl.attributes)
	{
		if (attribute.name->value == "__builtin")
		{
			apply_builtin(alias_decl, attribute, context);
		}
		else if (auto const attribute_info = context.global_ctx.get_builtin_attribute(attribute.name->value))
		{
			auto const good = resolve_attribute(attribute, *attribute_info, context);
			if (good)
			{
				attribute_info->apply_funcs(alias_decl, attribute, context);
			}
		}
		else
		{
			report_unknown_attribute(attribute, context);
		}
	}
}

void resolve_attributes(ast::type_info &info, ctx::parse_context &context)
{
	for (auto &attribute : info.attributes)
	{
		if (attribute.name->value == "__builtin")
		{
			apply_builtin(info, attribute, context);
		}
		else if (auto const attribute_info = context.global_ctx.get_builtin_attribute(attribute.name->value))
		{
			auto const good = resolve_attribute(attribute, *attribute_info, context);
			if (good)
			{
				attribute_info->apply_funcs(info, attribute, context);
			}
		}
		else
		{
			report_unknown_attribute(attribute, context);
		}
	}
}

} // namespace resolve
