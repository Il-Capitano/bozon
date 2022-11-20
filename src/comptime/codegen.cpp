#include "codegen.h"
#include "instructions.h"

namespace comptime
{

static type const *get_type(ast::typespec_view type, codegen_context &context)
{
	static_assert(ast::typespec_types::size() == 19);
	if (type.modifiers.empty())
	{
		switch (type.terminator_kind())
		{
		case ast::terminator_typespec_node_t::index_of<ast::ts_base_type>:
		{
			auto const info = type.get<ast::ts_base_type>().info;
			switch (info->kind)
			{
			case ast::type_info::int8_:
			case ast::type_info::uint8_:
				return context.get_builtin_type(builtin_type_kind::i8);
			case ast::type_info::int16_:
			case ast::type_info::uint16_:
				return context.get_builtin_type(builtin_type_kind::i16);
			case ast::type_info::int32_:
			case ast::type_info::uint32_:
				return context.get_builtin_type(builtin_type_kind::i32);
			case ast::type_info::int64_:
			case ast::type_info::uint64_:
				return context.get_builtin_type(builtin_type_kind::i64);
			case ast::type_info::float32_:
				return context.get_builtin_type(builtin_type_kind::f32);
			case ast::type_info::float64_:
				return context.get_builtin_type(builtin_type_kind::f64);
			case ast::type_info::char_:
				return context.get_builtin_type(builtin_type_kind::i32);
			case ast::type_info::str_:
				return context.get_str_t();
			case ast::type_info::bool_:
				return context.get_builtin_type(builtin_type_kind::i1);
			case ast::type_info::null_t_:
				return context.get_null_t();
			case ast::type_info::aggregate:
			{
				auto const elem_types = info->member_variables
					.transform([&context](auto const decl) { return get_type(decl->get_type(), context); })
					.collect();
				return context.get_aggregate_type(elem_types);
			}

			case ast::type_info::forward_declaration:
			default:
				bz_unreachable;
			}
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_enum>:
			return get_type(type.get<ast::ts_enum>().decl->underlying_type, context);
		case ast::terminator_typespec_node_t::index_of<ast::ts_void>:
			return context.get_builtin_type(builtin_type_kind::void_);
		case ast::terminator_typespec_node_t::index_of<ast::ts_function>:
			return context.get_pointer_type();
		case ast::terminator_typespec_node_t::index_of<ast::ts_array>:
		{
			auto &arr_t = type.get<ast::ts_array>();
			auto elem_t = get_type(arr_t.elem_type, context);
			return context.get_array_type(elem_t, arr_t.size);
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_array_slice>:
			return context.get_slice_t();
		case ast::terminator_typespec_node_t::index_of<ast::ts_tuple>:
		{
			auto &tuple_t = type.get<ast::ts_tuple>();
			auto const types = tuple_t.types
				.transform([&context](auto const &ts) { return get_type(ts, context); })
				.template collect<ast::arena_vector>();
			return context.get_aggregate_type(types);
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_auto>:
			bz_unreachable;
		case ast::terminator_typespec_node_t::index_of<ast::ts_unresolved>:
			bz_unreachable;
		case ast::terminator_typespec_node_t::index_of<ast::ts_typename>:
			bz_unreachable;
		default:
			bz_unreachable;
		}
	}
	else
	{
		switch (type.modifier_kind())
		{
		case ast::modifier_typespec_node_t::index_of<ast::ts_const>:
			return get_type(type.get<ast::ts_const>(), context);
		case ast::modifier_typespec_node_t::index_of<ast::ts_consteval>:
			return get_type(type.get<ast::ts_consteval>(), context);
		case ast::modifier_typespec_node_t::index_of<ast::ts_pointer>:
		case ast::modifier_typespec_node_t::index_of<ast::ts_lvalue_reference>:
		case ast::modifier_typespec_node_t::index_of<ast::ts_move_reference>:
			return context.get_pointer_type();
		case ast::modifier_typespec_node_t::index_of<ast::ts_optional>:
		{
			if (type.is_optional_pointer_like())
			{
				return context.get_pointer_type();
			}
			else
			{
				auto const inner_type = get_type(type.get<ast::ts_optional>(), context);
				return context.get_optional_type(inner_type);
			}
		}
		default:
			bz_unreachable;
		}
	}
}

static void generate_stmt_code(ast::statement const &stmt, codegen_context &context);

static expr_value generate_expr_code(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static expr_value generate_expr_code(
	ast::expr_identifier const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static expr_value generate_expr_code(
	ast::expr_integer_literal const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_null_literal const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_enum_literal const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_typed_literal const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_placeholder_literal const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_tuple const &tuple_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	if (!result_address.has_value())
	{
		auto const types = tuple_expr.elems
			.transform([](auto const &expr) { return expr.get_expr_type(); })
			.transform([&context](auto const ts) { return get_type(ts, context); })
			.collect();
		auto const result_type = context.get_aggregate_type(types);
		result_address = context.create_alloca(result_type);
	}

	auto const &result_expr_value = result_address.get();
	bz_assert(result_expr_value.get_type()->is_aggregate());
	bz_assert(result_expr_value.get_type()->get_aggregate_types().size() == tuple_expr.elems.size());
	for (auto const i : bz::iota(0, tuple_expr.elems.size()))
	{
		auto const elem_result_address = context.create_struct_gep(result_expr_value, i);
		generate_expr_code(tuple_expr.elems[i], context, elem_result_address);
	}

	return result_expr_value;
}

static expr_value generate_expr_code(
	ast::expr_unary_op const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_binary_op const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static expr_value generate_expr_code(
	ast::expr_tuple_subscript const &tuple_subscript,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	bz_assert(tuple_subscript.index.is<ast::constant_expression>());
	auto const &index_value = tuple_subscript.index.get<ast::constant_expression>().value;
	bz_assert(index_value.is_uint() || index_value.is_sint());
	auto const index_int_value = index_value.is_uint()
		? index_value.get_uint()
		: static_cast<uint64_t>(index_value.get_sint());

	expr_value result = expr_value::get_none();
	for (auto const i : bz::iota(0, tuple_subscript.base.elems.size()))
	{
		if (i == index_int_value)
		{
			result = generate_expr_code(tuple_subscript.base.elems[i], context, result_address);
		}
		else
		{
			generate_expr_code(tuple_subscript.base.elems[i], context, {});
		}
	}
	return result;
}

static expr_value generate_expr_code(
	ast::expr_rvalue_tuple_subscript const &rvalue_tuple_subscript,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	bz_assert(rvalue_tuple_subscript.index.is<ast::constant_expression>());
	auto const &index_value = rvalue_tuple_subscript.index.get<ast::constant_expression>().value;
	bz_assert(index_value.is_uint() || index_value.is_sint());
	auto const index_int_value = index_value.is_uint()
		? index_value.get_uint()
		: static_cast<uint64_t>(index_value.get_sint());

	auto const base_val = generate_expr_code(rvalue_tuple_subscript.base, context, {});
	bz_assert(base_val.is_reference());
	bz_assert(base_val.get_type()->is_aggregate());

	expr_value result = expr_value::get_none();
	for (auto const i : bz::iota(0, rvalue_tuple_subscript.elem_refs.size()))
	{
		if (rvalue_tuple_subscript.elem_refs[i].is_null())
		{
			continue;
		}

		auto const elem_ptr = context.create_struct_gep(base_val, i);
		auto const prev_value = context.push_value_reference(elem_ptr);
		if (i == index_int_value)
		{
			result = generate_expr_code(rvalue_tuple_subscript.elem_refs[i], context, result_address);
		}
		else
		{
			generate_expr_code(rvalue_tuple_subscript.elem_refs[i], context, {});
		}
		context.pop_value_reference(prev_value);
	}
	return result;
}

static expr_value generate_expr_code(
	ast::expr_subscript const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_rvalue_array_subscript const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_function_call const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static expr_value generate_expr_code(
	ast::expr_cast const &cast,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const expr_t = ast::remove_const_or_consteval(cast.expr.get_expr_type());
	auto const dest_t = ast::remove_const_or_consteval(cast.type);

	if (expr_t.is<ast::ts_base_type>() && dest_t.is<ast::ts_base_type>())
	{
		auto const dest_type = get_type(dest_t, context);
		auto const expr = generate_expr_code(cast.expr, context, {});
		auto const expr_kind = expr_t.get<ast::ts_base_type>().info->kind;
		auto const dest_kind = dest_t.get<ast::ts_base_type>().info->kind;

		if ((ast::is_integer_kind(expr_kind) || expr_kind == ast::type_info::bool_) && ast::is_integer_kind(dest_kind))
		{
			auto const result_value = context.create_int_cast(expr, dest_type, ast::is_signed_integer_kind(expr_kind));
			if (result_address.has_value())
			{
				context.create_store(result_value, result_address.get());
				return result_address.get();
			}
			else
			{
				return result_value;
			}
		}
		else if (ast::is_floating_point_kind(expr_kind) && ast::is_floating_point_kind(dest_kind))
		{
			auto const result_value = context.create_float_cast(expr, dest_type);
			if (result_address.has_value())
			{
				context.create_store(result_value, result_address.get());
				return result_address.get();
			}
			else
			{
				return result_value;
			}
		}
		else if (ast::is_floating_point_kind(expr_kind))
		{
			bz_assert(ast::is_integer_kind(dest_kind));
			auto const result_value = context.create_float_to_int_cast(expr, dest_type, ast::is_signed_integer_kind(dest_kind));
			if (result_address.has_value())
			{
				context.create_store(result_value, result_address.get());
				return result_address.get();
			}
			else
			{
				return result_value;
			}
		}
		else if (ast::is_integer_kind(expr_kind) && ast::is_floating_point_kind(dest_kind))
		{
			auto const result_value = context.create_int_to_float_cast(expr, dest_type);
			if (result_address.has_value())
			{
				context.create_store(result_value, result_address.get());
				return result_address.get();
			}
			else
			{
				return result_value;
			}
		}
		else
		{
			bz_assert(
				(expr_kind == ast::type_info::char_ && ast::is_integer_kind(dest_kind))
				|| (ast::is_integer_kind(expr_kind) && dest_kind == ast::type_info::char_)
			);
			auto const result_value = context.create_int_cast(expr, dest_type, ast::is_signed_integer_kind(expr_kind));
			if (result_address.has_value())
			{
				context.create_store(result_value, result_address.get());
				return result_address.get();
			}
			else
			{
				return result_value;
			}
		}
	}
	else if (
		(expr_t.is<ast::ts_pointer>() || expr_t.is_optional_pointer())
		&& (dest_t.is<ast::ts_pointer>() || dest_t.is_optional_pointer())
	)
	{
		auto const result_value = generate_expr_code(cast.expr, context, {});
		if (result_address.has_value())
		{
			context.create_store(result_value, result_address.get());
			return result_address.get();
		}
		else
		{
			return result_value;
		}
	}
	else if (expr_t.is<ast::ts_array>() && dest_t.is<ast::ts_array_slice>())
	{
		auto const expr_val = generate_expr_code(cast.expr, context, {});
		bz_assert(expr_val.get_type()->is_array());
		auto const array_size = expr_val.get_type()->get_array_size();
		auto const begin_ptr = context.create_struct_gep(expr_val, 0).get_reference();
		auto const end_ptr   = context.create_struct_gep(expr_val, array_size).get_reference();

		if (!result_address.has_value())
		{
			result_address = context.create_alloca(context.get_slice_t());
		}

		auto const &result_value = result_address.get();
		auto const begin_ptr_value = expr_value::get_value(begin_ptr, context.get_pointer_type());
		auto const end_ptr_value = expr_value::get_value(end_ptr, context.get_pointer_type());
		context.create_store(begin_ptr_value, context.create_struct_gep(result_value, 0));
		context.create_store(end_ptr_value, context.create_struct_gep(result_value, 1));
		return result_value;
	}
	else
	{
		bz_unreachable;
	}
}

static expr_value generate_expr_code(
	ast::expr_optional_cast const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_take_reference const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_take_move_reference const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static expr_value generate_expr_code(
	ast::expr_aggregate_init const &aggregate_init,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	if (!result_address.has_value())
	{
		auto const type = get_type(aggregate_init.type, context);
		result_address = context.create_alloca(type);
	}

	auto const &result_value = result_address.get();
	bz_assert(result_value.get_type()->is_aggregate() || result_value.get_type()->is_array());
	for (auto const i : bz::iota(0, aggregate_init.exprs.size()))
	{
		auto const member_ptr = context.create_struct_gep(result_value, i);
		generate_expr_code(aggregate_init.exprs[i], context, member_ptr);
	}
	return result_value;
}

static expr_value generate_expr_code(
	ast::expr_aggregate_default_construct const &aggregate_default_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	if (!result_address.has_value())
	{
		auto const type = get_type(aggregate_default_construct.type, context);
		result_address = context.create_alloca(type);
	}

	auto const &result_value = result_address.get();
	bz_assert(result_value.get_type()->is_aggregate() || result_value.get_type()->is_array());
	for (auto const i : bz::iota(0, aggregate_default_construct.default_construct_exprs.size()))
	{
		auto const member_ptr = context.create_struct_gep(result_value, i);
		generate_expr_code(aggregate_default_construct.default_construct_exprs[i], context, member_ptr);
	}
	return result_value;
}

static expr_value generate_expr_code(
	ast::expr_array_default_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_optional_default_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static expr_value generate_expr_code(
	ast::expr_builtin_default_construct const &builtin_default_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	bz_assert(builtin_default_construct.type.is<ast::ts_array_slice>());
	if (!result_address.has_value())
	{
		auto const slice_type = context.get_slice_t();
		result_address = context.create_alloca(slice_type);
	}

	auto const &result_value = result_address.get();
	auto const null_value = context.create_const_ptr_null();
	context.create_store(null_value, context.create_struct_gep(result_value, 0));
	context.create_store(null_value, context.create_struct_gep(result_value, 1));

	return result_value;
}

static expr_value generate_expr_code(
	ast::expr_aggregate_copy_construct const &aggregate_copy_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const copied_val = generate_expr_code(aggregate_copy_construct.copied_value, context, {});
	if (!result_address.has_value())
	{
		auto const type = copied_val.get_type();
		bz_assert(type->is_aggregate());
		result_address = context.create_alloca(type);
	}

	auto const &result_value = result_address.get();
	for (auto const i : bz::iota(0, aggregate_copy_construct.copy_exprs.size()))
	{
		auto const result_member_value = context.create_struct_gep(result_value, i);
		auto const member_value = context.create_struct_gep(copied_val, i);
		auto const prev_value = context.push_value_reference(member_value);
		generate_expr_code(aggregate_copy_construct.copy_exprs[i], context, result_member_value);
		context.pop_value_reference(prev_value);
	}
	return result_value;
}

static expr_value generate_expr_code(
	ast::expr_array_copy_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_optional_copy_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static expr_value generate_expr_code(
	ast::expr_builtin_copy_construct const &builtin_copy_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const copied_val = generate_expr_code(builtin_copy_construct.copied_value, context, {});
	if (copied_val.get_type()->is_aggregate())
	{
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(copied_val.get_type());
		}

		auto const &result_value = result_address.get();
		context.create_const_memcpy(result_value, copied_val, copied_val.get_type()->size);
		return result_value;
	}
	else
	{
		if (result_address.has_value())
		{
			context.create_store(copied_val, result_address.get());
			return result_address.get();
		}
		else
		{
			return expr_value::get_value(copied_val.get_value(context), copied_val.get_type());
		}
	}
}

static expr_value generate_expr_code(
	ast::expr_aggregate_move_construct const &aggregate_move_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const moved_val = generate_expr_code(aggregate_move_construct.moved_value, context, {});
	if (!result_address.has_value())
	{
		auto const type = moved_val.get_type();
		bz_assert(type->is_aggregate());
		result_address = context.create_alloca(type);
	}

	auto const &result_value = result_address.get();
	for (auto const i : bz::iota(0, aggregate_move_construct.move_exprs.size()))
	{
		auto const result_member_value = context.create_struct_gep(result_value, i);
		auto const member_value = context.create_struct_gep(moved_val, i);
		auto const prev_value = context.push_value_reference(member_value);
		generate_expr_code(aggregate_move_construct.move_exprs[i], context, result_member_value);
		context.pop_value_reference(prev_value);
	}
	return result_value;
}

static expr_value generate_expr_code(
	ast::expr_array_move_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_optional_move_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static expr_value generate_expr_code(
	ast::expr_trivial_relocate const &trivial_relocate,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const val = generate_expr_code(trivial_relocate.value, context, {});
	auto const type = val.get_type();

	if (type->is_builtin() || type->is_pointer())
	{
		if (result_address.has_value())
		{
			context.create_store(val, result_address.get());
			return result_address.get();
		}
		else
		{
			return expr_value::get_value(val.get_value(context), type);
		}
	}
	else
	{
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(type);
		}

		auto const &result_value = result_address.get();
		context.create_const_memcpy(result_value, val, type->size);
		return result_value;
	}
}

static expr_value generate_expr_code(
	ast::expr_aggregate_destruct const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_array_destruct const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_optional_destruct const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_base_type_destruct const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_destruct_value const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_aggregate_assign const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_aggregate_swap const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_array_swap const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_optional_swap const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_base_type_swap const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_trivial_swap const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_array_assign const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_optional_assign const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_optional_null_assign const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_optional_value_assign const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_base_type_assign const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_trivial_assign const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_member_access const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_optional_extract_value const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_rvalue_member_access const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_type_member_access const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_compound const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_if const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_if_consteval const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_switch const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_break const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_continue const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_unreachable const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_generic_type_instantiation const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);
static expr_value generate_expr_code(
	ast::expr_bitcode_value_reference const &,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static expr_value generate_expr_code(
	ast::constant_expression const &const_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::dynamic_expression const &dyn_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	if (
		!result_address.has_value()
		&& dyn_expr.kind == ast::expression_type_kind::rvalue
		&& (
			dyn_expr.destruct_op.not_null()
			|| dyn_expr.expr.is<ast::expr_compound>()
			|| dyn_expr.expr.is<ast::expr_if>()
			|| dyn_expr.expr.is<ast::expr_switch>()
		)
	)
	{
		result_address = context.create_alloca(get_type(dyn_expr.type, context));
	}

	expr_value result = expr_value::get_none();
	switch (dyn_expr.expr.kind())
	{
	static_assert(ast::expr_t::variant_count == 61);
	case ast::expr_t::index<ast::expr_identifier>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_identifier>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_integer_literal>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_integer_literal>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_null_literal>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_null_literal>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_enum_literal>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_enum_literal>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_typed_literal>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_typed_literal>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_placeholder_literal>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_placeholder_literal>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_tuple>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_tuple>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_unary_op>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_unary_op>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_binary_op>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_binary_op>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_tuple_subscript>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_tuple_subscript>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_rvalue_tuple_subscript>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_rvalue_tuple_subscript>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_subscript>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_subscript>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_rvalue_array_subscript>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_rvalue_array_subscript>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_function_call>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_function_call>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_cast>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_cast>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_optional_cast>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_optional_cast>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_take_reference>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_take_reference>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_take_move_reference>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_take_move_reference>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_aggregate_init>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_aggregate_init>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_aggregate_default_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_aggregate_default_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_array_default_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_array_default_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_optional_default_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_optional_default_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_builtin_default_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_builtin_default_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_aggregate_copy_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_aggregate_copy_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_array_copy_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_array_copy_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_optional_copy_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_optional_copy_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_builtin_copy_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_builtin_copy_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_aggregate_move_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_aggregate_move_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_array_move_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_array_move_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_optional_move_construct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_optional_move_construct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_trivial_relocate>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_trivial_relocate>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_aggregate_destruct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_aggregate_destruct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_array_destruct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_array_destruct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_optional_destruct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_optional_destruct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_base_type_destruct>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_base_type_destruct>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_destruct_value>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_destruct_value>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_aggregate_assign>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_aggregate_assign>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_aggregate_swap>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_aggregate_swap>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_array_swap>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_array_swap>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_optional_swap>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_optional_swap>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_base_type_swap>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_base_type_swap>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_trivial_swap>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_trivial_swap>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_array_assign>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_array_assign>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_optional_assign>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_optional_assign>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_optional_null_assign>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_optional_null_assign>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_optional_value_assign>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_optional_value_assign>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_base_type_assign>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_base_type_assign>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_trivial_assign>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_trivial_assign>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_member_access>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_member_access>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_optional_extract_value>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_optional_extract_value>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_rvalue_member_access>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_rvalue_member_access>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_type_member_access>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_type_member_access>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_compound>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_compound>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_if>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_if>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_if_consteval>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_if_consteval>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_switch>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_switch>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_break>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_break>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_continue>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_continue>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_unreachable>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_unreachable>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_generic_type_instantiation>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_generic_type_instantiation>(), context, result_address);
		break;
	case ast::expr_t::index<ast::expr_bitcode_value_reference>:
		result = generate_expr_code(dyn_expr.expr.get<ast::expr_bitcode_value_reference>(), context, result_address);
		break;
	default:
		bz_unreachable;
	}

	if (dyn_expr.destruct_op.not_null() || dyn_expr.destruct_op.move_destructed_decl != nullptr)
	{
		bz_assert(result.kind == expr_value_kind::reference);
		context.push_self_destruct_operation(dyn_expr.destruct_op, result);
	}
	return result;
}

static expr_value generate_expr_code(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	switch (expr.kind())
	{
		case ast::expression::index_of<ast::constant_expression>:
			return generate_expr_code(expr.get_constant(), context, result_address);
		case ast::expression::index_of<ast::dynamic_expression>:
			return generate_expr_code(expr.get_dynamic(), context, result_address);
		case ast::expression::index_of<ast::error_expression>:
			bz_unreachable;
		default:
			bz_unreachable;
	}
}

static void generate_stmt_code(ast::stmt_while const &while_stmt, codegen_context &context)
{
	auto const cond_check_bb = context.add_basic_block();
	auto const end_bb = context.add_basic_block();

	auto const prev_loop_info = context.push_loop(end_bb, cond_check_bb);

	context.create_jump(cond_check_bb);
	context.set_current_basic_block(cond_check_bb);
	auto const cond_prev_info = context.push_expression_scope();
	auto const condition = generate_expr_code(while_stmt.condition, context, {}).get_value(context);
	context.pop_expression_scope(cond_prev_info);

	auto const while_bb = context.add_basic_block();
	context.create_conditional_jump(condition, while_bb, end_bb);
	context.set_current_basic_block(while_bb);

	auto const while_prev_info = context.push_expression_scope();
	generate_expr_code(while_stmt.while_block, context, {});
	context.pop_expression_scope(while_prev_info);

	context.create_jump(cond_check_bb);
	context.set_current_basic_block(end_bb);

	context.pop_loop(prev_loop_info);
}

static void generate_stmt_code(ast::stmt_for const &for_stmt, codegen_context &context)
{
	auto const init_prev_info = context.push_expression_scope();
	if (for_stmt.init.not_null())
	{
		generate_stmt_code(for_stmt.init, context);
	}

	auto const begin_bb = context.get_current_basic_block();

	auto const iteration_bb = context.add_basic_block();
	auto const end_bb = context.add_basic_block();
	auto const prev_loop_info = context.push_loop(end_bb, iteration_bb);

	context.set_current_basic_block(iteration_bb);
	if (for_stmt.iteration.not_null())
	{
		auto const prev_info = context.push_expression_scope();
		generate_expr_code(for_stmt.iteration, context, {});
		context.pop_expression_scope(prev_info);
	}

	auto const cond_check_bb = context.add_basic_block();
	context.create_jump(cond_check_bb);

	context.set_current_basic_block(begin_bb);
	context.create_jump(cond_check_bb);

	context.set_current_basic_block(cond_check_bb);

	instruction_ref condition = {};
	if (for_stmt.condition.not_null())
	{
		auto const prev_info = context.push_expression_scope();
		condition = generate_expr_code(for_stmt.condition, context, {}).get_value(context);
		context.pop_expression_scope(prev_info);
	}

	auto const for_bb = context.add_basic_block();
	if (for_stmt.condition.not_null())
	{
		context.create_conditional_jump(condition, for_bb, end_bb);
	}
	else
	{
		context.create_jump(for_bb);
	}
	context.set_current_basic_block(for_bb);

	auto const for_prev_info = context.push_expression_scope();
	generate_expr_code(for_stmt.for_block, context, {});
	context.pop_expression_scope(for_prev_info);

	context.create_jump(iteration_bb);
	context.set_current_basic_block(end_bb);

	context.pop_expression_scope(init_prev_info);
	context.pop_loop(prev_loop_info);
}

static void generate_stmt_code(ast::stmt_foreach const &foreach_stmt, codegen_context &context)
{
	auto const outer_prev_info = context.push_expression_scope();

	generate_stmt_code(foreach_stmt.range_var_decl, context);
	generate_stmt_code(foreach_stmt.iter_var_decl, context);
	generate_stmt_code(foreach_stmt.end_var_decl, context);

	auto const begin_bb = context.get_current_basic_block();

	auto const iteration_bb = context.add_basic_block();
	auto const end_bb = context.add_basic_block();
	auto const prev_loop_info = context.push_loop(end_bb, iteration_bb);

	context.set_current_basic_block(iteration_bb);
	generate_expr_code(foreach_stmt.iteration, context, {});

	auto const condition_check_bb = context.add_basic_block();
	context.create_jump(condition_check_bb);
	context.set_current_basic_block(begin_bb);
	context.create_jump(condition_check_bb);

	context.set_current_basic_block(condition_check_bb);
	auto const condition = generate_expr_code(foreach_stmt.condition, context, {}).get_value(context);

	auto const foreach_bb = context.add_basic_block();
	context.create_conditional_jump(condition, foreach_bb, end_bb);

	auto const iter_prev_info = context.push_expression_scope();
	generate_stmt_code(foreach_stmt.iter_deref_var_decl, context);
	generate_expr_code(foreach_stmt.for_block, context, {});
	context.pop_expression_scope(iter_prev_info);

	context.create_jump(iteration_bb);
	context.set_current_basic_block(end_bb);

	context.set_current_basic_block(end_bb);
	context.pop_loop(prev_loop_info);
	context.pop_expression_scope(outer_prev_info);
}

static void generate_stmt_code(ast::stmt_return const &return_stmt, codegen_context &context)
{
	if (return_stmt.expr.is_null())
	{
		context.emit_all_destruct_operations();
		context.create_ret_void();
	}
	if (context.function_return_address.has_value())
	{
		bz_assert(return_stmt.expr.not_null());
		generate_expr_code(return_stmt.expr, context, context.function_return_address);
		context.emit_all_destruct_operations();
		context.create_ret_void();
	}
	else
	{
		auto const result_value = generate_expr_code(return_stmt.expr, context, {}).get_value(context);
		context.emit_all_destruct_operations();
		context.create_ret(result_value);
	}
}

static void generate_stmt_code(ast::stmt_defer const &defer_stmt, codegen_context &context)
{
	context.push_destruct_operation(defer_stmt.deferred_expr);
}

static void generate_stmt_code(ast::stmt_no_op const &, codegen_context &)
{
	// nothing
}

static void generate_stmt_code(ast::stmt_expression const &expr_stmt, codegen_context &context)
{
	auto const prev_info = context.push_expression_scope();
	generate_expr_code(expr_stmt.expr, context, {});
	context.pop_expression_scope(prev_info);
}

static void generate_stmt_code(ast::decl_variable const &, codegen_context &context);

static void generate_stmt_code(ast::statement const &stmt, codegen_context &context)
{
	switch (stmt.kind())
	{
	static_assert(ast::statement::variant_count == 16);
	case ast::statement::index<ast::stmt_while>:
		generate_stmt_code(stmt.get<ast::stmt_while>(), context);
		break;
	case ast::statement::index<ast::stmt_for>:
		generate_stmt_code(stmt.get<ast::stmt_for>(), context);
		break;
	case ast::statement::index<ast::stmt_foreach>:
		generate_stmt_code(stmt.get<ast::stmt_foreach>(), context);
		break;
	case ast::statement::index<ast::stmt_return>:
		generate_stmt_code(stmt.get<ast::stmt_return>(), context);
		break;
	case ast::statement::index<ast::stmt_defer>:
		generate_stmt_code(stmt.get<ast::stmt_defer>(), context);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		generate_stmt_code(stmt.get<ast::stmt_no_op>(), context);
		break;
	case ast::statement::index<ast::stmt_expression>:
		generate_stmt_code(stmt.get<ast::stmt_expression>(), context);
		break;
	case ast::statement::index<ast::stmt_static_assert>:
		// nothing
		break;

	case ast::statement::index<ast::decl_variable>:
		generate_stmt_code(stmt.get<ast::decl_variable>(), context);
		break;

	case ast::statement::index<ast::decl_function>:
	case ast::statement::index<ast::decl_operator>:
	case ast::statement::index<ast::decl_struct>:
	case ast::statement::index<ast::decl_enum>:
	case ast::statement::index<ast::decl_import>:
	case ast::statement::index<ast::decl_type_alias>:
		break;
	default:
		bz_unreachable;
	}
}

void generate_code(ast::function_body &body, codegen_context &context)
{
	bz_assert(body.state == ast::resolve_state::all);

	for (auto const &stmt : body.get_statements())
	{
		generate_stmt_code(stmt, context);
	}
}

} // namespace comptime
