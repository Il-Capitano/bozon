#include "codegen.h"
#include "codegen_context.h"
#include "memory_value_conversion.h"
#include "global_data.h"

namespace comptime
{

type const *get_type(ast::typespec_view type, codegen_context &context)
{
	return ast::get_type_prototype(type, context.type_set);
}

static expr_value value_or_result_address(expr_value value, bz::optional<expr_value> result_address, codegen_context &context)
{
	if (result_address.has_value())
	{
		context.create_store(value, result_address.get());
		context.create_start_lifetime(result_address.get());
		return result_address.get();
	}
	else
	{
		return value.get_value(context);
	}
}

static void generate_value_copy(expr_value value, expr_value dest, codegen_context &context)
{
	bz_assert(dest.is_reference());
	bz_assert(value.get_type() == dest.get_type());
	if (value.is_value() || value.get_type()->is_builtin() || value.get_type()->is_pointer())
	{
		context.create_store(value, dest);
	}
	else
	{
		context.create_const_memcpy(dest, value, value.get_type()->size);
	}
}

struct loop_info_t
{
	basic_block_ref condition_check_bb;
	basic_block_ref loop_bb;
	expr_value index_alloca;
	expr_value index;
	expr_value condition;
	codegen_context::expression_scope_info_t prev_scope_info;
};

static loop_info_t create_loop_start(size_t size, codegen_context &context)
{
	auto const index_alloca = context.create_alloca_without_lifetime(context.get_builtin_type(builtin_type_kind::i64));
	context.create_store(context.create_const_u64(0), index_alloca);

	auto const condition_check_bb = context.add_basic_block();
	context.create_jump(condition_check_bb);
	context.set_current_basic_block(condition_check_bb);
	auto const condition = context.create_int_cmp_neq(index_alloca, context.create_const_u64(size));

	auto const loop_bb = context.add_basic_block();
	context.set_current_basic_block(loop_bb);
	auto const index = index_alloca.get_value(context);

	return {
		.condition_check_bb = condition_check_bb,
		.loop_bb = loop_bb,
		.index_alloca = index_alloca,
		.index = index,
		.condition = condition,
		.prev_scope_info = context.push_expression_scope(),
	};
}

static void create_loop_end(loop_info_t loop_info, codegen_context &context)
{
	context.pop_expression_scope(loop_info.prev_scope_info);

	auto const next_i = context.create_add(loop_info.index, context.create_const_u64(1));
	context.create_store(next_i, loop_info.index_alloca);
	context.create_jump(loop_info.condition_check_bb);

	auto const end_bb = context.add_basic_block();
	context.set_current_basic_block(loop_info.condition_check_bb);
	context.create_conditional_jump(loop_info.condition, loop_info.loop_bb, end_bb);
	context.set_current_basic_block(end_bb);
}

struct reversed_loop_info_t
{
	basic_block_ref condition_check_bb;
	basic_block_ref loop_bb;
	expr_value index_alloca;
	expr_value index;
	expr_value condition;
	codegen_context::expression_scope_info_t prev_scope_info;
};

static reversed_loop_info_t create_reversed_loop_start(size_t size, codegen_context &context)
{
	auto const index_alloca = context.create_alloca_without_lifetime(context.get_builtin_type(builtin_type_kind::i64));
	context.create_store(context.create_const_u64(size), index_alloca);

	auto const condition_check_bb = context.add_basic_block();
	context.create_jump(condition_check_bb);
	context.set_current_basic_block(condition_check_bb);
	auto const condition = context.create_int_cmp_neq(index_alloca, context.create_const_u64(0));

	auto const loop_bb = context.add_basic_block();
	context.set_current_basic_block(loop_bb);
	auto const index = context.create_sub(index_alloca, context.create_const_u64(1));

	return {
		.condition_check_bb = condition_check_bb,
		.loop_bb = loop_bb,
		.index_alloca = index_alloca,
		.index = index,
		.condition = condition,
		.prev_scope_info = context.push_expression_scope(),
	};
}

static void create_reversed_loop_end(reversed_loop_info_t loop_info, codegen_context &context)
{
	context.pop_expression_scope(loop_info.prev_scope_info);

	context.create_store(loop_info.index, loop_info.index_alloca);
	context.create_jump(loop_info.condition_check_bb);

	auto const end_bb = context.add_basic_block();
	context.set_current_basic_block(loop_info.condition_check_bb);
	context.create_conditional_jump(loop_info.condition, loop_info.loop_bb, end_bb);
	context.set_current_basic_block(end_bb);
}

static expr_value get_optional_value(expr_value opt_value, codegen_context &context)
{
	if (opt_value.get_type()->is_pointer())
	{
		return opt_value;
	}
	else
	{
		return context.create_struct_gep(opt_value, 0);
	}
}

static expr_value get_optional_has_value(expr_value opt_value, codegen_context &context)
{
	if (opt_value.get_type()->is_pointer())
	{
		return context.create_pointer_cmp_neq(opt_value, context.create_const_ptr_null());
	}
	else
	{
		return context.create_struct_gep(opt_value, 1);
	}
}

static expr_value get_optional_has_value_ref(expr_value opt_value, codegen_context &context)
{
	bz_assert(opt_value.get_type()->is_aggregate());
	return context.create_struct_gep(opt_value, 1);
}

static void set_optional_has_value(expr_value opt_value, bool has_value, codegen_context &context)
{
	if (opt_value.get_type()->is_pointer())
	{
		if (!has_value)
		{
			context.create_store(context.create_const_ptr_null(), opt_value);
		}
	}
	else
	{
		auto const has_value_ref = context.create_struct_gep(opt_value, 1);
		bz_assert(has_value_ref.get_type()->is_builtin() && has_value_ref.get_type()->get_builtin_kind() == builtin_type_kind::i1);
		context.create_store(context.create_const_i1(has_value), has_value_ref);
	}
}

static void set_optional_has_value(expr_value opt_value, expr_value has_value, codegen_context &context)
{
	bz_assert(opt_value.get_type()->is_aggregate());
	auto const has_value_ref = context.create_struct_gep(opt_value, 1);
	bz_assert(has_value_ref.get_type()->is_builtin() && has_value_ref.get_type()->get_builtin_kind() == builtin_type_kind::i1);
	context.create_store(has_value, has_value_ref);
}

static void generate_stmt_code(ast::statement const &stmt, codegen_context &context);

static expr_value generate_expr_code(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_variable_name const &var_name,
	codegen_context &context
)
{
	auto const result = context.get_variable(var_name.decl);

	if (result.is_none())
	{
		context.create_error(
			lex::src_tokens::from_range(var_name.id.tokens),
			bz::format("variable '{}' cannot be used in a constant expression", var_name.id.as_string())
		);
		auto const type = get_type(var_name.decl->get_type(), context);
		return context.get_dummy_value(type);
	}

	if (var_name.decl->get_type().is<ast::ts_lvalue_reference>() || var_name.decl->get_type().is<ast::ts_move_reference>())
	{
		auto const object_typespec = var_name.decl->get_type().remove_any_reference();
		context.create_memory_access_check(original_expression.src_tokens, result, object_typespec);
	}
	return result;
}

static expr_value generate_expr_code(
	ast::expr_function_name const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_function_alias_name const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_function_overload_set const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_struct_name const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_enum_name const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_type_alias_name const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	// this is always a constant expression
	bz_unreachable;
}

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
	ast::expr_typename_literal const &,
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
	for (auto const i : bz::iota(0, tuple_expr.elems.size()))
	{
		if (result_address.has_value() && tuple_expr.elems[i].get_expr_type().is_reference())
		{
			auto const elem_result_address = context.create_struct_gep(result_address.get(), i);
			auto const ref_ref = generate_expr_code(tuple_expr.elems[i], context, {});
			auto const ref_value = expr_value::get_value(ref_ref.get_reference(), context.get_pointer_type());
			bz_assert(elem_result_address.get_type()->is_pointer());
			context.create_store(ref_value, elem_result_address);
			context.create_start_lifetime(elem_result_address);
		}
		else if (result_address.has_value())
		{
			auto const elem_result_address = context.create_struct_gep(result_address.get(), i);
			generate_expr_code(tuple_expr.elems[i], context, elem_result_address);
		}
		else
		{
			generate_expr_code(tuple_expr.elems[i], context, {});
		}
	}

	return result_address.has_value() ? result_address.get() : expr_value::get_none();
}

static expr_value generate_builtin_unary_address_of(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const value = generate_expr_code(expr, context, {});
	if (!value.is_reference())
	{
		if (auto const id_expr = expr.get_expr().get_if<ast::expr_variable_name>())
		{
			context.create_error(
				expr.src_tokens,
				bz::format("unable to take address of variable '{}'", id_expr->decl->get_id().format_as_unqualified())
			);
		}
		else
		{
			context.create_error(expr.src_tokens, "unable to take address of value");
		}
		return result_address.has_value()
			? result_address.get()
			: context.get_dummy_value(context.get_pointer_type());
	}
	else
	{
		auto const value_ptr = expr_value::get_value(value.get_reference(), context.get_pointer_type());
		return value_or_result_address(value_ptr, result_address, context);
	}
}

static expr_value generate_expr_code(
	ast::expr_unary_op const &unary_op,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	switch (unary_op.op)
	{
	case lex::token::address_of:
		return generate_builtin_unary_address_of(unary_op.expr, context, result_address);
	case lex::token::kw_move:
	case lex::token::kw_unsafe_move:
		bz_assert(!result_address.has_value());
		return generate_expr_code(unary_op.expr, context, result_address);
	default:
		bz_unreachable;
	}
}

static expr_value generate_builtin_binary_bool_and(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const result_value = context.create_alloca_without_lifetime(context.get_builtin_type(builtin_type_kind::i1));

	auto const lhs_prev_info = context.push_expression_scope();
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	context.create_store(lhs_value, result_value);
	context.pop_expression_scope(lhs_prev_info);
	auto const lhs_bb_end = context.get_current_basic_block();

	auto const rhs_bb = context.add_basic_block();
	context.set_current_basic_block(rhs_bb);

	auto const rhs_prev_info = context.push_expression_scope();
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	context.create_store(rhs_value, result_value);
	context.pop_expression_scope(rhs_prev_info);
	auto const rhs_bb_end = context.get_current_basic_block();

	auto const end_bb = context.add_basic_block();

	context.set_current_basic_block(lhs_bb_end);
	// if lhs is true we need to check rhs, otherwise short-circuit to end_bb
	context.create_conditional_jump(lhs_value, rhs_bb, end_bb);

	context.set_current_basic_block(rhs_bb_end);
	context.create_jump(end_bb);

	context.set_current_basic_block(end_bb);

	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_builtin_binary_bool_xor(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const result_value = context.create_xor(lhs_value, rhs_value);

	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_builtin_binary_bool_or(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const result_value = context.create_alloca_without_lifetime(context.get_builtin_type(builtin_type_kind::i1));

	auto const lhs_prev_info = context.push_expression_scope();
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	context.create_store(lhs_value, result_value);
	context.pop_expression_scope(lhs_prev_info);
	auto const lhs_bb_end = context.get_current_basic_block();

	auto const rhs_bb = context.add_basic_block();
	context.set_current_basic_block(rhs_bb);

	auto const rhs_prev_info = context.push_expression_scope();
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	context.create_store(rhs_value, result_value);
	context.pop_expression_scope(rhs_prev_info);
	auto const rhs_bb_end = context.get_current_basic_block();

	auto const end_bb = context.add_basic_block();

	context.set_current_basic_block(lhs_bb_end);
	// if lhs is false we need to check rhs, otherwise short-circuit to end_bb
	context.create_conditional_jump(lhs_value, end_bb, rhs_bb);

	context.set_current_basic_block(rhs_bb_end);
	context.create_jump(end_bb);

	context.set_current_basic_block(end_bb);

	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_expr_code(
	ast::expr_binary_op const &binary_op,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	switch (binary_op.op)
	{
	case lex::token::comma:
		generate_expr_code(binary_op.lhs, context, {});
		return generate_expr_code(binary_op.rhs, context, result_address);
	case lex::token::bool_and:
		return generate_builtin_binary_bool_and(binary_op.lhs, binary_op.rhs, context, result_address);
	case lex::token::bool_xor:
		return generate_builtin_binary_bool_xor(binary_op.lhs, binary_op.rhs, context, result_address);
	case lex::token::bool_or:
		return generate_builtin_binary_bool_or(binary_op.lhs, binary_op.rhs, context, result_address);
	default:
		bz_unreachable;
	}
}

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
	bz_assert(rvalue_tuple_subscript.index.is_constant());
	auto const &index_value = rvalue_tuple_subscript.index.get_constant_value();
	bz_assert(index_value.is_uint() || index_value.is_sint());
	auto const index_int_value = index_value.is_uint()
		? index_value.get_uint()
		: static_cast<uint64_t>(index_value.get_sint());

	auto const base_val = generate_expr_code(rvalue_tuple_subscript.base, context, {});
	bz_assert(base_val.is_reference());
	bz_assert(base_val.get_type()->is_aggregate());

	auto const is_reference_result = rvalue_tuple_subscript.elem_refs[index_int_value].get_expr_type().is_reference();
	bz_assert(!result_address.has_value() || !is_reference_result);
	expr_value result = expr_value::get_none();
	for (auto const i : bz::iota(0, rvalue_tuple_subscript.elem_refs.size()))
	{
		if (rvalue_tuple_subscript.elem_refs[i].is_null())
		{
			continue;
		}

		auto const elem_ptr = [&]() {
			if (i == index_int_value && is_reference_result)
			{
				auto const ref_ref = context.create_struct_gep(base_val, i);
				bz_assert(ref_ref.get_type()->is_pointer());
				auto const ref_value = context.create_load(ref_ref);
				auto const accessed_type = rvalue_tuple_subscript.elem_refs[index_int_value].get_expr_type();
				return expr_value::get_reference(
					ref_value.get_value_as_instruction(context),
					get_type(accessed_type.remove_reference(), context)
				);
			}
			else
			{
				return context.create_struct_gep(base_val, i);
			}
		}();
		auto const prev_value = context.push_value_reference(elem_ptr);
		if (i == index_int_value)
		{
			auto const prev_info = context.push_expression_scope();
			result = generate_expr_code(rvalue_tuple_subscript.elem_refs[i], context, result_address);
			context.pop_expression_scope(prev_info);
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
	lex::src_tokens const &src_tokens,
	ast::expr_subscript const &subscript,
	codegen_context &context
)
{
	auto const base_type = subscript.base.get_expr_type().remove_mut_reference();
	if (base_type.is<ast::ts_array>())
	{
		auto const array = generate_expr_code(subscript.base, context, {});
		auto index = generate_expr_code(subscript.index, context, {}).get_value(context);
		bz_assert(subscript.index.get_expr_type().remove_any_mut().is<ast::ts_base_type>());
		auto const kind = subscript.index.get_expr_type().remove_any_mut().get<ast::ts_base_type>().info->kind;

		bz_assert(index.get_type()->is_builtin());
		auto const size = base_type.get<ast::ts_array>().size;
		auto const is_index_signed = ast::is_signed_integer_kind(kind);
		if (context.is_64_bit() || index.get_type()->get_builtin_kind() == builtin_type_kind::i64)
		{
			bz_assert(size <= std::numeric_limits<uint64_t>::max());
			index = context.create_int_cast(index, context.get_builtin_type(builtin_type_kind::i64), is_index_signed);
			context.create_array_bounds_check(
				src_tokens,
				index,
				context.create_const_u64(size),
				is_index_signed
			);
		}
		else
		{
			bz_assert(size <= std::numeric_limits<uint32_t>::max());
			index = context.create_int_cast(index, context.get_builtin_type(builtin_type_kind::i32), is_index_signed);
			context.create_array_bounds_check(
				src_tokens,
				index,
				context.create_const_u32(static_cast<uint32_t>(size)),
				is_index_signed
			);
		}
		return context.create_array_gep(array, index);
	}
	else if (base_type.is<ast::ts_array_slice>())
	{
		auto const slice = generate_expr_code(subscript.base, context, {});
		auto const index = generate_expr_code(subscript.index, context, {}).get_value(context);
		bz_assert(subscript.index.get_expr_type().remove_any_mut().is<ast::ts_base_type>());
		auto const kind = subscript.index.get_expr_type().remove_any_mut().get<ast::ts_base_type>().info->kind;
		auto const elem_ts = base_type.get<ast::ts_array_slice>().elem_type.as_typespec_view();
		auto const elem_type = get_type(elem_ts, context);

		auto const begin_ptr = context.create_struct_gep(slice, 0).get_value(context);
		auto const end_ptr   = context.create_struct_gep(slice, 1).get_value(context);

		auto const size = context.create_ptrdiff_unchecked(end_ptr, begin_ptr, elem_type);
		auto const is_index_signed = ast::is_signed_integer_kind(kind);
		if (context.is_64_bit() || index.get_type()->get_builtin_kind() == builtin_type_kind::i64)
		{
			auto const index_cast = context.create_int_cast(index, context.get_builtin_type(builtin_type_kind::i64), is_index_signed);
			auto const size_cast = size.get_type() != index_cast.get_type()
				? context.create_int_cast(size, index_cast.get_type(), false)
				: size;
			context.create_array_bounds_check(src_tokens, index_cast, size_cast, is_index_signed);
			auto const result = context.create_array_slice_gep(begin_ptr, index_cast, elem_type);
			context.create_memory_access_check(src_tokens, result, elem_ts);
			return result;
		}
		else
		{
			auto const index_cast = context.create_int_cast(index, context.get_builtin_type(builtin_type_kind::i32), is_index_signed);
			bz_assert(size.get_type() == index_cast.get_type());
			context.create_array_bounds_check(src_tokens, index_cast, size, is_index_signed);
			auto const result = context.create_array_slice_gep(begin_ptr, index_cast, elem_type);
			context.create_memory_access_check(src_tokens, result, elem_ts);
			return result;
		}
	}
	else
	{
		bz_assert(base_type.is<ast::ts_tuple>());
		auto const tuple = generate_expr_code(subscript.base, context, {});
		bz_assert(subscript.index.is_constant());
		auto const &index_value = subscript.index.get_constant_value();
		bz_assert(index_value.is_uint() || index_value.is_sint());
		auto const index_int_value = index_value.is_uint()
			? index_value.get_uint()
			: static_cast<uint64_t>(index_value.get_sint());

		bz_assert(tuple.get_type()->is_aggregate());

		auto const &types = base_type.get<ast::ts_tuple>().types;
		if (types[index_int_value].is_reference())
		{
			auto const ref_value = context.create_struct_gep(tuple, index_int_value).get_value_as_instruction(context);
			auto const type = get_type(types[index_int_value].remove_mut_reference(), context);
			return expr_value::get_reference(ref_value, type);
		}
		else
		{
			return context.create_struct_gep(tuple, index_int_value);
		}
	}
}

static expr_value generate_expr_code(
	lex::src_tokens const &src_tokens,
	ast::expr_rvalue_array_subscript const &rvalue_array_subscript,
	codegen_context &context
)
{
	bz_assert(rvalue_array_subscript.base.get_expr_type().is<ast::ts_array>());
	auto const &array_t = rvalue_array_subscript.base.get_expr_type().get<ast::ts_array>();
	auto const array = generate_expr_code(rvalue_array_subscript.base, context, {});
	auto index = generate_expr_code(rvalue_array_subscript.index, context, {}).get_value(context);
	bz_assert(rvalue_array_subscript.index.get_expr_type().remove_any_mut().is<ast::ts_base_type>());
	auto const kind = rvalue_array_subscript.index.get_expr_type().remove_any_mut().get<ast::ts_base_type>().info->kind;

	bz_assert(index.get_type()->is_builtin());
	auto const size = array_t.size;
	auto const is_index_signed = ast::is_signed_integer_kind(kind);
	if (context.is_64_bit() || index.get_type()->get_builtin_kind() == builtin_type_kind::i64)
	{
		bz_assert(size <= std::numeric_limits<uint64_t>::max());
		index = context.create_int_cast(index, context.get_builtin_type(builtin_type_kind::i64), is_index_signed);
		context.create_array_bounds_check(
			src_tokens,
			index,
			context.create_const_u64(size),
			is_index_signed
		);
	}
	else
	{
		bz_assert(size <= std::numeric_limits<uint32_t>::max());
		index = context.create_int_cast(index, context.get_builtin_type(builtin_type_kind::i32), is_index_signed);
		context.create_array_bounds_check(
			src_tokens,
			index,
			context.create_const_u32(static_cast<uint32_t>(size)),
			is_index_signed
		);
	}

	auto const result_value = context.create_array_gep(array, index);
	context.push_rvalue_array_destruct_operation(rvalue_array_subscript.elem_destruct_op, array, result_value.get_reference());
	return result_value;
}

static expr_value generate_builtin_unary_plus(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	return generate_expr_code(expr, context, result_address);
}

static expr_value generate_builtin_unary_minus(
	ast::expression const &original_expression,
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const value = generate_expr_code(expr, context, {}).get_value(context);
	if (original_expression.paren_level < 2)
	{
		context.create_neg_check(original_expression.src_tokens, value);
	}
	return value_or_result_address(context.create_neg(value), result_address, context);
}

static expr_value generate_builtin_unary_dereference(
	ast::expression const &expr,
	codegen_context &context
)
{
	auto const ptr_value = generate_expr_code(expr, context, {}).get_value(context);
	auto const type = expr.get_expr_type();
	bz_assert(type.is_optional_pointer() || type.is<ast::ts_pointer>());
	auto const object_typespec = type.is_optional_pointer()
		? type.get_optional_pointer()
		: type.get<ast::ts_pointer>();
	auto const object_type = get_type(object_typespec, context);
	auto const result = expr_value::get_reference(ptr_value.get_value_as_instruction(context), object_type);
	return result;
}

static expr_value generate_builtin_unary_bit_not(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const value = generate_expr_code(expr, context, {}).get_value(context);
	return value_or_result_address(context.create_not(value), result_address, context);
}

static expr_value generate_builtin_unary_bool_not(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const value = generate_expr_code(expr, context, {}).get_value(context);
	return value_or_result_address(context.create_not(value), result_address, context);
}

static expr_value generate_builtin_unary_plus_plus(
	ast::expression const &original_expression,
	ast::expression const &expr,
	codegen_context &context
)
{
	auto const value_ref = generate_expr_code(expr, context, {});
	bz_assert(value_ref.is_reference());
	auto const value = value_ref.get_value(context);

	if (value.get_type()->is_pointer())
	{
		auto const expr_type = expr.get_expr_type().get_mut_reference();
		bz_assert(expr_type.is<ast::ts_pointer>() || expr_type.is_optional_pointer());
		auto const object_type = expr_type.is<ast::ts_pointer>()
			? get_type(expr_type.get<ast::ts_pointer>(), context)
			: get_type(expr_type.get_optional_pointer(), context);

		auto const intptr_type = context.get_builtin_type(context.is_64_bit() ? builtin_type_kind::i64 : builtin_type_kind::i32);
		auto const const_one = context.create_const_int(intptr_type, uint64_t(1));
		auto const incremented_value = context.create_ptr_add(
			original_expression.src_tokens,
			value,
			const_one,
			false,
			object_type,
			expr_type
		);
		context.create_store(incremented_value, value_ref);
		return value_ref;
	}
	else
	{
		bz_assert(value.get_type()->is_integer_type());
		auto const expr_type = expr.get_expr_type().get_mut_reference();
		bz_assert(expr_type.is<ast::ts_base_type>());
		auto const expr_kind = expr_type.get<ast::ts_base_type>().info->kind;
		auto const is_signed = ast::is_signed_integer_kind(expr_kind);

		auto const const_one = is_signed
			? context.create_const_int(value.get_type(), int64_t(1))
			: context.create_const_int(value.get_type(), uint64_t(1));
		if (original_expression.paren_level < 2)
		{
			context.create_add_check(original_expression.src_tokens, value, const_one, is_signed);
		}
		auto const incremented_value = context.create_add(value, const_one);
		context.create_store(incremented_value, value_ref);
		return value_ref;
	}
}

static expr_value generate_builtin_unary_minus_minus(
	ast::expression const &original_expression,
	ast::expression const &expr,
	codegen_context &context
)
{
	auto const value_ref = generate_expr_code(expr, context, {});
	bz_assert(value_ref.is_reference());
	auto const value = value_ref.get_value(context);

	if (value.get_type()->is_pointer())
	{
		auto const expr_type = expr.get_expr_type().get_mut_reference();
		bz_assert(expr_type.is<ast::ts_pointer>() || expr_type.is_optional_pointer());
		auto const object_type = expr_type.is<ast::ts_pointer>()
			? get_type(expr_type.get<ast::ts_pointer>(), context)
			: get_type(expr_type.get_optional_pointer(), context);

		auto const intptr_type = context.get_builtin_type(context.is_64_bit() ? builtin_type_kind::i64 : builtin_type_kind::i32);
		auto const const_one = context.create_const_int(intptr_type, uint64_t(1));
		auto const decremented_value = context.create_ptr_sub(
			original_expression.src_tokens,
			value,
			const_one,
			false,
			object_type,
			expr_type
		);
		context.create_store(decremented_value, value_ref);
		return value_ref;
	}
	else
	{
		bz_assert(value.get_type()->is_integer_type());
		auto const expr_type = expr.get_expr_type().get_mut_reference();
		bz_assert(expr_type.is<ast::ts_base_type>());
		auto const expr_kind = expr_type.get<ast::ts_base_type>().info->kind;
		auto const is_signed = ast::is_signed_integer_kind(expr_kind);

		auto const const_one = is_signed
			? context.create_const_int(value.get_type(), int64_t(1))
			: context.create_const_int(value.get_type(), uint64_t(1));
		if (original_expression.paren_level < 2)
		{
			context.create_sub_check(original_expression.src_tokens, value, const_one, is_signed);
		}
		auto const decremented_value = context.create_sub(value, const_one);
		context.create_store(decremented_value, value_ref);
		return value_ref;
	}
}

static expr_value generate_builtin_binary_plus(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);

	auto const lhs_type = lhs.get_expr_type();
	auto const rhs_type = rhs.get_expr_type();
	if (lhs_value.get_type()->is_pointer())
	{
		bz_assert(lhs_type.is<ast::ts_pointer>() || lhs_type.is_optional_pointer());
		auto const object_type = lhs_type.is<ast::ts_pointer>()
			? get_type(lhs_type.get<ast::ts_pointer>(), context)
			: get_type(lhs_type.get_optional_pointer(), context);

		bz_assert(rhs_type.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_type.get<ast::ts_base_type>().info->kind;

		auto const result_value = context.create_ptr_add(
			original_expression.src_tokens,
			lhs_value,
			rhs_value,
			ast::is_signed_integer_kind(rhs_kind),
			object_type,
			lhs_type
		);
		return value_or_result_address(result_value, result_address, context);
	}
	else if (rhs_value.get_type()->is_pointer())
	{
		bz_assert(lhs_type.is<ast::ts_base_type>());
		auto const lhs_kind = lhs_type.get<ast::ts_base_type>().info->kind;

		bz_assert(rhs_type.is<ast::ts_pointer>() || rhs_type.is_optional_pointer());
		auto const object_type = rhs_type.is<ast::ts_pointer>()
			? get_type(rhs_type.get<ast::ts_pointer>(), context)
			: get_type(rhs_type.get_optional_pointer(), context);

		auto const result_value = context.create_ptr_add(
			original_expression.src_tokens,
			rhs_value,
			lhs_value,
			ast::is_signed_integer_kind(lhs_kind),
			object_type,
			rhs_type
		);
		return value_or_result_address(result_value, result_address, context);
	}
	else
	{
		bz_assert(lhs_type.is<ast::ts_base_type>() && rhs_type.is<ast::ts_base_type>());
		auto const lhs_kind = lhs_type.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_type.get<ast::ts_base_type>().info->kind;

		if (lhs_kind == ast::type_info::char_)
		{
			auto const rhs_cast = context.create_int_cast(rhs_value, lhs_value.get_type(), ast::is_signed_integer_kind(rhs_kind));
			auto const result_value = context.create_add(lhs_value, rhs_cast);
			return value_or_result_address(result_value, result_address, context);
		}
		else if (rhs_kind == ast::type_info::char_)
		{
			auto const lhs_cast = context.create_int_cast(lhs_value, rhs_value.get_type(), ast::is_signed_integer_kind(lhs_kind));
			auto const result_value = context.create_add(lhs_cast, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
		else
		{
			bz_assert(lhs_kind == rhs_kind);
			if (original_expression.paren_level < 2)
			{
				context.create_add_check(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(lhs_kind));
			}
			auto const result_value = context.create_add(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
	}
}

static expr_value generate_builtin_binary_plus_eq(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const lhs_ref = generate_expr_code(lhs, context, {});
	bz_assert(lhs_ref.is_reference());
	auto const lhs_value = lhs_ref.get_value(context);

	auto const lhs_type = lhs.get_expr_type().get_mut_reference();
	auto const rhs_type = rhs.get_expr_type();
	if (lhs_value.get_type()->is_pointer())
	{
		bz_assert(lhs_type.is<ast::ts_pointer>() || lhs_type.is_optional_pointer());
		auto const object_type = lhs_type.is<ast::ts_pointer>()
			? get_type(lhs_type.get<ast::ts_pointer>(), context)
			: get_type(lhs_type.get_optional_pointer(), context);

		bz_assert(rhs_type.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_type.get<ast::ts_base_type>().info->kind;

		auto const result_value = context.create_ptr_add(
			original_expression.src_tokens,
			lhs_value,
			rhs_value,
			ast::is_signed_integer_kind(rhs_kind),
			object_type,
			lhs_type
		);
		context.create_store(result_value, lhs_ref);
		return lhs_ref;
	}
	else
	{
		bz_assert(lhs_type.is<ast::ts_base_type>() && rhs_type.is<ast::ts_base_type>());
		auto const lhs_kind = lhs_type.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_type.get<ast::ts_base_type>().info->kind;

		if (lhs_kind == ast::type_info::char_)
		{
			auto const rhs_cast = context.create_int_cast(rhs_value, lhs_value.get_type(), ast::is_signed_integer_kind(rhs_kind));
			auto const result_value = context.create_add(lhs_value, rhs_cast);
			context.create_store(result_value, lhs_ref);
			return lhs_ref;
		}
		else
		{
			bz_assert(lhs_kind == rhs_kind);
			if (original_expression.paren_level < 2)
			{
				context.create_add_check(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(lhs_kind));
			}
			auto const result_value = context.create_add(lhs_value, rhs_value);
			context.create_store(result_value, lhs_ref);
			return lhs_ref;
		}
	}
}

static expr_value generate_builtin_binary_minus(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);

	auto const lhs_type = lhs.get_expr_type();
	auto const rhs_type = rhs.get_expr_type();
	if (lhs_value.get_type()->is_pointer() && rhs_value.get_type()->is_pointer())
	{
		bz_assert(lhs_type == rhs_type);
		bz_assert(lhs_type.is<ast::ts_pointer>() || lhs_type.is_optional_pointer());
		auto const object_type = lhs_type.is<ast::ts_pointer>()
			? get_type(lhs_type.get<ast::ts_pointer>(), context)
			: get_type(lhs_type.get_optional_pointer(), context);

		auto const result_value = context.create_ptrdiff(
			original_expression.src_tokens,
			lhs_value,
			rhs_value,
			object_type,
			lhs_type
		);
		return value_or_result_address(result_value, result_address, context);
	}
	else if (lhs_value.get_type()->is_pointer())
	{
		bz_assert(lhs_type.is<ast::ts_pointer>() || lhs_type.is_optional_pointer());
		auto const object_type = lhs_type.is<ast::ts_pointer>()
			? get_type(lhs_type.get<ast::ts_pointer>(), context)
			: get_type(lhs_type.get_optional_pointer(), context);

		bz_assert(rhs_type.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_type.get<ast::ts_base_type>().info->kind;

		auto const result_value = context.create_ptr_sub(
			original_expression.src_tokens,
			lhs_value,
			rhs_value,
			ast::is_signed_integer_kind(rhs_kind),
			object_type,
			lhs_type
		);
		return value_or_result_address(result_value, result_address, context);
	}
	else
	{
		bz_assert(lhs_type.is<ast::ts_base_type>() && rhs_type.is<ast::ts_base_type>());
		auto const lhs_kind = lhs_type.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_type.get<ast::ts_base_type>().info->kind;

		if (lhs_kind == ast::type_info::char_ && rhs_kind == ast::type_info::char_)
		{
			auto const result_value = context.create_sub(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
		else if (lhs_kind == ast::type_info::char_)
		{
			auto const rhs_cast = context.create_int_cast(rhs_value, lhs_value.get_type(), ast::is_signed_integer_kind(rhs_kind));
			auto const result_value = context.create_sub(lhs_value, rhs_cast);
			return value_or_result_address(result_value, result_address, context);
		}
		else
		{
			bz_assert(lhs_kind == rhs_kind);
			if (original_expression.paren_level < 2)
			{
				context.create_sub_check(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(lhs_kind));
			}
			auto const result_value = context.create_sub(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
	}
}


static expr_value generate_builtin_binary_minus_eq(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const lhs_ref = generate_expr_code(lhs, context, {});
	bz_assert(lhs_ref.is_reference());
	auto const lhs_value = lhs_ref.get_value(context);

	auto const lhs_type = lhs.get_expr_type().get_mut_reference();
	auto const rhs_type = rhs.get_expr_type();
	if (lhs_value.get_type()->is_pointer())
	{
		bz_assert(lhs_type.is<ast::ts_pointer>() || lhs_type.is_optional_pointer());
		auto const object_type = lhs_type.is<ast::ts_pointer>()
			? get_type(lhs_type.get<ast::ts_pointer>(), context)
			: get_type(lhs_type.get_optional_pointer(), context);

		bz_assert(rhs_type.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_type.get<ast::ts_base_type>().info->kind;

		auto const result_value = context.create_ptr_sub(
			original_expression.src_tokens,
			lhs_value,
			rhs_value,
			ast::is_signed_integer_kind(rhs_kind),
			object_type,
			lhs_type
		);
		context.create_store(result_value, lhs_ref);
		return lhs_ref;
	}
	else
	{
		bz_assert(lhs_type.is<ast::ts_base_type>() && rhs_type.is<ast::ts_base_type>());
		auto const lhs_kind = lhs_type.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_type.get<ast::ts_base_type>().info->kind;

		if (lhs_kind == ast::type_info::char_)
		{
			auto const rhs_cast = context.create_int_cast(rhs_value, lhs_value.get_type(), ast::is_signed_integer_kind(rhs_kind));
			auto const result_value = context.create_sub(lhs_value, rhs_cast);
			context.create_store(result_value, lhs_ref);
			return lhs_ref;
		}
		else
		{
			bz_assert(lhs_kind == rhs_kind);
			if (original_expression.paren_level < 2)
			{
				context.create_sub_check(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(lhs_kind));
			}
			auto const result_value = context.create_sub(lhs_value, rhs_value);
			context.create_store(result_value, lhs_ref);
			return lhs_ref;
		}
	}
}


static expr_value generate_builtin_binary_multiply(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);

	if (original_expression.paren_level < 2)
	{
		bz_assert(lhs.get_expr_type().is<ast::ts_base_type>());
		auto const type_kind = lhs.get_expr_type().get<ast::ts_base_type>().info->kind;
		context.create_mul_check(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(type_kind));
	}

	auto const result_value = context.create_mul(lhs_value, rhs_value);
	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_builtin_binary_multiply_eq(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const lhs_ref = generate_expr_code(lhs, context, {});
	bz_assert(lhs_ref.is_reference());
	auto const lhs_value = lhs_ref.get_value(context);

	if (original_expression.paren_level < 2)
	{
		bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
		auto const type_kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;
		context.create_mul_check(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(type_kind));
	}

	auto const result_value = context.create_mul(lhs_value, rhs_value);
	context.create_store(result_value, lhs_ref);
	return lhs_ref;
}

static expr_value generate_builtin_binary_divide(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);

	bz_assert(lhs.get_expr_type().is<ast::ts_base_type>());
	auto const type_kind = lhs.get_expr_type().get<ast::ts_base_type>().info->kind;
	if (original_expression.paren_level < 2)
	{
		context.create_div_check(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(type_kind));
	}

	auto const result_value = context.create_div(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(type_kind));
	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_builtin_binary_divide_eq(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const lhs_ref = generate_expr_code(lhs, context, {});
	bz_assert(lhs_ref.is_reference());
	auto const lhs_value = lhs_ref.get_value(context);

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const type_kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;
	if (original_expression.paren_level < 2)
	{
		context.create_div_check(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(type_kind));
	}

	auto const result_value = context.create_div(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(type_kind));
	context.create_store(result_value, lhs_ref);
	return lhs_ref;
}

static expr_value generate_builtin_binary_modulo(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);

	bz_assert(lhs.get_expr_type().is<ast::ts_base_type>());
	auto const type_kind = lhs.get_expr_type().get<ast::ts_base_type>().info->kind;

	auto const result_value = context.create_rem(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(type_kind));
	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_builtin_binary_modulo_eq(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const lhs_ref = generate_expr_code(lhs, context, {});
	bz_assert(lhs_ref.is_reference());
	auto const lhs_value = lhs_ref.get_value(context);

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const type_kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;

	auto const result_value = context.create_rem(original_expression.src_tokens, lhs_value, rhs_value, ast::is_signed_integer_kind(type_kind));
	context.create_store(result_value, lhs_ref);
	return lhs_ref;
}

static expr_value generate_builtin_binary_equals(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_t = lhs.get_expr_type().remove_reference();
	auto const rhs_t = rhs.get_expr_type().remove_reference();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		bz_assert(lhs_kind != ast::type_info::str_);
		if (ast::is_integer_kind(lhs_kind) || lhs_kind == ast::type_info::char_)
		{
			auto const result_value = context.create_int_cmp_eq(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
		else
		{
			if (original_expression.paren_level < 2)
			{
				context.create_float_cmp_eq_check(original_expression.src_tokens, lhs_value, rhs_value);
			}
			auto const result_value = context.create_float_cmp_eq(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
	}
	else if (lhs_t.is<ast::ts_enum>() && rhs_t.is<ast::ts_enum>())
	{
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		auto const result_value = context.create_int_cmp_eq(lhs_value, rhs_value);
		return value_or_result_address(result_value, result_address, context);
	}
	else if (
		(lhs_t.is<ast::ts_optional>() && rhs_t.is<ast::ts_base_type>())
		|| (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_optional>())
	)
	{
		auto const lhs_value = generate_expr_code(lhs, context, {});
		auto const rhs_value = generate_expr_code(rhs, context, {});
		auto const &optional_value = lhs_t.is<ast::ts_optional>() ? lhs_value : rhs_value;
		auto const has_value = get_optional_has_value(optional_value, context);
		auto const result_value = context.create_not(has_value);
		return value_or_result_address(result_value, result_address, context);
	}
	else // if pointer or function
	{
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		auto const result_value = context.create_pointer_cmp_eq(lhs_value, rhs_value);
		return value_or_result_address(result_value, result_address, context);
	}
}

static expr_value generate_builtin_binary_not_equals(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_t = lhs.get_expr_type().remove_reference();
	auto const rhs_t = rhs.get_expr_type().remove_reference();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		bz_assert(lhs_kind != ast::type_info::str_);
		if (ast::is_integer_kind(lhs_kind) || lhs_kind == ast::type_info::char_)
		{
			auto const result_value = context.create_int_cmp_neq(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
		else
		{
			if (original_expression.paren_level < 2)
			{
				context.create_float_cmp_neq_check(original_expression.src_tokens, lhs_value, rhs_value);
			}
			auto const result_value = context.create_float_cmp_neq(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
	}
	else if (lhs_t.is<ast::ts_enum>() && rhs_t.is<ast::ts_enum>())
	{
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		auto const result_value = context.create_int_cmp_neq(lhs_value, rhs_value);
		return value_or_result_address(result_value, result_address, context);
	}
	else if (
		(lhs_t.is<ast::ts_optional>() && rhs_t.is<ast::ts_base_type>())
		|| (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_optional>())
	)
	{
		auto const lhs_value = generate_expr_code(lhs, context, {});
		auto const rhs_value = generate_expr_code(rhs, context, {});
		auto const &optional_value = lhs_t.is<ast::ts_optional>() ? lhs_value : rhs_value;
		auto const result_value = get_optional_has_value(optional_value, context);
		return value_or_result_address(result_value, result_address, context);
	}
	else // if pointer or function
	{
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		auto const result_value = context.create_pointer_cmp_neq(lhs_value, rhs_value);
		return value_or_result_address(result_value, result_address, context);
	}
}

static expr_value generate_builtin_binary_less_than(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_t = lhs.get_expr_type();
	auto const rhs_t = rhs.get_expr_type();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		bz_assert(lhs_kind != ast::type_info::str_);
		if (ast::is_integer_kind(lhs_kind) || lhs_kind == ast::type_info::char_)
		{
			auto const result_value = context.create_int_cmp_lt(lhs_value, rhs_value, ast::is_signed_integer_kind(lhs_kind));
			return value_or_result_address(result_value, result_address, context);
		}
		else
		{
			if (original_expression.paren_level < 2)
			{
				context.create_float_cmp_lt_check(original_expression.src_tokens, lhs_value, rhs_value);
			}
			auto const result_value = context.create_float_cmp_lt(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
	}
	else // if pointer
	{
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		auto const result_value = context.create_pointer_cmp_lt(original_expression.src_tokens, lhs_value, rhs_value);
		return value_or_result_address(result_value, result_address, context);
	}
}

static expr_value generate_builtin_binary_less_than_eq(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_t = lhs.get_expr_type();
	auto const rhs_t = rhs.get_expr_type();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		bz_assert(lhs_kind != ast::type_info::str_);
		if (ast::is_integer_kind(lhs_kind) || lhs_kind == ast::type_info::char_)
		{
			auto const result_value = context.create_int_cmp_lte(lhs_value, rhs_value, ast::is_signed_integer_kind(lhs_kind));
			return value_or_result_address(result_value, result_address, context);
		}
		else
		{
			if (original_expression.paren_level < 2)
			{
				context.create_float_cmp_lte_check(original_expression.src_tokens, lhs_value, rhs_value);
			}
			auto const result_value = context.create_float_cmp_lte(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
	}
	else // if pointer
	{
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		auto const result_value = context.create_pointer_cmp_lte(original_expression.src_tokens, lhs_value, rhs_value);
		return value_or_result_address(result_value, result_address, context);
	}
}

static expr_value generate_builtin_binary_greater_than(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_t = lhs.get_expr_type();
	auto const rhs_t = rhs.get_expr_type();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		bz_assert(lhs_kind != ast::type_info::str_);
		if (ast::is_integer_kind(lhs_kind) || lhs_kind == ast::type_info::char_)
		{
			auto const result_value = context.create_int_cmp_gt(lhs_value, rhs_value, ast::is_signed_integer_kind(lhs_kind));
			return value_or_result_address(result_value, result_address, context);
		}
		else
		{
			if (original_expression.paren_level < 2)
			{
				context.create_float_cmp_gt_check(original_expression.src_tokens, lhs_value, rhs_value);
			}
			auto const result_value = context.create_float_cmp_gt(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
	}
	else // if pointer
	{
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		auto const result_value = context.create_pointer_cmp_gt(original_expression.src_tokens, lhs_value, rhs_value);
		return value_or_result_address(result_value, result_address, context);
	}
}

static expr_value generate_builtin_binary_greater_than_eq(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_t = lhs.get_expr_type();
	auto const rhs_t = rhs.get_expr_type();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		bz_assert(lhs_kind != ast::type_info::str_);
		if (ast::is_integer_kind(lhs_kind) || lhs_kind == ast::type_info::char_)
		{
			auto const result_value = context.create_int_cmp_gte(lhs_value, rhs_value, ast::is_signed_integer_kind(lhs_kind));
			return value_or_result_address(result_value, result_address, context);
		}
		else
		{
			if (original_expression.paren_level < 2)
			{
				context.create_float_cmp_gte_check(original_expression.src_tokens, lhs_value, rhs_value);
			}
			auto const result_value = context.create_float_cmp_gte(lhs_value, rhs_value);
			return value_or_result_address(result_value, result_address, context);
		}
	}
	else // if pointer
	{
		auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
		auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
		auto const result_value = context.create_pointer_cmp_gte(original_expression.src_tokens, lhs_value, rhs_value);
		return value_or_result_address(result_value, result_address, context);
	}
}

static expr_value generate_builtin_binary_bit_and(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);

	auto const result_value = context.create_and(lhs_value, rhs_value);
	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_builtin_binary_bit_and_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const lhs_ref = generate_expr_code(lhs, context, {});
	bz_assert(lhs_ref.is_reference());
	auto const lhs_value = lhs_ref.get_value(context);

	auto const result_value = context.create_and(lhs_value, rhs_value);
	context.create_store(result_value, lhs_ref);
	return lhs_ref;
}

static expr_value generate_builtin_binary_bit_xor(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);

	auto const result_value = context.create_xor(lhs_value, rhs_value);
	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_builtin_binary_bit_xor_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const lhs_ref = generate_expr_code(lhs, context, {});
	bz_assert(lhs_ref.is_reference());
	auto const lhs_value = lhs_ref.get_value(context);

	auto const result_value = context.create_xor(lhs_value, rhs_value);
	context.create_store(result_value, lhs_ref);
	return lhs_ref;
}

static expr_value generate_builtin_binary_bit_or(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);

	auto const result_value = context.create_or(lhs_value, rhs_value);
	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_builtin_binary_bit_or_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const lhs_ref = generate_expr_code(lhs, context, {}).get_value(context);
	bz_assert(lhs_ref.is_reference());
	auto const lhs_value = lhs_ref.get_value(context);

	auto const result_value = context.create_or(lhs_value, rhs_value);
	context.create_store(result_value, lhs_ref);
	return lhs_ref;
}

static expr_value generate_builtin_binary_bit_left_shift(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const rhs_kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;

	auto const result_value = context.create_shl(
		original_expression.src_tokens,
		lhs_value,
		rhs_value,
		ast::is_signed_integer_kind(rhs_kind)
	);
	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_builtin_binary_bit_left_shift_eq(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const lhs_ref = generate_expr_code(lhs, context, {});
	bz_assert(lhs_ref.is_reference());
	auto const lhs_value = lhs_ref.get_value(context);

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const rhs_kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;

	auto const result_value = context.create_shl(
		original_expression.src_tokens,
		lhs_value,
		rhs_value,
		ast::is_signed_integer_kind(rhs_kind)
	);
	context.create_store(result_value, lhs_ref);
	return lhs_ref;
}

static expr_value generate_builtin_binary_bit_right_shift(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const lhs_value = generate_expr_code(lhs, context, {}).get_value(context);
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const rhs_kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;

	auto const result_value = context.create_shr(
		original_expression.src_tokens,
		lhs_value,
		rhs_value,
		ast::is_signed_integer_kind(rhs_kind)
	);
	return value_or_result_address(result_value, result_address, context);
}

static expr_value generate_builtin_binary_bit_right_shift_eq(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expr_code(rhs, context, {}).get_value(context);
	auto const lhs_ref = generate_expr_code(lhs, context, {});
	bz_assert(lhs_ref.is_reference());
	auto const lhs_value = lhs_ref.get_value(context);

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const rhs_kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;

	auto const result_value = context.create_shr(
		original_expression.src_tokens,
		lhs_value,
		rhs_value,
		ast::is_signed_integer_kind(rhs_kind)
	);
	context.create_store(result_value, lhs_ref);
	return lhs_ref;
}

enum class range_kind
{
	regular,
	from,
	to,
	unbounded,
};

static range_kind range_kind_from_name(bz::u8string_view struct_name)
{
	if (struct_name == "__integer_range")
	{
		return range_kind::regular;
	}
	else if (struct_name == "__integer_range_from")
	{
		return range_kind::from;
	}
	else if (struct_name == "__integer_range_to")
	{
		return range_kind::to;
	}
	else if (struct_name == "__range_unbounded")
	{
		return range_kind::unbounded;
	}
	else
	{
		bz_unreachable;
	}
}

static expr_value generate_builtin_subscript_range(
	ast::expression const &original_expression,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	if (!result_address.has_value())
	{
		result_address = context.create_alloca(original_expression.src_tokens, context.get_slice_t());
	}
	auto const &result_value = result_address.get();

	auto const lhs_type = lhs.get_expr_type().remove_mut_reference();
	auto const rhs_type = rhs.get_expr_type();
	auto const lhs_value = generate_expr_code(lhs, context, {});
	auto const rhs_value = generate_expr_code(rhs, context, {});

	bz_assert(rhs_type.is<ast::ts_base_type>());
	bz_assert(rhs_type.get<ast::ts_base_type>().info->type_name.values.size() == 1);
	auto const kind = range_kind_from_name(rhs_type.get<ast::ts_base_type>().info->type_name.values[0]);

	auto const is_index_signed = [&]() {
		if (kind == range_kind::unbounded)
		{
			return false;
		}

		bz_assert(rhs_type.is<ast::ts_base_type>());
		bz_assert(rhs_type.get<ast::ts_base_type>().info->is_generic_instantiation());
		bz_assert(rhs_type.get<ast::ts_base_type>().info->generic_parameters.size() == 1);
		bz_assert(rhs_type.get<ast::ts_base_type>().info->generic_parameters[0].init_expr.is_typename());
		auto const &index_type = rhs_type.get<ast::ts_base_type>().info->generic_parameters[0].init_expr.get_typename();
		bz_assert(index_type.is<ast::ts_base_type>());
		bz_assert(ast::is_integer_kind(index_type.get<ast::ts_base_type>().info->kind));
		return ast::is_signed_integer_kind(index_type.get<ast::ts_base_type>().info->kind);
	}();

	struct begin_end_pair_t
	{
		expr_value begin;
		expr_value end;
	};

	auto const [begin_index, end_index] = [&]() -> begin_end_pair_t {
		switch (kind)
		{
		case range_kind::regular:
			bz_assert(rhs_value.get_type()->get_aggregate_types().size() == 2);
			return {
				.begin = context.create_struct_gep(rhs_value, 0).get_value(context),
				.end   = context.create_struct_gep(rhs_value, 1).get_value(context),
			};
		case range_kind::from:
			bz_assert(rhs_value.get_type()->get_aggregate_types().size() == 1);
			return {
				.begin = context.create_struct_gep(rhs_value, 0).get_value(context),
				.end   = expr_value::get_none(),
			};
		case range_kind::to:
			bz_assert(rhs_value.get_type()->get_aggregate_types().size() == 1);
			return {
				.begin = expr_value::get_none(),
				.end   = context.create_struct_gep(rhs_value, 0).get_value(context),
			};
		case range_kind::unbounded:
			return {
				.begin = expr_value::get_none(),
				.end   = expr_value::get_none(),
			};
		}
	}();

	struct bounds_check_info_t
	{
		expr_value begin_index_cast;
		expr_value end_index_cast;
		expr_value size_cast;
	};

	auto const get_bounds_check_info = [&](expr_value size) -> bounds_check_info_t {
		switch (kind)
		{
		case range_kind::regular:
			if (context.is_64_bit() || begin_index.get_type()->get_builtin_kind() == builtin_type_kind::i64)
			{
				auto const i64_type = context.get_builtin_type(builtin_type_kind::i64);
				return {
					.begin_index_cast = context.create_int_cast(begin_index, i64_type, is_index_signed),
					.end_index_cast = context.create_int_cast(end_index, i64_type, is_index_signed),
					.size_cast = size.get_type() != i64_type
						? context.create_int_cast(size, i64_type, false)
						: size,
				};
			}
			else
			{
				auto const i32_type = context.get_builtin_type(builtin_type_kind::i32);
				bz_assert(size.get_type() == i32_type);
				return {
					.begin_index_cast = context.create_int_cast(begin_index, i32_type, is_index_signed),
					.end_index_cast = context.create_int_cast(end_index, i32_type, is_index_signed),
					.size_cast = size,
				};
			}
		case range_kind::from:
			if (context.is_64_bit() || begin_index.get_type()->get_builtin_kind() == builtin_type_kind::i64)
			{
				auto const i64_type = context.get_builtin_type(builtin_type_kind::i64);
				return {
					.begin_index_cast = context.create_int_cast(begin_index, i64_type, is_index_signed),
					.end_index_cast = expr_value::get_none(),
					.size_cast = size.get_type() != i64_type
						? context.create_int_cast(size, i64_type, false)
						: size,
				};
			}
			else
			{
				auto const i32_type = context.get_builtin_type(builtin_type_kind::i32);
				bz_assert(size.get_type() == i32_type);
				return {
					.begin_index_cast = context.create_int_cast(begin_index, i32_type, is_index_signed),
					.end_index_cast = expr_value::get_none(),
					.size_cast = size,
				};
			}
		case range_kind::to:
			if (context.is_64_bit() || end_index.get_type()->get_builtin_kind() == builtin_type_kind::i64)
			{
				auto const i64_type = context.get_builtin_type(builtin_type_kind::i64);
				return {
					.begin_index_cast = expr_value::get_none(),
					.end_index_cast = context.create_int_cast(end_index, i64_type, is_index_signed),
					.size_cast = size.get_type() != i64_type
						? context.create_int_cast(size, i64_type, false)
						: size,
				};
			}
			else
			{
				auto const i32_type = context.get_builtin_type(builtin_type_kind::i32);
				bz_assert(size.get_type() == i32_type);
				return {
					.begin_index_cast = expr_value::get_none(),
					.end_index_cast = context.create_int_cast(end_index, i32_type, is_index_signed),
					.size_cast = size,
				};
			}
		case range_kind::unbounded:
			return {
				.begin_index_cast = expr_value::get_none(),
				.end_index_cast = expr_value::get_none(),
				.size_cast = expr_value::get_none(),
			};
		}
	};

	if (lhs_type.is<ast::ts_array_slice>())
	{
		auto const elem_type = get_type(lhs_type.get<ast::ts_array_slice>().elem_type, context);
		auto const lhs_begin_ptr = context.create_struct_gep(lhs_value, 0).get_value(context);
		auto const lhs_end_ptr   = context.create_struct_gep(lhs_value, 1).get_value(context);
		auto const size = context.create_ptrdiff_unchecked(lhs_end_ptr, lhs_begin_ptr, elem_type);

		// bounds check
		auto const [begin_index_cast, end_index_cast, size_cast] = get_bounds_check_info(size);
		switch (kind)
		{
		case range_kind::regular:
			context.create_array_range_bounds_check(original_expression.src_tokens, begin_index_cast, end_index_cast, size_cast, is_index_signed);
			break;
		case range_kind::from:
			context.create_array_range_begin_bounds_check(original_expression.src_tokens, begin_index_cast, size_cast, is_index_signed);
			break;
		case range_kind::to:
			context.create_array_range_end_bounds_check(original_expression.src_tokens, end_index_cast, size_cast, is_index_signed);
			break;
		case range_kind::unbounded:
			break;
		}

		auto const [begin_ptr, end_ptr] = [&]() -> begin_end_pair_t {
			auto const pointer_type = context.get_pointer_type();
			switch (kind)
			{
			case range_kind::regular:
			{
				auto const begin_ptr = context.create_array_slice_gep(lhs_begin_ptr, begin_index, elem_type).get_reference();
				auto const end_ptr   = context.create_array_slice_gep(lhs_begin_ptr,   end_index, elem_type).get_reference();
				return {
					.begin = expr_value::get_value(begin_ptr, pointer_type),
					.end   = expr_value::get_value(end_ptr,   pointer_type),
				};
			}
			case range_kind::from:
			{
				auto const begin_ptr = context.create_array_slice_gep(lhs_begin_ptr, begin_index, elem_type).get_reference();
				return {
					.begin = expr_value::get_value(begin_ptr, pointer_type),
					.end   = lhs_end_ptr,
				};
			}
			case range_kind::to:
			{
				auto const end_ptr = context.create_array_slice_gep(lhs_begin_ptr, end_index, elem_type).get_reference();
				return {
					.begin = lhs_begin_ptr,
					.end   = expr_value::get_value(end_ptr, pointer_type),
				};
			}
			case range_kind::unbounded:
				return {
					.begin = lhs_begin_ptr,
					.end   = lhs_end_ptr,
				};
			}
		}();

		context.create_store(begin_ptr, context.create_struct_gep(result_value, 0));
		context.create_store(end_ptr,   context.create_struct_gep(result_value, 1));
	}
	else if (lhs_type.is<ast::ts_array>())
	{
		auto const size = lhs_value.get_type()->get_array_size();
		auto const index_type_kind = [&]() {
			if (!begin_index.is_none())
			{
				return begin_index.get_type()->get_builtin_kind();
			}
			else if (!end_index.is_none())
			{
				return end_index.get_type()->get_builtin_kind();
			}
			else
			{
				return builtin_type_kind::i32;
			}
		}();
		auto const size_value = [&]() {
			if (context.is_64_bit() || index_type_kind == builtin_type_kind::i64)
			{
				bz_assert(size <= std::numeric_limits<uint64_t>::max());
				return context.create_const_u64(static_cast<uint64_t>(size));
			}
			else
			{
				bz_assert(size <= std::numeric_limits<uint32_t>::max());
				return context.create_const_u32(static_cast<uint32_t>(size));
			}
		}();

		// bounds check
		auto const [begin_index_cast, end_index_cast, size_cast] = get_bounds_check_info(size_value);
		switch (kind)
		{
		case range_kind::regular:
			context.create_array_range_bounds_check(original_expression.src_tokens, begin_index_cast, end_index_cast, size_cast, is_index_signed);
			break;
		case range_kind::from:
			context.create_array_range_begin_bounds_check(original_expression.src_tokens, begin_index_cast, size_cast, is_index_signed);
			break;
		case range_kind::to:
			context.create_array_range_end_bounds_check(original_expression.src_tokens, end_index_cast, size_cast, is_index_signed);
			break;
		case range_kind::unbounded:
			break;
		}

		auto const [begin_ptr, end_ptr] = [&]() -> begin_end_pair_t {
			auto const pointer_type = context.get_pointer_type();
			switch (kind)
			{
			case range_kind::regular:
			{
				auto const begin_ptr = context.create_array_gep(lhs_value, begin_index).get_reference();
				auto const end_ptr   = context.create_array_gep(lhs_value, end_index).get_reference();
				return {
					.begin = expr_value::get_value(begin_ptr, pointer_type),
					.end   = expr_value::get_value(end_ptr,   pointer_type),
				};
			}
			case range_kind::from:
			{
				auto const begin_ptr = context.create_array_gep(lhs_value, begin_index).get_reference();
				auto const end_ptr   = context.create_struct_gep(lhs_value, size).get_reference();
				return {
					.begin = expr_value::get_value(begin_ptr, pointer_type),
					.end   = expr_value::get_value(end_ptr,   pointer_type),
				};
			}
			case range_kind::to:
			{
				auto const begin_ptr = context.create_struct_gep(lhs_value, 0).get_reference();
				auto const end_ptr   = context.create_array_gep(lhs_value, end_index).get_reference();
				return {
					.begin = expr_value::get_value(begin_ptr, pointer_type),
					.end   = expr_value::get_value(end_ptr,   pointer_type),
				};
			}
			case range_kind::unbounded:
			{
				auto const begin_ptr = context.create_struct_gep(lhs_value, 0).get_reference();
				auto const end_ptr   = context.create_struct_gep(lhs_value, size).get_reference();
				return {
					.begin = expr_value::get_value(begin_ptr, pointer_type),
					.end   = expr_value::get_value(end_ptr,   pointer_type),
				};
			}
			}
		}();

		context.create_store(begin_ptr, context.create_struct_gep(result_value, 0));
		context.create_store(end_ptr,   context.create_struct_gep(result_value, 1));
	}
	else
	{
		bz_unreachable;
	}

	context.create_start_lifetime(result_value);
	return result_value;
}

static expr_value generate_intrinsic_function_call_code(
	ast::expression const &original_expression,
	ast::expr_function_call const &func_call,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	switch (func_call.func_body->intrinsic_kind)
	{
	static_assert(ast::function_body::_builtin_last - ast::function_body::_builtin_first == 269);
	static_assert(ast::function_body::_builtin_default_constructor_last - ast::function_body::_builtin_default_constructor_first == 14);
	static_assert(ast::function_body::_builtin_unary_operator_last - ast::function_body::_builtin_unary_operator_first == 7);
	static_assert(ast::function_body::_builtin_binary_operator_last - ast::function_body::_builtin_binary_operator_first == 28);
	case ast::function_body::builtin_str_length:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_str_starts_with:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_str_ends_with:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_str_begin_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const s = generate_expr_code(func_call.params[0], context, {});
		auto const result_value = context.create_struct_gep(s, 0).get_value(context);
		return value_or_result_address(result_value, result_address, context);
	}
	case ast::function_body::builtin_str_end_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const s = generate_expr_code(func_call.params[0], context, {});
		auto const result_value = context.create_struct_gep(s, 1).get_value(context);
		return value_or_result_address(result_value, result_address, context);
	}
	case ast::function_body::builtin_str_size:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_str_from_ptrs:
	{
		bz_assert(func_call.params.size() == 2);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, context.get_str_t());
		}
		auto const &result_value = result_address.get();

		auto const begin_ptr = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const end_ptr   = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		context.create_str_construction_check(func_call.src_tokens, begin_ptr, end_ptr);
		context.create_store(begin_ptr, context.create_struct_gep(result_value, 0));
		context.create_store(end_ptr,   context.create_struct_gep(result_value, 1));
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_slice_begin_ptr:
	case ast::function_body::builtin_slice_begin_mut_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const slice = generate_expr_code(func_call.params[0], context, {});
		auto const result_value = context.create_struct_gep(slice, 0).get_value(context);
		return value_or_result_address(result_value, result_address, context);
	}
	case ast::function_body::builtin_slice_end_ptr:
	case ast::function_body::builtin_slice_end_mut_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const slice = generate_expr_code(func_call.params[0], context, {});
		auto const result_value = context.create_struct_gep(slice, 1).get_value(context);
		return value_or_result_address(result_value, result_address, context);
	}
	case ast::function_body::builtin_slice_size:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_slice_from_ptrs:
	case ast::function_body::builtin_slice_from_mut_ptrs:
	{
		bz_assert(func_call.params.size() == 2);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, context.get_slice_t());
		}
		auto const &result_value = result_address.get();

		bz_assert(func_call.func_body->return_type.is<ast::ts_array_slice>());
		auto const elem_type = get_type(func_call.func_body->return_type.get<ast::ts_array_slice>().elem_type, context);
		auto const begin_ptr = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const end_ptr   = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		context.create_slice_construction_check(func_call.src_tokens, begin_ptr, end_ptr, elem_type, func_call.func_body->return_type);
		context.create_store(begin_ptr, context.create_struct_gep(result_value, 0));
		context.create_store(end_ptr,   context.create_struct_gep(result_value, 1));
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_array_begin_ptr:
	case ast::function_body::builtin_array_begin_mut_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const array = generate_expr_code(func_call.params[0], context, {});
		bz_assert(array.get_type()->is_array());
		auto const result_value = expr_value::get_value(
			context.create_struct_gep(array, 0).get_reference(),
			context.get_pointer_type()
		);
		return value_or_result_address(result_value, result_address, context);
	}
	case ast::function_body::builtin_array_end_ptr:
	case ast::function_body::builtin_array_end_mut_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const array = generate_expr_code(func_call.params[0], context, {});
		bz_assert(array.get_type()->is_array());
		auto const result_value = expr_value::get_value(
			context.create_struct_gep(array, array.get_type()->get_array_size()).get_reference(),
			context.get_pointer_type()
		);
		return value_or_result_address(result_value, result_address, context);
	}
	case ast::function_body::builtin_array_size:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::builtin_integer_range_i8:
	case ast::function_body::builtin_integer_range_i16:
	case ast::function_body::builtin_integer_range_i32:
	case ast::function_body::builtin_integer_range_i64:
	case ast::function_body::builtin_integer_range_u8:
	case ast::function_body::builtin_integer_range_u16:
	case ast::function_body::builtin_integer_range_u32:
	case ast::function_body::builtin_integer_range_u64:
	case ast::function_body::builtin_integer_range_inclusive_i8:
	case ast::function_body::builtin_integer_range_inclusive_i16:
	case ast::function_body::builtin_integer_range_inclusive_i32:
	case ast::function_body::builtin_integer_range_inclusive_i64:
	case ast::function_body::builtin_integer_range_inclusive_u8:
	case ast::function_body::builtin_integer_range_inclusive_u16:
	case ast::function_body::builtin_integer_range_inclusive_u32:
	case ast::function_body::builtin_integer_range_inclusive_u64:
	{
		bz_assert(func_call.params.size() == 2);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, get_type(func_call.func_body->return_type, context));
		}
		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type()->is_aggregate());
		bz_assert(result_value.get_type()->get_aggregate_types().size() == 2);

		auto const begin = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const end   = generate_expr_code(func_call.params[1], context, {}).get_value(context);

		auto const is_signed = [&]() {
			switch (func_call.func_body->intrinsic_kind)
			{
			case ast::function_body::builtin_integer_range_i8:
			case ast::function_body::builtin_integer_range_i16:
			case ast::function_body::builtin_integer_range_i32:
			case ast::function_body::builtin_integer_range_i64:
			case ast::function_body::builtin_integer_range_inclusive_i8:
			case ast::function_body::builtin_integer_range_inclusive_i16:
			case ast::function_body::builtin_integer_range_inclusive_i32:
			case ast::function_body::builtin_integer_range_inclusive_i64:
				return true;
			default:
				return false;
			}
		}();
		context.create_range_bounds_check(func_call.src_tokens, begin, end, is_signed);

		context.create_store(begin, context.create_struct_gep(result_value, 0));
		context.create_store(end,   context.create_struct_gep(result_value, 1));
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_integer_range_from_i8:
	case ast::function_body::builtin_integer_range_from_i16:
	case ast::function_body::builtin_integer_range_from_i32:
	case ast::function_body::builtin_integer_range_from_i64:
	case ast::function_body::builtin_integer_range_from_u8:
	case ast::function_body::builtin_integer_range_from_u16:
	case ast::function_body::builtin_integer_range_from_u32:
	case ast::function_body::builtin_integer_range_from_u64:
	{
		bz_assert(func_call.params.size() == 1);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, get_type(func_call.func_body->return_type, context));
		}
		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type()->is_aggregate());
		bz_assert(result_value.get_type()->get_aggregate_types().size() == 1);

		auto const begin = generate_expr_code(func_call.params[0], context, {}).get_value(context);

		context.create_store(begin, context.create_struct_gep(result_value, 0));
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_integer_range_to_i8:
	case ast::function_body::builtin_integer_range_to_i16:
	case ast::function_body::builtin_integer_range_to_i32:
	case ast::function_body::builtin_integer_range_to_i64:
	case ast::function_body::builtin_integer_range_to_u8:
	case ast::function_body::builtin_integer_range_to_u16:
	case ast::function_body::builtin_integer_range_to_u32:
	case ast::function_body::builtin_integer_range_to_u64:
	case ast::function_body::builtin_integer_range_to_inclusive_i8:
	case ast::function_body::builtin_integer_range_to_inclusive_i16:
	case ast::function_body::builtin_integer_range_to_inclusive_i32:
	case ast::function_body::builtin_integer_range_to_inclusive_i64:
	case ast::function_body::builtin_integer_range_to_inclusive_u8:
	case ast::function_body::builtin_integer_range_to_inclusive_u16:
	case ast::function_body::builtin_integer_range_to_inclusive_u32:
	case ast::function_body::builtin_integer_range_to_inclusive_u64:
	{
		bz_assert(func_call.params.size() == 1);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, get_type(func_call.func_body->return_type, context));
		}
		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type()->is_aggregate());
		bz_assert(result_value.get_type()->get_aggregate_types().size() == 1);

		auto const end = generate_expr_code(func_call.params[0], context, {}).get_value(context);

		context.create_store(end, context.create_struct_gep(result_value, 0));
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_range_unbounded:
	{
		bz_assert(func_call.params.empty());
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, get_type(func_call.func_body->return_type, context));
		}
		auto const &result_value = result_address.get();
		context.create_const_memset_zero(result_value);
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_integer_range_begin_value:
	case ast::function_body::builtin_integer_range_inclusive_begin_value:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expr_code(func_call.params[0], context, {});
		auto const begin_value = context.create_struct_gep(range_value, 0);
		return value_or_result_address(begin_value, result_address, context);
	}
	case ast::function_body::builtin_integer_range_end_value:
	case ast::function_body::builtin_integer_range_inclusive_end_value:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expr_code(func_call.params[0], context, {});
		auto const end_value = context.create_struct_gep(range_value, 1);
		return value_or_result_address(end_value, result_address, context);
	}
	case ast::function_body::builtin_integer_range_from_begin_value:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expr_code(func_call.params[0], context, {});
		auto const begin_value = context.create_struct_gep(range_value, 0);
		return value_or_result_address(begin_value, result_address, context);
	}
	case ast::function_body::builtin_integer_range_to_end_value:
	case ast::function_body::builtin_integer_range_to_inclusive_end_value:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expr_code(func_call.params[0], context, {});
		auto const end_value = context.create_struct_gep(range_value, 0);
		return value_or_result_address(end_value, result_address, context);
	}
	case ast::function_body::builtin_integer_range_begin_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, get_type(func_call.func_body->return_type, context));
		}
		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type()->is_aggregate());
		bz_assert(result_value.get_type()->get_aggregate_types().size() == 1);

		auto const range_value = generate_expr_code(func_call.params[0], context, {});
		auto const begin_value = context.create_struct_gep(range_value, 0);

		context.create_store(begin_value, context.create_struct_gep(result_value, 0));
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_integer_range_end_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, get_type(func_call.func_body->return_type, context));
		}
		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type()->is_aggregate());
		bz_assert(result_value.get_type()->get_aggregate_types().size() == 1);

		auto const range_value = generate_expr_code(func_call.params[0], context, {});
		auto const end_value = context.create_struct_gep(range_value, 1);

		context.create_store(end_value, context.create_struct_gep(result_value, 0));
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_integer_range_iterator_dereference:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expr_code(func_call.params[0], context, {});
		auto const integer_value = context.create_struct_gep(it_value, 0);
		return value_or_result_address(integer_value, result_address, context);
	}
	case ast::function_body::builtin_integer_range_iterator_equals:
	{
		bz_assert(func_call.params.size() == 2);
		auto const lhs_it_value = generate_expr_code(func_call.params[0], context, {});
		auto const rhs_it_value = generate_expr_code(func_call.params[1], context, {});
		auto const lhs_integer_value = context.create_struct_gep(lhs_it_value, 0);
		auto const rhs_integer_value = context.create_struct_gep(rhs_it_value, 0);
		auto const result = context.create_int_cmp_eq(lhs_integer_value, rhs_integer_value);
		return value_or_result_address(result, result_address, context);
	}
	case ast::function_body::builtin_integer_range_iterator_not_equals:
	{
		bz_assert(func_call.params.size() == 2);
		auto const lhs_it_value = generate_expr_code(func_call.params[0], context, {});
		auto const rhs_it_value = generate_expr_code(func_call.params[1], context, {});
		auto const lhs_integer_value = context.create_struct_gep(lhs_it_value, 0);
		auto const rhs_integer_value = context.create_struct_gep(rhs_it_value, 0);
		auto const result = context.create_int_cmp_neq(lhs_integer_value, rhs_integer_value);
		return value_or_result_address(result, result_address, context);
	}
	case ast::function_body::builtin_integer_range_iterator_plus_plus:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expr_code(func_call.params[0], context, {});
		bz_assert(it_value.is_reference());
		auto const integer_value_ref = context.create_struct_gep(it_value, 0);
		bz_assert(func_call.params[0].get_expr_type().get_mut_reference().is<ast::ts_base_type>());
		auto const it_type_info = func_call.params[0].get_expr_type().remove_mut_reference().get<ast::ts_base_type>().info;
		bz_assert(it_type_info->generic_parameters.size() == 1);
		bz_assert(it_type_info->generic_parameters[0].init_expr.is_typename());
		auto const &it_integer_type = it_type_info->generic_parameters[0].init_expr.get_typename();
		bz_assert(it_integer_type.is<ast::ts_base_type>());
		auto const is_signed = ast::is_signed_integer_kind(it_integer_type.get<ast::ts_base_type>().info->kind);
		auto const one_value = is_signed
			? context.create_const_int(integer_value_ref.get_type(), int64_t(1))
			: context.create_const_int(integer_value_ref.get_type(), uint64_t(1));
		auto const integer_value = integer_value_ref.get_value(context);
		context.create_add_check(func_call.src_tokens, integer_value, one_value, is_signed);
		auto const new_value = context.create_add(integer_value, one_value);
		context.create_store(new_value, integer_value_ref);
		return it_value;
	}
	case ast::function_body::builtin_integer_range_iterator_minus_minus:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expr_code(func_call.params[0], context, {});
		bz_assert(it_value.is_reference());
		auto const integer_value_ref = context.create_struct_gep(it_value, 0);
		bz_assert(func_call.params[0].get_expr_type().get_mut_reference().is<ast::ts_base_type>());
		auto const it_type_info = func_call.params[0].get_expr_type().get_mut_reference().get<ast::ts_base_type>().info;
		bz_assert(it_type_info->generic_parameters.size() == 1);
		bz_assert(it_type_info->generic_parameters[0].init_expr.is_typename());
		auto const &it_integer_type = it_type_info->generic_parameters[0].init_expr.get_typename();
		bz_assert(it_integer_type.is<ast::ts_base_type>());
		auto const is_signed = ast::is_signed_integer_kind(it_integer_type.get<ast::ts_base_type>().info->kind);
		auto const one_value = is_signed
			? context.create_const_int(integer_value_ref.get_type(), int64_t(1))
			: context.create_const_int(integer_value_ref.get_type(), uint64_t(1));
		auto const integer_value = integer_value_ref.get_value(context);
		context.create_sub_check(func_call.src_tokens, integer_value, one_value, is_signed);
		auto const new_value = context.create_sub(integer_value, one_value);
		context.create_store(new_value, integer_value_ref);
		return it_value;
	}
	case ast::function_body::builtin_integer_range_inclusive_begin_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, get_type(func_call.func_body->return_type, context));
		}
		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type()->is_aggregate());
		bz_assert(result_value.get_type()->get_aggregate_types().size() == 3);

		auto const range_value = generate_expr_code(func_call.params[0], context, {});
		auto const begin_value = context.create_struct_gep(range_value, 0);
		auto const end_value = context.create_struct_gep(range_value, 1);
		auto const false_value = context.create_const_i1(false);

		context.create_store(begin_value, context.create_struct_gep(result_value, 0));
		context.create_store(end_value,   context.create_struct_gep(result_value, 1));
		context.create_store(false_value, context.create_struct_gep(result_value, 2));
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_integer_range_inclusive_end_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, get_type(func_call.func_body->return_type, context));
		}
		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type()->is_aggregate());
		bz_assert(result_value.get_type()->get_aggregate_types().size() == 0);

		generate_expr_code(func_call.params[0], context, {});

		context.create_const_memset_zero(result_value);
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_dereference:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expr_code(func_call.params[0], context, {});
		auto const integer_value = context.create_struct_gep(it_value, 0);
		return value_or_result_address(integer_value, result_address, context);
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_left_equals:
	{
		bz_assert(func_call.params.size() == 2);
		auto const it_value = generate_expr_code(func_call.params[0], context, {});
		generate_expr_code(func_call.params[1], context, {});
		auto const at_end = context.create_struct_gep(it_value, 2).get_value(context);
		return value_or_result_address(at_end, result_address, context);
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_right_equals:
	{
		bz_assert(func_call.params.size() == 2);
		generate_expr_code(func_call.params[0], context, {});
		auto const it_value = generate_expr_code(func_call.params[1], context, {});
		auto const at_end = context.create_struct_gep(it_value, 2).get_value(context);
		return value_or_result_address(at_end, result_address, context);
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_left_not_equals:
	{
		bz_assert(func_call.params.size() == 2);
		auto const it_value = generate_expr_code(func_call.params[0], context, {});
		generate_expr_code(func_call.params[1], context, {});
		auto const at_end = context.create_struct_gep(it_value, 2);
		auto const result = context.create_not(at_end);
		return value_or_result_address(result, result_address, context);
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_right_not_equals:
	{
		bz_assert(func_call.params.size() == 2);
		generate_expr_code(func_call.params[0], context, {});
		auto const it_value = generate_expr_code(func_call.params[1], context, {});
		auto const at_end = context.create_struct_gep(it_value, 2);
		auto const result = context.create_not(at_end);
		return value_or_result_address(result, result_address, context);
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_plus_plus:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expr_code(func_call.params[0], context, {});
		bz_assert(it_value.is_reference());
		auto const integer_value_ref = context.create_struct_gep(it_value, 0);
		auto const integer_value = integer_value_ref.get_value(context);
		auto const end_value = context.create_struct_gep(it_value, 1).get_value(context);

		auto const begin_bb = context.get_current_basic_block();
		auto const is_at_end = context.create_int_cmp_eq(integer_value, end_value);

		auto const increment_bb = context.add_basic_block();
		context.set_current_basic_block(increment_bb);

		bz_assert(func_call.params[0].get_expr_type().get_mut_reference().is<ast::ts_base_type>());
		auto const it_type_info = func_call.params[0].get_expr_type().get_mut_reference().get<ast::ts_base_type>().info;
		bz_assert(it_type_info->generic_parameters.size() == 1);
		bz_assert(it_type_info->generic_parameters[0].init_expr.is_typename());
		auto const &it_integer_type = it_type_info->generic_parameters[0].init_expr.get_typename();
		bz_assert(it_integer_type.is<ast::ts_base_type>());
		auto const is_signed = ast::is_signed_integer_kind(it_integer_type.get<ast::ts_base_type>().info->kind);

		auto const one_value = is_signed
			? context.create_const_int(integer_value.get_type(), int64_t(1))
			: context.create_const_int(integer_value.get_type(), uint64_t(1));
		auto const new_value = context.create_add(integer_value, one_value);
		context.create_store(new_value, integer_value_ref);

		auto const at_end_bb = context.add_basic_block();
		context.set_current_basic_block(at_end_bb);
		auto const at_end_ref = context.create_struct_gep(it_value, 2);
		context.create_store(context.create_const_i1(true), at_end_ref);

		auto const end_bb = context.add_basic_block();
		context.set_current_basic_block(begin_bb);
		context.create_conditional_jump(is_at_end, at_end_bb, increment_bb);
		context.set_current_basic_block(increment_bb);
		context.create_jump(end_bb);
		context.set_current_basic_block(at_end_bb);
		context.create_jump(end_bb);

		context.set_current_basic_block(end_bb);
		return it_value;
	}
	case ast::function_body::builtin_integer_range_from_begin_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, get_type(func_call.func_body->return_type, context));
		}
		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type()->is_aggregate());
		bz_assert(result_value.get_type()->get_aggregate_types().size() == 1);

		auto const range_value = generate_expr_code(func_call.params[0], context, {});
		auto const begin_value = context.create_struct_gep(range_value, 0);

		context.create_store(begin_value, context.create_struct_gep(result_value, 0));
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_integer_range_from_end_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, get_type(func_call.func_body->return_type, context));
		}
		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type()->is_aggregate());
		bz_assert(result_value.get_type()->get_aggregate_types().size() == 0);

		generate_expr_code(func_call.params[0], context, {});

		context.create_const_memset_zero(result_value);
		context.create_start_lifetime(result_value);
		return result_value;
	}
	case ast::function_body::builtin_integer_range_from_iterator_dereference:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expr_code(func_call.params[0], context, {});
		auto const integer_value = context.create_struct_gep(it_value, 0);
		return value_or_result_address(integer_value, result_address, context);
	}
	case ast::function_body::builtin_integer_range_from_iterator_left_equals:
	case ast::function_body::builtin_integer_range_from_iterator_right_equals:
	{
		bz_assert(func_call.params.size() == 2);
		generate_expr_code(func_call.params[0], context, {});
		generate_expr_code(func_call.params[1], context, {});
		auto const result = context.create_const_i1(false);
		return value_or_result_address(result, result_address, context);
	}
	case ast::function_body::builtin_integer_range_from_iterator_left_not_equals:
	case ast::function_body::builtin_integer_range_from_iterator_right_not_equals:
	{
		bz_assert(func_call.params.size() == 2);
		generate_expr_code(func_call.params[0], context, {});
		generate_expr_code(func_call.params[1], context, {});
		auto const result = context.create_const_i1(true);
		return value_or_result_address(result, result_address, context);
	}
	case ast::function_body::builtin_integer_range_from_iterator_plus_plus:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expr_code(func_call.params[0], context, {});
		bz_assert(it_value.is_reference());
		auto const integer_value_ref = context.create_struct_gep(it_value, 0);
		bz_assert(func_call.params[0].get_expr_type().get_mut_reference().is<ast::ts_base_type>());
		auto const it_type_info = func_call.params[0].get_expr_type().get_mut_reference().get<ast::ts_base_type>().info;
		bz_assert(it_type_info->generic_parameters.size() == 1);
		bz_assert(it_type_info->generic_parameters[0].init_expr.is_typename());
		auto const &it_integer_type = it_type_info->generic_parameters[0].init_expr.get_typename();
		bz_assert(it_integer_type.is<ast::ts_base_type>());
		auto const is_signed = ast::is_signed_integer_kind(it_integer_type.get<ast::ts_base_type>().info->kind);
		auto const one_value = is_signed
			? context.create_const_int(integer_value_ref.get_type(), int64_t(1))
			: context.create_const_int(integer_value_ref.get_type(), uint64_t(1));
		auto const integer_value = integer_value_ref.get_value(context);
		context.create_add_check(func_call.src_tokens, integer_value, one_value, is_signed);
		auto const new_value = context.create_add(integer_value, one_value);
		context.create_store(new_value, integer_value_ref);
		return it_value;
	}
	case ast::function_body::builtin_optional_get_value_ref:
	case ast::function_body::builtin_optional_get_mut_value_ref:
	{
		bz_assert(func_call.params.size() == 1);
		auto const value = generate_expr_code(func_call.params[0], context, {});
		context.create_optional_get_value_check(func_call.src_tokens, get_optional_has_value(value, context));
		bz_assert(!result_address.has_value());
		return get_optional_value(value, context);
	}
	case ast::function_body::builtin_optional_get_value:
		// this is handled as a separate expression, not a function call
		bz_unreachable;
	case ast::function_body::builtin_pointer_cast:
		bz_assert(func_call.params.size() == 2);
		bz_assert(func_call.params[0].is_typename());
		generate_expr_code(func_call.params[1], context, {});
		context.create_error(
			func_call.src_tokens,
			bz::format("'{}' cannot be used in a constant expression", func_call.func_body->get_signature())
		);
		return value_or_result_address(
			context.get_dummy_value(context.get_pointer_type()),
			result_address,
			context
		);
	case ast::function_body::builtin_pointer_to_int:
		bz_assert(func_call.params.size() == 1);
		generate_expr_code(func_call.params[0], context, {});
		context.create_error(
			func_call.src_tokens,
			bz::format("'{}' cannot be used in a constant expression", func_call.func_body->get_signature())
		);
		return value_or_result_address(
			context.get_dummy_value(get_type(func_call.func_body->return_type, context)),
			result_address,
			context
		);
	case ast::function_body::builtin_int_to_pointer:
		bz_assert(func_call.params.size() == 2);
		bz_assert(func_call.params[0].is_typename());
		generate_expr_code(func_call.params[1], context, {});
		context.create_error(
			func_call.src_tokens,
			bz::format("'{}' cannot be used in a constant expression", func_call.func_body->get_signature())
		);
		return value_or_result_address(
			context.get_dummy_value(context.get_pointer_type()),
			result_address,
			context
		);
	case ast::function_body::builtin_enum_value:
		bz_assert(func_call.params.size() == 1);
		return generate_expr_code(func_call.params[0], context, result_address);
	case ast::function_body::builtin_destruct_value:
		// this is handled as a separate expression, not a function call
		bz_unreachable;
	case ast::function_body::builtin_inplace_construct:
	{
		bz_assert(func_call.params.size() == 2);
		auto const dest_ptr = generate_expr_code(func_call.params[0], context, {});
		auto const dest_typespec = func_call.func_body->params[1].get_type().as_typespec_view();
		auto const dest_type = get_type(dest_typespec, context);
		auto const dest_ref = expr_value::get_reference(dest_ptr.get_value_as_instruction(context), dest_type);
		context.create_inplace_construct_check(func_call.src_tokens, dest_ref, dest_typespec);
		generate_expr_code(func_call.params[1], context, dest_ref);
		bz_assert(!result_address.has_value());
		return expr_value::get_none();
	}
	case ast::function_body::builtin_swap:
		// this is handled as a separate expression, not a function call
		bz_unreachable;
	case ast::function_body::builtin_is_comptime:
		return value_or_result_address(context.create_const_i1(true), result_address, context);
	case ast::function_body::builtin_is_option_set:
	{
		bz_assert(func_call.params.size() == 1);
		auto const option = generate_expr_code(func_call.params[0], context, {});
		bz_assert(option.get_type() == context.get_str_t());
		auto const begin_ptr = context.create_struct_gep(option, 0).get_value(context);
		auto const end_ptr = context.create_struct_gep(option, 1).get_value(context);
		auto const result_value = context.create_is_option_set(begin_ptr, end_ptr);
		return value_or_result_address(result_value, result_address, context);
	}
	case ast::function_body::builtin_panic:
	{
		bz_assert(func_call.params.size() == 1);
		auto const message_value = generate_expr_code(func_call.params[0], context, {});
		auto const begin_ptr = context.create_struct_gep(message_value, 0);
		auto const end_ptr = context.create_struct_gep(message_value, 1);
		context.create_error_str(func_call.src_tokens, begin_ptr, end_ptr);
		bz_assert(!result_address.has_value());
		return expr_value::get_none();
	}
	case ast::function_body::builtin_panic_handler:
		// implemented in <target>/__main.bz
		bz_unreachable;
	case ast::function_body::builtin_call_main:
		bz_assert(func_call.params.size() == 1);
		generate_expr_code(func_call.params[0], context, {});
		context.create_error(
			func_call.src_tokens,
			bz::format("'{}' cannot be used in a constant expression", func_call.func_body->get_signature())
		);
		return value_or_result_address(
			context.get_dummy_value(get_type(func_call.func_body->return_type, context)),
			result_address,
			context
		);
	case ast::function_body::comptime_malloc:
	{
		bz_assert(func_call.params.size() == 2);
		bz_assert(func_call.params[0].is_constant() && func_call.params[0].is_typename());
		auto const alloc_type = get_type(func_call.params[0].get_typename(), context);
		auto const count = generate_expr_code(func_call.params[1], context, {});
		return value_or_result_address(context.create_malloc(func_call.src_tokens, alloc_type, count), result_address, context);
	}
	case ast::function_body::comptime_free:
	{
		bz_assert(func_call.params.size() == 1);
		auto const ptr = generate_expr_code(func_call.params[0], context, {});
		context.create_free(func_call.src_tokens, ptr);
		bz_assert(!result_address.has_value());
		return expr_value::get_none();
	}
	case ast::function_body::comptime_print:
	{
		if (global_data::enable_comptime_print)
		{
			bz_assert(func_call.params.size() == 1);
			auto const message_value = generate_expr_code(func_call.params[0], context, {});
			auto const begin_ptr = context.create_struct_gep(message_value, 0);
			auto const end_ptr = context.create_struct_gep(message_value, 1);
			context.create_print(begin_ptr, end_ptr);
		}
		else
		{
			context.create_error(
				func_call.src_tokens,
				bz::format("'{}' cannot be used in a constant expression", func_call.func_body->get_signature())
			);
		}
		bz_assert(!result_address.has_value());
		return expr_value::get_none();
	}
	case ast::function_body::comptime_compile_error:
	{
		bz_assert(func_call.params.size() == 1);
		auto const message_value = generate_expr_code(func_call.params[0], context, {});
		auto const begin_ptr = context.create_struct_gep(message_value, 0);
		auto const end_ptr = context.create_struct_gep(message_value, 1);
		context.create_error_str(func_call.src_tokens, begin_ptr, end_ptr);
		bz_assert(!result_address.has_value());
		return expr_value::get_none();
	}
	case ast::function_body::comptime_compile_warning:
	{
		bz_assert(func_call.params.size() == 1);
		auto const message_value = generate_expr_code(func_call.params[0], context, {});
		auto const begin_ptr = context.create_struct_gep(message_value, 0);
		auto const end_ptr = context.create_struct_gep(message_value, 1);
		context.create_warning_str(func_call.src_tokens, ctx::warning_kind::comptime_warning, begin_ptr, end_ptr);
		bz_assert(!result_address.has_value());
		return expr_value::get_none();
	}
	case ast::function_body::comptime_add_global_array_data:
	{
		bz_assert(func_call.params.size() == 2);
		auto const begin_ptr = generate_expr_code(func_call.params[0], context, {});
		auto const end_ptr = generate_expr_code(func_call.params[1], context, {});
		bz_assert(func_call.params[0].get_expr_type().is<ast::ts_pointer>());
		auto const elem_type = func_call.params[0].get_expr_type().get<ast::ts_pointer>().remove_any_mut();
		return value_or_result_address(
			context.create_add_global_array_data(func_call.src_tokens, get_type(elem_type, context), begin_ptr, end_ptr),
			result_address,
			context
		);
	}
	case ast::function_body::comptime_create_global_string:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::comptime_concatenate_strs:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::typename_as_str:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_mut:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_consteval:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_pointer:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_optional:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_reference:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_move_reference:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_slice:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_array:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_tuple:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_enum:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_mut:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_consteval:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_pointer:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_optional:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_reference:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_move_reference:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::slice_value_type:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::array_value_type:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::tuple_value_type:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::concat_tuple_types:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::enum_underlying_type:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_default_constructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_copy_constructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivially_copy_constructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_move_constructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivially_move_constructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivially_destructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivially_move_destructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivially_relocatable:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivial:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::create_initialized_array:
		// this is handled as a separate expression, not a function call
		bz_unreachable;
	case ast::function_body::trivially_copy_values:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expr_code(func_call.params[0], context, {});
		auto const source = generate_expr_code(func_call.params[1], context, {});
		auto const count = generate_expr_code(func_call.params[2], context, {});
		bz_assert(func_call.func_body->params[0].get_type().is_optional_pointer());
		auto const elem_typespec = func_call.func_body->params[0].get_type().get_optional_pointer();
		auto const elem_type = get_type(elem_typespec, context);
		context.create_copy_values(func_call.src_tokens, dest, source, count, elem_type, elem_typespec);
		return expr_value::get_none();
	}
	case ast::function_body::trivially_copy_overlapping_values:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expr_code(func_call.params[0], context, {});
		auto const source = generate_expr_code(func_call.params[1], context, {});
		auto const count = generate_expr_code(func_call.params[2], context, {});
		bz_assert(func_call.func_body->params[0].get_type().is_optional_pointer());
		auto const elem_typespec = func_call.func_body->params[0].get_type().get_optional_pointer();
		auto const elem_type = get_type(elem_typespec, context);
		context.create_copy_overlapping_values(func_call.src_tokens, dest, source, count, elem_type);
		return expr_value::get_none();
	}
	case ast::function_body::trivially_relocate_values:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expr_code(func_call.params[0], context, {});
		auto const source = generate_expr_code(func_call.params[1], context, {});
		auto const count = generate_expr_code(func_call.params[2], context, {});
		bz_assert(func_call.func_body->params[0].get_type().is_optional_pointer());
		auto const elem_typespec = func_call.func_body->params[0].get_type().get_optional_pointer();
		auto const elem_type = get_type(elem_typespec, context);
		context.create_relocate_values(func_call.src_tokens, dest, source, count, elem_type, elem_typespec);
		return expr_value::get_none();
	}
	case ast::function_body::trivially_set_values:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expr_code(func_call.params[0], context, {});
		auto const value = generate_expr_code(func_call.params[1], context, {});
		auto const count = generate_expr_code(func_call.params[2], context, {});
		context.create_set_values(func_call.src_tokens, dest, value, count);
		return expr_value::get_none();
	}
	case ast::function_body::bit_cast:
		// this is handled as a separate expression, not a function call
		bz_unreachable;
	case ast::function_body::trap:
		bz_assert(func_call.params.empty());
		context.create_error(
			func_call.src_tokens,
			bz::format("'{}' called in compile time execution", func_call.func_body->get_signature())
		);
		return expr_value::get_none();
	case ast::function_body::memcpy:
		bz_assert(func_call.params.size() == 3);
		generate_expr_code(func_call.params[0], context, {});
		generate_expr_code(func_call.params[1], context, {});
		generate_expr_code(func_call.params[2], context, {});
		context.create_error(
			func_call.src_tokens,
			bz::format("'{}' cannot be used in compile time execution", func_call.func_body->get_signature())
		);
		return expr_value::get_none();
	case ast::function_body::memmove:
		bz_assert(func_call.params.size() == 3);
		generate_expr_code(func_call.params[0], context, {});
		generate_expr_code(func_call.params[1], context, {});
		generate_expr_code(func_call.params[2], context, {});
		context.create_error(
			func_call.src_tokens,
			bz::format("'{}' cannot be used in compile time execution", func_call.func_body->get_signature())
		);
		return expr_value::get_none();
	case ast::function_body::memset:
		bz_assert(func_call.params.size() == 3);
		generate_expr_code(func_call.params[0], context, {});
		generate_expr_code(func_call.params[1], context, {});
		generate_expr_code(func_call.params[2], context, {});
		context.create_error(
			func_call.src_tokens,
			bz::format("'{}' cannot be used in compile time execution", func_call.func_body->get_signature())
		);
		return expr_value::get_none();
	case ast::function_body::isnan_f32:
	case ast::function_body::isnan_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		return value_or_result_address(context.create_isnan(x), result_address, context);
	}
	case ast::function_body::isinf_f32:
	case ast::function_body::isinf_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		return value_or_result_address(context.create_isinf(x), result_address, context);
	}
	case ast::function_body::isfinite_f32:
	case ast::function_body::isfinite_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		return value_or_result_address(context.create_isfinite(x), result_address, context);
	}
	case ast::function_body::abs_i8:
	case ast::function_body::abs_i16:
	case ast::function_body::abs_i32:
	case ast::function_body::abs_i64:
	case ast::function_body::abs_f32:
	case ast::function_body::abs_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const value = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_abs_check(func_call.src_tokens, value);
		}
		return value_or_result_address(context.create_abs(value), result_address, context);
	}
	case ast::function_body::min_i8:
	case ast::function_body::min_i16:
	case ast::function_body::min_i32:
	case ast::function_body::min_i64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const a = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const b = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		return value_or_result_address(context.create_min(a, b, true), result_address, context);
	}
	case ast::function_body::min_u8:
	case ast::function_body::min_u16:
	case ast::function_body::min_u32:
	case ast::function_body::min_u64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const a = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const b = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		return value_or_result_address(context.create_min(a, b, false), result_address, context);
	}
	case ast::function_body::fmin_f32:
	case ast::function_body::fmin_f64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const y = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_min_check(func_call.src_tokens, x, y);
		}
		return value_or_result_address(context.create_min(x, y, false), result_address, context);
	}
	case ast::function_body::max_i8:
	case ast::function_body::max_i16:
	case ast::function_body::max_i32:
	case ast::function_body::max_i64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const a = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const b = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		return value_or_result_address(context.create_max(a, b, true), result_address, context);
	}
	case ast::function_body::max_u8:
	case ast::function_body::max_u16:
	case ast::function_body::max_u32:
	case ast::function_body::max_u64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const a = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const b = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		return value_or_result_address(context.create_max(a, b, false), result_address, context);
	}
	case ast::function_body::fmax_f32:
	case ast::function_body::fmax_f64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const y = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_max_check(func_call.src_tokens, x, y);
		}
		return value_or_result_address(context.create_max(x, y, false), result_address, context);
	}
	case ast::function_body::exp_f32:
	case ast::function_body::exp_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_exp_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_exp(x), result_address, context);
	}
	case ast::function_body::exp2_f32:
	case ast::function_body::exp2_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_exp2_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_exp2(x), result_address, context);
	}
	case ast::function_body::expm1_f32:
	case ast::function_body::expm1_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_expm1_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_expm1(x), result_address, context);
	}
	case ast::function_body::log_f32:
	case ast::function_body::log_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_log_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_log(x), result_address, context);
	}
	case ast::function_body::log10_f32:
	case ast::function_body::log10_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_log10_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_log10(x), result_address, context);
	}
	case ast::function_body::log2_f32:
	case ast::function_body::log2_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_log2_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_log2(x), result_address, context);
	}
	case ast::function_body::log1p_f32:
	case ast::function_body::log1p_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_log1p_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_log1p(x), result_address, context);
	}
	case ast::function_body::sqrt_f32:
	case ast::function_body::sqrt_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_sqrt_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_sqrt(x), result_address, context);
	}
	case ast::function_body::pow_f32:
	case ast::function_body::pow_f64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const y = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_pow_check(original_expression.src_tokens, x, y);
		}
		return value_or_result_address(context.create_pow(x, y), result_address, context);
	}
	case ast::function_body::cbrt_f32:
	case ast::function_body::cbrt_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_cbrt_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_cbrt(x), result_address, context);
	}
	case ast::function_body::hypot_f32:
	case ast::function_body::hypot_f64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const y = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_hypot_check(original_expression.src_tokens, x, y);
		}
		return value_or_result_address(context.create_hypot(x, y), result_address, context);
	}
	case ast::function_body::sin_f32:
	case ast::function_body::sin_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_sin_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_sin(x), result_address, context);
	}
	case ast::function_body::cos_f32:
	case ast::function_body::cos_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_cos_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_cos(x), result_address, context);
	}
	case ast::function_body::tan_f32:
	case ast::function_body::tan_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_tan_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_tan(x), result_address, context);
	}
	case ast::function_body::asin_f32:
	case ast::function_body::asin_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_asin_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_asin(x), result_address, context);
	}
	case ast::function_body::acos_f32:
	case ast::function_body::acos_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_acos_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_acos(x), result_address, context);
	}
	case ast::function_body::atan_f32:
	case ast::function_body::atan_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_atan_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_atan(x), result_address, context);
	}
	case ast::function_body::atan2_f32:
	case ast::function_body::atan2_f64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const y = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const x = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_atan2_check(original_expression.src_tokens, y, x);
		}
		return value_or_result_address(context.create_atan2(y, x), result_address, context);
	}
	case ast::function_body::sinh_f32:
	case ast::function_body::sinh_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_sinh_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_sinh(x), result_address, context);
	}
	case ast::function_body::cosh_f32:
	case ast::function_body::cosh_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_cosh_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_cosh(x), result_address, context);
	}
	case ast::function_body::tanh_f32:
	case ast::function_body::tanh_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_tanh_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_tanh(x), result_address, context);
	}
	case ast::function_body::asinh_f32:
	case ast::function_body::asinh_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_asinh_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_asinh(x), result_address, context);
	}
	case ast::function_body::acosh_f32:
	case ast::function_body::acosh_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_acosh_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_acosh(x), result_address, context);
	}
	case ast::function_body::atanh_f32:
	case ast::function_body::atanh_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_atanh_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_atanh(x), result_address, context);
	}
	case ast::function_body::erf_f32:
	case ast::function_body::erf_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_erf_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_erf(x), result_address, context);
	}
	case ast::function_body::erfc_f32:
	case ast::function_body::erfc_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_erfc_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_erfc(x), result_address, context);
	}
	case ast::function_body::tgamma_f32:
	case ast::function_body::tgamma_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_tgamma_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_tgamma(x), result_address, context);
	}
	case ast::function_body::lgamma_f32:
	case ast::function_body::lgamma_f64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const x = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		if (original_expression.paren_level < 2)
		{
			context.create_lgamma_check(original_expression.src_tokens, x);
		}
		return value_or_result_address(context.create_lgamma(x), result_address, context);
	}
	case ast::function_body::bitreverse_u8:
	case ast::function_body::bitreverse_u16:
	case ast::function_body::bitreverse_u32:
	case ast::function_body::bitreverse_u64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const value = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		return value_or_result_address(context.create_bitreverse(value), result_address, context);
	}
	case ast::function_body::popcount_u8:
	case ast::function_body::popcount_u16:
	case ast::function_body::popcount_u32:
	case ast::function_body::popcount_u64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const value = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		return value_or_result_address(context.create_popcount(value), result_address, context);
	}
	case ast::function_body::byteswap_u16:
	case ast::function_body::byteswap_u32:
	case ast::function_body::byteswap_u64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const value = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		return value_or_result_address(context.create_byteswap(value), result_address, context);
	}
	case ast::function_body::clz_u8:
	case ast::function_body::clz_u16:
	case ast::function_body::clz_u32:
	case ast::function_body::clz_u64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const value = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		return value_or_result_address(context.create_clz(value), result_address, context);
	}
	case ast::function_body::ctz_u8:
	case ast::function_body::ctz_u16:
	case ast::function_body::ctz_u32:
	case ast::function_body::ctz_u64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const value = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		return value_or_result_address(context.create_ctz(value), result_address, context);
	}
	case ast::function_body::fshl_u8:
	case ast::function_body::fshl_u16:
	case ast::function_body::fshl_u32:
	case ast::function_body::fshl_u64:
	{
		bz_assert(func_call.params.size() == 3);
		auto const a = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const b = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		auto const amount = generate_expr_code(func_call.params[2], context, {}).get_value(context);
		return value_or_result_address(context.create_fshl(a, b, amount), result_address, context);
	}
	case ast::function_body::fshr_u8:
	case ast::function_body::fshr_u16:
	case ast::function_body::fshr_u32:
	case ast::function_body::fshr_u64:
	{
		bz_assert(func_call.params.size() == 3);
		auto const a = generate_expr_code(func_call.params[0], context, {}).get_value(context);
		auto const b = generate_expr_code(func_call.params[1], context, {}).get_value(context);
		auto const amount = generate_expr_code(func_call.params[2], context, {}).get_value(context);
		return value_or_result_address(context.create_fshr(a, b, amount), result_address, context);
	}
	case ast::function_body::i8_default_constructor:
	case ast::function_body::i16_default_constructor:
	case ast::function_body::i32_default_constructor:
	case ast::function_body::i64_default_constructor:
	case ast::function_body::u8_default_constructor:
	case ast::function_body::u16_default_constructor:
	case ast::function_body::u32_default_constructor:
	case ast::function_body::u64_default_constructor:
	case ast::function_body::f32_default_constructor:
	case ast::function_body::f64_default_constructor:
	case ast::function_body::char_default_constructor:
	case ast::function_body::str_default_constructor:
	case ast::function_body::bool_default_constructor:
	case ast::function_body::null_t_default_constructor:
		// these are guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::builtin_unary_plus:
		bz_assert(func_call.params.size() == 1);
		return generate_builtin_unary_plus(func_call.params[0], context, result_address);
	case ast::function_body::builtin_unary_minus:
		bz_assert(func_call.params.size() == 1);
		return generate_builtin_unary_minus(original_expression, func_call.params[0], context, result_address);
	case ast::function_body::builtin_unary_dereference:
		bz_assert(func_call.params.size() == 1);
		bz_assert(!result_address.has_value());
		return generate_builtin_unary_dereference(func_call.params[0], context);
	case ast::function_body::builtin_unary_bit_not:
		bz_assert(func_call.params.size() == 1);
		return generate_builtin_unary_bit_not(func_call.params[0], context, result_address);
	case ast::function_body::builtin_unary_bool_not:
		bz_assert(func_call.params.size() == 1);
		return generate_builtin_unary_bool_not(func_call.params[0], context, result_address);
	case ast::function_body::builtin_unary_plus_plus:
		bz_assert(func_call.params.size() == 1);
		bz_assert(!result_address.has_value());
		return generate_builtin_unary_plus_plus(original_expression, func_call.params[0], context);
	case ast::function_body::builtin_unary_minus_minus:
		bz_assert(func_call.params.size() == 1);
		bz_assert(!result_address.has_value());
		return generate_builtin_unary_minus_minus(original_expression, func_call.params[0], context);
	case ast::function_body::builtin_binary_assign:
		// assignment is handled as a separate expression
		bz_unreachable;
	case ast::function_body::builtin_binary_plus:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_plus(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_plus_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_address.has_value());
		return generate_builtin_binary_plus_eq(original_expression, func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_minus:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_minus(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_minus_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_address.has_value());
		return generate_builtin_binary_minus_eq(original_expression, func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_multiply:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_multiply(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_multiply_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_address.has_value());
		return generate_builtin_binary_multiply_eq(original_expression, func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_divide:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_divide(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_divide_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_address.has_value());
		return generate_builtin_binary_divide_eq(original_expression, func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_modulo:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_modulo(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_modulo_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_address.has_value());
		return generate_builtin_binary_modulo_eq(original_expression, func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_equals:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_equals(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_not_equals:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_not_equals(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_less_than:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_less_than(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_less_than_eq:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_less_than_eq(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_greater_than:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_greater_than(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_greater_than_eq:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_greater_than_eq(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_bit_and:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_bit_and(func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_bit_and_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_address.has_value());
		return generate_builtin_binary_bit_and_eq(func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_bit_xor:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_bit_xor(func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_bit_xor_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_address.has_value());
		return generate_builtin_binary_bit_xor_eq(func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_bit_or:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_bit_or(func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_bit_or_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_address.has_value());
		return generate_builtin_binary_bit_or_eq(func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_bit_left_shift:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_bit_left_shift(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_bit_left_shift_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_address.has_value());
		return generate_builtin_binary_bit_left_shift_eq(original_expression, func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_bit_right_shift:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_bit_right_shift(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	case ast::function_body::builtin_binary_bit_right_shift_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_address.has_value());
		return generate_builtin_binary_bit_right_shift_eq(original_expression, func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_subscript:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_subscript_range(original_expression, func_call.params[0], func_call.params[1], context, result_address);
	default:
		bz_unreachable;
	}
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_function_call const &func_call,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	if (func_call.func_body->is_intrinsic() && func_call.func_body->body.is_null())
	{
		return generate_intrinsic_function_call_code(original_expression, func_call, context, result_address);
	}
	bz_assert(!func_call.func_body->is_default_copy_constructor());
	bz_assert(!func_call.func_body->is_default_move_constructor());
	bz_assert(!func_call.func_body->is_default_default_constructor());
	bz_assert(!func_call.func_body->is_default_op_assign());
	bz_assert(!func_call.func_body->is_default_op_move_assign());

	if (!func_call.func_body->is_intrinsic() && func_call.func_body->body.is_null())
	{
		context.create_error(
			func_call.src_tokens,
			bz::format("unable to call external function '{}' in compile time execution", func_call.func_body->get_signature())
		);
		if (result_address.has_value())
		{
			return result_address.get();
		}
		else if (func_call.func_body->return_type.is<ast::ts_void>())
		{
			return expr_value::get_none();
		}
		else
		{
			return context.get_dummy_value(get_type(func_call.func_body->return_type, context));
		}
	}

	auto const func = context.get_function(func_call.func_body);
	bz_assert(func != nullptr);

	// along with the arguments, the result address is passed as the first argument if it's not a builtin or pointer type
	auto const needs_return_address = !func->return_type->is_simple_value_type();
	auto arg_refs = bz::fixed_vector<instruction_ref>(func->arg_types.size() + (needs_return_address ? 1 : 0));

	if (func_call.param_resolve_order == ast::resolve_order::regular)
	{
		size_t arg_ref_index = needs_return_address ? 1 : 0;

		for (auto const arg_index : bz::iota(0, func_call.params.size()))
		{
			if (ast::is_generic_parameter(func_call.func_body->params[arg_index]))
			{
				bz_assert(func_call.params[arg_index].is_constant() || func_call.params[arg_index].is_error());
				continue;
			}
			else if (func_call.params[arg_index].is_error())
			{
				continue;
			}
			else
			{
				auto const arg_type = func->arg_types[arg_ref_index - needs_return_address];
				auto const &param_type = func_call.func_body->params[arg_index].get_type();
				if (param_type.is_any_reference())
				{
					auto const arg_value = generate_expr_code(func_call.params[arg_index], context, {});
					bz_assert(arg_type->is_pointer());
					bz_assert(arg_value.is_reference());
					arg_refs[arg_ref_index] = arg_value.get_reference();
				}
				else if (arg_type->is_simple_value_type())
				{
					auto const arg_value = generate_expr_code(func_call.params[arg_index], context, {});
					bz_assert(arg_value.get_type() == arg_type);
					arg_refs[arg_ref_index] = arg_value.get_value_as_instruction(context);
				}
				else
				{
					auto const param_result_address = context.create_alloca(func_call.params[arg_index].src_tokens, arg_type);
					generate_expr_code(func_call.params[arg_index], context, param_result_address);
					arg_refs[arg_ref_index] = param_result_address.get_reference();
				}
				++arg_ref_index;
			}
		}

		bz_assert(arg_ref_index == arg_refs.size());
	}
	else
	{
		size_t arg_ref_index = arg_refs.size();

		for (auto const arg_index : bz::iota(0, func_call.params.size()).reversed())
		{
			if (ast::is_generic_parameter(func_call.func_body->params[arg_index]))
			{
				bz_assert(func_call.params[arg_index].is_constant() || func_call.params[arg_index].is_error());
				continue;
			}
			else if (func_call.params[arg_index].is_error())
			{
				continue;
			}
			else
			{
				--arg_ref_index;
				auto const arg_type = func->arg_types[arg_ref_index - needs_return_address];
				auto const &param_type = func_call.func_body->params[arg_index].get_type();
				if (param_type.is<ast::ts_lvalue_reference>() || param_type.is<ast::ts_move_reference>())
				{
					auto const arg_value = generate_expr_code(func_call.params[arg_index], context, {});
					bz_assert(func->arg_types[arg_ref_index - needs_return_address]->is_pointer());
					bz_assert(arg_value.is_reference());
					arg_refs[arg_ref_index] = arg_value.get_reference();
				}
				else if (arg_type->is_simple_value_type())
				{
					auto const arg_value = generate_expr_code(func_call.params[arg_index], context, {});
					bz_assert(arg_value.get_type() == arg_type);
					arg_refs[arg_ref_index] = arg_value.get_value_as_instruction(context);
				}
				else
				{
					auto const param_result_address = context.create_alloca(func_call.params[arg_index].src_tokens, arg_type);
					generate_expr_code(func_call.params[arg_index], context, param_result_address);
					arg_refs[arg_ref_index] = param_result_address.get_reference();
				}
			}
		}

		bz_assert(arg_ref_index == (needs_return_address ? 1 : 0));
	}

	if (needs_return_address)
	{
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, func->return_type);
		}

		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type() == func->return_type);

		arg_refs[0] = result_value.get_reference();

		context.create_function_call(func_call.src_tokens, func, std::move(arg_refs));

		return result_value;
	}
	else if (func->return_type->is_void())
	{
		context.create_function_call(func_call.src_tokens, func, std::move(arg_refs));
		return expr_value::get_none();
	}
	else
	{
		auto const result_value = context.create_function_call(func_call.src_tokens, func, std::move(arg_refs));
		if (func_call.func_body->return_type.is<ast::ts_lvalue_reference>())
		{
			auto const type = get_type(func_call.func_body->return_type.get<ast::ts_lvalue_reference>(), context);
			return expr_value::get_reference(result_value.get_value_as_instruction(context), type);
		}
		else
		{
			return value_or_result_address(result_value, result_address, context);
		}
	}
}

static expr_value generate_expr_code(
	ast::expr_indirect_function_call const &func_call,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const func_ptr = generate_expr_code(func_call.called, context, {});
	bz_assert(func_call.called.get_expr_type().remove_mut_reference().is<ast::ts_function>());
	auto const &function_typespec = func_call.called.get_expr_type().remove_mut_reference().get<ast::ts_function>();
	auto const return_type = get_type(function_typespec.return_type, context);

	// along with the arguments, the result address is passed as the first argument if it's not a builtin or pointer type
	auto const needs_return_address = !return_type->is_simple_value_type();
	auto arg_refs = bz::fixed_vector<instruction_ref>(function_typespec.param_types.size() + (needs_return_address ? 1 : 0));

	for (auto const arg_index : bz::iota(0, func_call.params.size()))
	{
		if (func_call.params[arg_index].is_error())
		{
			continue;
		}
		else
		{
			auto const &param_type = function_typespec.param_types[arg_index];
			auto const arg_type = get_type(param_type, context);
			if (param_type.is<ast::ts_lvalue_reference>() || param_type.is<ast::ts_move_reference>())
			{
				auto const arg_value = generate_expr_code(func_call.params[arg_index], context, {});
				bz_assert(arg_type->is_pointer());
				bz_assert(arg_value.is_reference());
				arg_refs[arg_index + (needs_return_address ? 1 : 0)] = arg_value.get_reference();
			}
			else if (arg_type->is_simple_value_type())
			{
				auto const arg_value = generate_expr_code(func_call.params[arg_index], context, {});
				bz_assert(arg_value.get_type() == arg_type);
				arg_refs[arg_index + (needs_return_address ? 1 : 0)] = arg_value.get_value_as_instruction(context);
			}
			else
			{
				auto const param_result_address = context.create_alloca(func_call.params[arg_index].src_tokens, arg_type);
				generate_expr_code(func_call.params[arg_index], context, param_result_address);
				arg_refs[arg_index + (needs_return_address ? 1 : 0)] = param_result_address.get_reference();
			}
		}
	}

	if (needs_return_address)
	{
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(func_call.src_tokens, return_type);
		}

		auto const &result_value = result_address.get();
		bz_assert(result_value.get_type() == return_type);

		arg_refs[0] = result_value.get_reference();

		context.create_indirect_function_call(func_call.src_tokens, func_ptr, return_type, std::move(arg_refs));

		return result_value;
	}
	else if (return_type->is_void())
	{
		context.create_indirect_function_call(func_call.src_tokens, func_ptr, return_type, std::move(arg_refs));
		return expr_value::get_none();
	}
	else
	{
		auto const result_value = context.create_indirect_function_call(
			func_call.src_tokens,
			func_ptr,
			return_type,
			std::move(arg_refs)
		);
		if (function_typespec.return_type.is<ast::ts_lvalue_reference>())
		{
			auto const type = get_type(function_typespec.return_type.get<ast::ts_lvalue_reference>(), context);
			return expr_value::get_reference(result_value.get_value_as_instruction(context), type);
		}
		else
		{
			return value_or_result_address(result_value, result_address, context);
		}
	}
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_cast const &cast,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const expr_t = cast.expr.get_expr_type().remove_mut_reference();
	auto const dest_t = cast.type.remove_any_mut();

	if (expr_t.is<ast::ts_base_type>() && dest_t.is<ast::ts_base_type>())
	{
		auto const dest_type = get_type(dest_t, context);
		auto const expr = generate_expr_code(cast.expr, context, {});
		auto const expr_kind = expr_t.get<ast::ts_base_type>().info->kind;
		auto const dest_kind = dest_t.get<ast::ts_base_type>().info->kind;

		if ((ast::is_integer_kind(expr_kind) || expr_kind == ast::type_info::bool_) && ast::is_integer_kind(dest_kind))
		{
			auto const result_value = context.create_int_cast(expr, dest_type, ast::is_signed_integer_kind(expr_kind));
			return value_or_result_address(result_value, result_address, context);
		}
		else if (ast::is_floating_point_kind(expr_kind) && ast::is_floating_point_kind(dest_kind))
		{
			auto const result_value = context.create_float_cast(expr, dest_type);
			return value_or_result_address(result_value, result_address, context);
		}
		else if (ast::is_floating_point_kind(expr_kind))
		{
			bz_assert(ast::is_integer_kind(dest_kind));
			auto const result_value = context.create_float_to_int_cast(expr, dest_type, ast::is_signed_integer_kind(dest_kind));
			return value_or_result_address(result_value, result_address, context);
		}
		else if (ast::is_integer_kind(expr_kind) && ast::is_floating_point_kind(dest_kind))
		{
			auto const result_value = context.create_int_to_float_cast(expr, dest_type, ast::is_signed_integer_kind(expr_kind));
			return value_or_result_address(result_value, result_address, context);
		}
		else
		{
			bz_assert(
				(expr_kind == ast::type_info::char_ && ast::is_integer_kind(dest_kind))
				|| (ast::is_integer_kind(expr_kind) && dest_kind == ast::type_info::char_)
			);
			auto const result_value = context.create_int_cast(expr, dest_type, ast::is_signed_integer_kind(expr_kind));
			return value_or_result_address(result_value, result_address, context);
		}
	}
	else if (
		(expr_t.is<ast::ts_pointer>() || expr_t.is_optional_pointer())
		&& (dest_t.is<ast::ts_pointer>() || dest_t.is_optional_pointer())
	)
	{
		auto const result_value = generate_expr_code(cast.expr, context, {});
		return value_or_result_address(result_value, result_address, context);
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
			result_address = context.create_alloca(original_expression.src_tokens, context.get_slice_t());
		}

		auto const &result_value = result_address.get();
		auto const begin_ptr_value = expr_value::get_value(begin_ptr, context.get_pointer_type());
		auto const end_ptr_value = expr_value::get_value(end_ptr, context.get_pointer_type());
		context.create_store(begin_ptr_value, context.create_struct_gep(result_value, 0));
		context.create_store(end_ptr_value, context.create_struct_gep(result_value, 1));
		context.create_start_lifetime(result_value);
		return result_value;
	}
	else
	{
		bz_unreachable;
	}
}

static bool contains_pointer(type const *type)
{
	if (type->is_builtin())
	{
		return false;
	}
	else if (type->is_pointer())
	{
		return true;
	}
	else if (type->is_aggregate())
	{
		return type->get_aggregate_types().is_any([](auto const elem_type) { return contains_pointer(elem_type); });
	}
	else if (type->is_array())
	{
		return contains_pointer(type->get_array_element_type());
	}
	else
	{
		return false;
	}
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_bit_cast const &bit_cast,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const expr_type = get_type(bit_cast.expr.get_expr_type(), context);
	if (!result_address.has_value())
	{
		result_address = context.create_alloca(original_expression.src_tokens, get_type(bit_cast.type, context));
	}

	auto const &result_value = result_address.get();

	if (contains_pointer(expr_type))
	{
		context.create_error(
			original_expression.src_tokens,
			bz::format(
				"value of type '{}' cannot be used in a bit cast in compile time execution because it contains pointers",
				bit_cast.expr.get_expr_type()
			)
		);
	}
	else if (contains_pointer(result_value.get_type()))
	{
		context.create_error(
			original_expression.src_tokens,
			bz::format(
				"result type '{}' cannot be used in a bit cast in compile time execution because it contains pointers",
				bit_cast.type
			)
		);
	}
	else
	{
		auto const expr_result_address = expr_value::get_reference(result_value.get_reference(), expr_type);
		generate_expr_code(bit_cast.expr, context, expr_result_address);
	}
	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_optional_cast const &optional_cast,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	if (optional_cast.type.is_optional_pointer_like())
	{
		return generate_expr_code(optional_cast.expr, context, result_address);
	}
	else
	{
		if (!result_address.has_value())
		{
			bz_assert(optional_cast.type.is<ast::ts_optional>());
			auto const type = get_type(optional_cast.type, context);

			result_address = context.create_alloca(original_expression.src_tokens, type);
		}

		auto const &result_value = result_address.get();

		auto const opt_value = get_optional_value(result_value, context);
		generate_expr_code(optional_cast.expr, context, opt_value);
		set_optional_has_value(result_value, true, context);
		context.create_start_lifetime(get_optional_has_value_ref(result_value, context));

		return result_value;
	}
}

static expr_value generate_expr_code(
	ast::expr_take_reference const &take_ref,
	codegen_context &context
)
{
	auto const result = generate_expr_code(take_ref.expr, context, {});
	bz_assert(result.is_reference());
	return result;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_take_move_reference const &take_move_ref,
	codegen_context &context
)
{
	if (!take_move_ref.expr.is_dynamic() || take_move_ref.expr.get_dynamic().destruct_op.is_null())
	{
		auto const result = generate_expr_code(take_move_ref.expr, context, {});
		if (result.is_reference())
		{
			return result;
		}
		else
		{
			auto const alloca = context.create_alloca(original_expression.src_tokens, result.get_type());
			context.create_store(result, alloca);
			context.create_start_lifetime(alloca);
			context.push_end_lifetime(alloca);
			return alloca;
		}
	}
	else
	{
		return generate_expr_code(take_move_ref.expr, context, {});
	}
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_aggregate_init const &aggregate_init,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	if (!result_address.has_value())
	{
		auto const type = get_type(aggregate_init.type, context);
		result_address = context.create_alloca(original_expression.src_tokens, type);
	}

	auto const &result_value = result_address.get();
	bz_assert(result_value.get_type()->is_aggregate() || result_value.get_type()->is_array());
	for (auto const i : bz::iota(0, aggregate_init.exprs.size()))
	{
		auto const member_ptr = context.create_struct_gep(result_value, i);
		if (aggregate_init.exprs[i].get_expr_type().is_reference())
		{
			auto const ref = generate_expr_code(aggregate_init.exprs[i], context, {});
			bz_assert(member_ptr.get_type()->is_pointer());
			context.create_store(expr_value::get_value(ref.get_reference(), context.get_pointer_type()), member_ptr);
			context.create_start_lifetime(member_ptr);
		}
		else
		{
			generate_expr_code(aggregate_init.exprs[i], context, member_ptr);
		}
	}
	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_array_value_init const &array_value_init,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	bz_assert(array_value_init.type.is<ast::ts_array>());
	auto const size = array_value_init.type.get<ast::ts_array>().size;

	if (!result_address.has_value())
	{
		auto const type = get_type(array_value_init.type, context);
		result_address = context.create_alloca(original_expression.src_tokens, type);
	}

	auto const &result_value = result_address.get();
	bz_assert(result_value.get_type()->is_array());

	auto const value = generate_expr_code(array_value_init.value, context, {});
	auto const prev_value = context.push_value_reference(value);

	auto const loop_info = create_loop_start(size, context);

	auto const elem_result_address = context.create_array_gep(result_value, loop_info.index);
	generate_expr_code(array_value_init.copy_expr, context, elem_result_address);

	create_loop_end(loop_info, context);

	context.pop_value_reference(prev_value);
	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_aggregate_default_construct const &aggregate_default_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	if (!result_address.has_value())
	{
		auto const type = get_type(aggregate_default_construct.type, context);
		result_address = context.create_alloca(original_expression.src_tokens, type);
	}

	auto const &result_value = result_address.get();
	bz_assert(result_value.get_type()->is_aggregate() || result_value.get_type()->is_array());
	for (auto const i : bz::iota(0, aggregate_default_construct.default_construct_exprs.size()))
	{
		bz_assert(!aggregate_default_construct.default_construct_exprs[i].get_expr_type().is_any_reference());
		auto const member_ptr = context.create_struct_gep(result_value, i);
		generate_expr_code(aggregate_default_construct.default_construct_exprs[i], context, member_ptr);
	}
	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_array_default_construct const &array_default_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	bz_assert(array_default_construct.type.is<ast::ts_array>());
	auto const size = array_default_construct.type.get<ast::ts_array>().size;

	if (!result_address.has_value())
	{
		result_address = context.create_alloca(original_expression.src_tokens, get_type(array_default_construct.type, context));
	}

	auto const &result_value = result_address.get();

	auto const loop_info = create_loop_start(size, context);

	auto const elem_result_address = context.create_array_gep(result_value, loop_info.index);
	generate_expr_code(array_default_construct.default_construct_expr, context, elem_result_address);

	create_loop_end(loop_info, context);

	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_optional_default_construct const &optional_default_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const is_ptr = optional_default_construct.type.is_optional_pointer_like();
	if (is_ptr)
	{
		return value_or_result_address(context.create_const_ptr_null(), result_address, context);
	}
	else
	{
		if (!result_address.has_value())
		{
			auto const type = get_type(optional_default_construct.type, context);
			result_address = context.create_alloca(original_expression.src_tokens, type);
		}

		auto const &result_value = result_address.get();
		set_optional_has_value(result_value, false, context);
		context.create_start_lifetime(get_optional_has_value_ref(result_value, context));
		return result_value;
	}
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_builtin_default_construct const &builtin_default_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	bz_assert(builtin_default_construct.type.is<ast::ts_array_slice>());
	if (!result_address.has_value())
	{
		auto const slice_type = context.get_slice_t();
		result_address = context.create_alloca(original_expression.src_tokens, slice_type);
	}

	auto const &result_value = result_address.get();
	context.create_const_memset_zero(result_value);
	context.create_start_lifetime(result_value);
	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
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
		result_address = context.create_alloca(original_expression.src_tokens, type);
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
	ast::expression const &original_expression,
	ast::expr_array_copy_construct const &array_copy_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const copied_val = generate_expr_code(array_copy_construct.copied_value, context, {});

	if (!result_address.has_value())
	{
		result_address = context.create_alloca(original_expression.src_tokens, copied_val.get_type());
	}

	auto const &result_value = result_address.get();

	bz_assert(copied_val.get_type()->is_array());
	auto const loop_info = create_loop_start(copied_val.get_type()->get_array_size(), context);

	auto const elem_result_address = context.create_array_gep(result_value, loop_info.index);
	auto const copied_elem = context.create_array_gep(copied_val, loop_info.index);
	auto const prev_value = context.push_value_reference(copied_elem);
	generate_expr_code(array_copy_construct.copy_expr, context, elem_result_address);
	context.pop_value_reference(prev_value);

	create_loop_end(loop_info, context);

	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_optional_copy_construct const &optional_copy_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const copied_val = generate_expr_code(optional_copy_construct.copied_value, context, {});

	if (!result_address.has_value())
	{
		result_address = context.create_alloca(original_expression.src_tokens, copied_val.get_type());
	}

	auto const &result_value = result_address.get();

	auto const has_value = get_optional_has_value(copied_val, context);
	set_optional_has_value(result_value, has_value, context);
	context.create_start_lifetime(get_optional_has_value_ref(result_value, context));
	auto const begin_bb = context.get_current_basic_block();

	auto const copy_bb = context.add_basic_block();
	context.set_current_basic_block(copy_bb);

	auto const result_opt_value = get_optional_value(result_value, context);
	auto const prev_value = context.push_value_reference(get_optional_value(copied_val, context));
	generate_expr_code(optional_copy_construct.value_copy_expr, context, result_opt_value);
	context.pop_value_reference(prev_value);

	auto const end_bb = context.add_basic_block();
	context.create_jump(end_bb);

	context.set_current_basic_block(begin_bb);
	context.create_conditional_jump(has_value, copy_bb, end_bb);
	context.set_current_basic_block(end_bb);

	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_trivial_copy_construct const &trivial_copy_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const copied_val = generate_expr_code(trivial_copy_construct.copied_value, context, {});
	if (copied_val.get_type()->is_aggregate() || copied_val.get_type()->is_array())
	{
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(original_expression.src_tokens, copied_val.get_type());
		}

		auto const &result_value = result_address.get();
		generate_value_copy(copied_val, result_value, context);
		context.create_start_lifetime(result_value);
		return result_value;
	}
	else
	{
		return value_or_result_address(copied_val.get_value(context), result_address, context);
	}
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
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
		result_address = context.create_alloca(original_expression.src_tokens, type);
	}

	auto const &result_value = result_address.get();
	for (auto const i : bz::iota(0, aggregate_move_construct.move_exprs.size()))
	{
		auto const result_member_value = context.create_struct_gep(result_value, i);
		auto const member_value = context.create_struct_gep(moved_val, i);
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(member_value);
		generate_expr_code(aggregate_move_construct.move_exprs[i], context, result_member_value);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}
	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_array_move_construct const &array_move_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const moved_val = generate_expr_code(array_move_construct.moved_value, context, {});

	if (!result_address.has_value())
	{
		result_address = context.create_alloca(original_expression.src_tokens, moved_val.get_type());
	}

	auto const &result_value = result_address.get();

	bz_assert(moved_val.get_type()->is_array());
	auto const loop_info = create_loop_start(moved_val.get_type()->get_array_size(), context);

	auto const elem_result_address = context.create_array_gep(result_value, loop_info.index);
	auto const moved_elem = context.create_array_gep(moved_val, loop_info.index);
	auto const prev_value = context.push_value_reference(moved_elem);
	generate_expr_code(array_move_construct.move_expr, context, elem_result_address);
	context.pop_value_reference(prev_value);

	create_loop_end(loop_info, context);

	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_optional_move_construct const &optional_move_construct,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const moved_val = generate_expr_code(optional_move_construct.moved_value, context, {});

	if (!result_address.has_value())
	{
		result_address = context.create_alloca(original_expression.src_tokens, moved_val.get_type());
	}

	auto const &result_value = result_address.get();

	auto const has_value = get_optional_has_value(moved_val, context);
	set_optional_has_value(result_value, has_value, context);
	context.create_start_lifetime(get_optional_has_value_ref(result_value, context));
	auto const begin_bb = context.get_current_basic_block();

	auto const copy_bb = context.add_basic_block();
	context.set_current_basic_block(copy_bb);

	auto const result_opt_value = get_optional_value(result_value, context);
	auto const prev_info = context.push_expression_scope();
	auto const prev_value = context.push_value_reference(get_optional_value(moved_val, context));
	generate_expr_code(optional_move_construct.value_move_expr, context, result_opt_value);
	context.pop_value_reference(prev_value);
	context.pop_expression_scope(prev_info);

	auto const end_bb = context.add_basic_block();
	context.create_jump(end_bb);

	context.set_current_basic_block(begin_bb);
	context.create_conditional_jump(has_value, copy_bb, end_bb);
	context.set_current_basic_block(end_bb);

	return result_value;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_trivial_relocate const &trivial_relocate,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const value = generate_expr_code(trivial_relocate.value, context, {});
	auto const type = value.get_type();

	if (type->is_builtin() || type->is_pointer())
	{
		return value_or_result_address(value.get_value(context), result_address, context);
	}
	else
	{
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(original_expression.src_tokens, type);
		}

		auto const &result_value = result_address.get();
		context.create_const_memcpy(result_value, value, type->size);
		context.create_start_lifetime(result_value);
		return result_value;
	}
}

static expr_value generate_expr_code(
	ast::expr_aggregate_destruct const &aggregate_destruct,
	codegen_context &context
)
{
	auto const value = generate_expr_code(aggregate_destruct.value, context, {});
	bz_assert(value.is_reference());
	bz_assert(
		value.get_type()->is_aggregate()
		&& aggregate_destruct.elem_destruct_calls.size() == value.get_type()->get_aggregate_types().size()
	);

	for (auto const i : bz::iota(0, aggregate_destruct.elem_destruct_calls.size()).reversed())
	{
		auto const elem_value = context.create_struct_gep(value, i);
		if (aggregate_destruct.elem_destruct_calls[i].not_null())
		{
			auto const prev_value = context.push_value_reference(elem_value);
			generate_expr_code(aggregate_destruct.elem_destruct_calls[i], context, {});
			context.pop_value_reference(prev_value);
		}
		else
		{
			context.create_end_lifetime(elem_value);
		}
	}

	return expr_value::get_none();
}

static expr_value generate_expr_code(
	ast::expr_array_destruct const &array_destruct,
	codegen_context &context
)
{
	auto const value = generate_expr_code(array_destruct.value, context, {});
	bz_assert(value.get_type()->is_array());

	auto const loop_info = create_reversed_loop_start(value.get_type()->get_array_size(), context);

	auto const elem_value = context.create_array_gep(value, loop_info.index);
	auto const prev_value = context.push_value_reference(elem_value);
	generate_expr_code(array_destruct.elem_destruct_call, context, {});
	context.pop_value_reference(prev_value);

	create_reversed_loop_end(loop_info, context);

	return expr_value::get_none();
}

static expr_value generate_expr_code(
	ast::expr_optional_destruct const &optional_destruct,
	codegen_context &context
)
{
	auto const value = generate_expr_code(optional_destruct.value, context, {});

	auto const has_value = get_optional_has_value(value, context);
	auto const begin_bb = context.get_current_basic_block();

	auto const destruct_bb = context.add_basic_block();
	context.set_current_basic_block(destruct_bb);

	auto const prev_value = context.push_value_reference(get_optional_value(value, context));
	generate_expr_code(optional_destruct.value_destruct_call, context, {});
	context.pop_value_reference(prev_value);

	auto const end_bb = context.add_basic_block();
	context.create_jump(end_bb);

	context.set_current_basic_block(begin_bb);
	context.create_conditional_jump(has_value, destruct_bb, end_bb);
	context.set_current_basic_block(end_bb);

	bz_assert(value.get_type()->is_aggregate());
	context.create_end_lifetime(get_optional_has_value_ref(value, context));

	return expr_value::get_none();
}

static expr_value generate_expr_code(
	ast::expr_base_type_destruct const &base_type_destruct,
	codegen_context &context
)
{
	auto const value = generate_expr_code(base_type_destruct.value, context, {});
	bz_assert(value.is_reference());
	bz_assert(
		value.get_type()->is_aggregate()
		&& base_type_destruct.member_destruct_calls.size() == value.get_type()->get_aggregate_types().size()
	);

	if (base_type_destruct.destruct_call.not_null())
	{
		auto const prev_value = context.push_value_reference(value);
		generate_expr_code(base_type_destruct.destruct_call, context, {});
		context.pop_value_reference(prev_value);
	}

	for (auto const i : bz::iota(0, base_type_destruct.member_destruct_calls.size()).reversed())
	{
		auto const elem_value = context.create_struct_gep(value, i);
		if (base_type_destruct.member_destruct_calls[i].not_null())
		{
			auto const prev_value = context.push_value_reference(elem_value);
			generate_expr_code(base_type_destruct.member_destruct_calls[i], context, {});
			context.pop_value_reference(prev_value);
		}
		else
		{
			context.create_end_lifetime(elem_value);
		}
	}

	return expr_value::get_none();
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_destruct_value const &destruct_value,
	codegen_context &context
)
{
	auto const value = generate_expr_code(destruct_value.value, context, {});
	bz_assert(value.is_reference());
	context.create_destruct_value_check(
		original_expression.src_tokens,
		value,
		destruct_value.value.get_expr_type().remove_mut_reference()
	);
	if (destruct_value.destruct_call.not_null())
	{
		auto const prev_value = context.push_value_reference(value);
		generate_expr_code(destruct_value.destruct_call, context, {});
		context.pop_value_reference(prev_value);
	}
	else
	{
		context.create_end_lifetime(value);
	}

	return expr_value::get_none();
}

struct pointer_compare_info_t
{
	basic_block_ref begin_bb;
	basic_block_ref neq_bb;
	expr_value are_pointers_equal;
};

static pointer_compare_info_t create_pointer_compare_begin(expr_value lhs, expr_value rhs, codegen_context &context)
{
	bz_assert(lhs.is_reference());
	bz_assert(rhs.is_reference());

	auto const lhs_ptr = expr_value::get_value(lhs.get_reference(), context.get_pointer_type());
	auto const rhs_ptr = expr_value::get_value(rhs.get_reference(), context.get_pointer_type());
	auto const are_pointers_equal = context.create_pointer_cmp_eq(lhs_ptr, rhs_ptr);

	auto const begin_bb = context.get_current_basic_block();
	auto const neq_bb = context.add_basic_block();
	context.set_current_basic_block(neq_bb);

	return {
		.begin_bb = begin_bb,
		.neq_bb = neq_bb,
		.are_pointers_equal = are_pointers_equal,
	};
}

static void create_pointer_compare_end(pointer_compare_info_t const &info, codegen_context &context)
{
	auto const end_bb = context.add_basic_block();
	context.create_jump(end_bb);

	context.set_current_basic_block(info.begin_bb);
	context.create_conditional_jump(info.are_pointers_equal, end_bb, info.neq_bb);
	context.set_current_basic_block(end_bb);
}

static expr_value generate_expr_code(
	ast::expr_aggregate_swap const &aggregate_swap,
	codegen_context &context
)
{
	auto const lhs = generate_expr_code(aggregate_swap.lhs, context, {});
	auto const rhs = generate_expr_code(aggregate_swap.rhs, context, {});
	auto const pointer_compare_info = create_pointer_compare_begin(lhs, rhs, context);

	for (auto const i : bz::iota(0, aggregate_swap.swap_exprs.size()))
	{
		auto const lhs_member = context.create_struct_gep(lhs, i);
		auto const rhs_member = context.create_struct_gep(rhs, i);
		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(lhs_member);
		auto const rhs_prev_value = context.push_value_reference(rhs_member);
		generate_expr_code(aggregate_swap.swap_exprs[i], context, {});
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}

	create_pointer_compare_end(pointer_compare_info, context);
	return expr_value::get_none();
}

static expr_value generate_expr_code(
	ast::expr_array_swap const &array_swap,
	codegen_context &context
)
{
	auto const lhs = generate_expr_code(array_swap.lhs, context, {});
	auto const rhs = generate_expr_code(array_swap.rhs, context, {});
	auto const pointer_compare_info = create_pointer_compare_begin(lhs, rhs, context);

	bz_assert(lhs.get_type()->is_array());
	auto const size = lhs.get_type()->get_array_size();

	auto const loop_info = create_loop_start(size, context);

	auto const lhs_element = context.create_array_gep(lhs, loop_info.index);
	auto const rhs_element = context.create_array_gep(rhs, loop_info.index);
	auto const lhs_prev_value = context.push_value_reference(lhs_element);
	auto const rhs_prev_value = context.push_value_reference(rhs_element);
	generate_expr_code(array_swap.swap_expr, context, {});
	context.pop_value_reference(rhs_prev_value);
	context.pop_value_reference(lhs_prev_value);

	create_loop_end(loop_info, context);

	create_pointer_compare_end(pointer_compare_info, context);
	return expr_value::get_none();
}

static basic_block_ref generate_optional_swap_both(
	expr_value lhs,
	expr_value rhs,
	ast::expression const &value_swap_expr,
	codegen_context &context
)
{
	auto const prev_info = context.push_expression_scope();
	auto const lhs_prev_value = context.push_value_reference(get_optional_value(lhs, context));
	auto const rhs_prev_value = context.push_value_reference(get_optional_value(rhs, context));
	generate_expr_code(value_swap_expr, context, {});
	context.pop_value_reference(rhs_prev_value);
	context.pop_value_reference(lhs_prev_value);
	context.pop_expression_scope(prev_info);
	return context.get_current_basic_block();
}

static basic_block_ref generate_optional_swap_lhs(
	expr_value lhs,
	expr_value rhs,
	ast::expression const &lhs_move_expr,
	codegen_context &context
)
{
	auto const prev_info = context.push_expression_scope();
	auto const rhs_value = get_optional_value(rhs, context);
	auto const prev_value = context.push_value_reference(get_optional_value(lhs, context));
	generate_expr_code(lhs_move_expr, context, rhs_value);
	context.pop_value_reference(prev_value);

	set_optional_has_value(lhs, false, context);
	set_optional_has_value(rhs, true, context);
	context.pop_expression_scope(prev_info);
	return context.get_current_basic_block();
}

static basic_block_ref generate_optional_swap_rhs(
	expr_value lhs,
	expr_value rhs,
	ast::expression const &rhs_move_expr,
	codegen_context &context
)
{
	auto const prev_info = context.push_expression_scope();
	auto const lhs_value = get_optional_value(lhs, context);
	auto const prev_value = context.push_value_reference(get_optional_value(rhs, context));
	generate_expr_code(rhs_move_expr, context, lhs_value);
	context.pop_value_reference(prev_value);

	set_optional_has_value(lhs, true, context);
	set_optional_has_value(rhs, false, context);
	context.pop_expression_scope(prev_info);
	return context.get_current_basic_block();
}

static expr_value generate_expr_code(
	ast::expr_optional_swap const &optional_swap,
	codegen_context &context
)
{
	auto const lhs = generate_expr_code(optional_swap.lhs, context, {});
	auto const rhs = generate_expr_code(optional_swap.rhs, context, {});
	auto const pointer_compare_info = create_pointer_compare_begin(lhs, rhs, context);

	auto const lhs_has_value = get_optional_has_value(lhs, context).get_value(context);
	auto const rhs_has_value = get_optional_has_value(rhs, context).get_value(context);
	auto const any_has_value = context.create_or(lhs_has_value, rhs_has_value);
	auto const begin_bb = context.get_current_basic_block();

	auto const any_has_value_bb = context.add_basic_block();
	context.set_current_basic_block(any_has_value_bb);

	auto const both_have_value = context.create_and(lhs_has_value, rhs_has_value);

	auto const both_have_value_bb = context.add_basic_block();
	context.set_current_basic_block(both_have_value_bb);
	auto const both_have_value_bb_end = generate_optional_swap_both(lhs, rhs, optional_swap.value_swap_expr, context);

	auto const one_has_value_bb = context.add_basic_block();

	auto const lhs_has_value_bb = context.add_basic_block();
	context.set_current_basic_block(lhs_has_value_bb);
	auto const lhs_has_value_bb_end = generate_optional_swap_lhs(lhs, rhs, optional_swap.lhs_move_expr, context);

	auto const rhs_has_value_bb = context.add_basic_block();
	context.set_current_basic_block(rhs_has_value_bb);
	auto const rhs_has_value_bb_end = generate_optional_swap_rhs(lhs, rhs, optional_swap.rhs_move_expr, context);

	auto const end_bb = context.add_basic_block();

	context.set_current_basic_block(begin_bb);
	context.create_conditional_jump(any_has_value, any_has_value_bb, end_bb);

	context.set_current_basic_block(any_has_value_bb);
	context.create_conditional_jump(both_have_value, both_have_value_bb, one_has_value_bb);

	context.set_current_basic_block(both_have_value_bb_end);
	context.create_jump(end_bb);

	context.set_current_basic_block(one_has_value_bb);
	context.create_conditional_jump(lhs_has_value, lhs_has_value_bb, rhs_has_value_bb);

	context.set_current_basic_block(lhs_has_value_bb_end);
	context.create_jump(end_bb);

	context.set_current_basic_block(rhs_has_value_bb_end);
	context.create_jump(end_bb);

	context.set_current_basic_block(end_bb);

	create_pointer_compare_end(pointer_compare_info, context);
	return expr_value::get_none();
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_base_type_swap const &base_type_swap,
	codegen_context &context
)
{
	auto const lhs = generate_expr_code(base_type_swap.lhs, context, {});
	auto const rhs = generate_expr_code(base_type_swap.rhs, context, {});
	auto const pointer_compare_info = create_pointer_compare_begin(lhs, rhs, context);

	bz_assert(lhs.get_type() == rhs.get_type());
	auto const type = lhs.get_type();
	auto const temp = context.create_alloca(original_expression.src_tokens, type);

	// temp = move lhs
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(lhs);
		generate_expr_code(base_type_swap.lhs_move_expr, context, temp);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}
	// lhs = move rhs
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(rhs);
		generate_expr_code(base_type_swap.rhs_move_expr, context, lhs);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}
	// rhs = move temp
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(temp);
		generate_expr_code(base_type_swap.temp_move_expr, context, rhs);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}

	create_pointer_compare_end(pointer_compare_info, context);
	return expr_value::get_none();
}

static expr_value generate_expr_code(
	ast::expr_trivial_swap const &trivial_swap,
	codegen_context &context
)
{
	auto const lhs = generate_expr_code(trivial_swap.lhs, context, {});
	auto const rhs = generate_expr_code(trivial_swap.rhs, context, {});
	auto const pointer_compare_info = create_pointer_compare_begin(lhs, rhs, context);

	bz_assert(lhs.get_type() == rhs.get_type());
	auto const type = lhs.get_type();
	if (type->is_builtin() || type->is_pointer())
	{
		auto const lhs_value = lhs.get_value(context);
		auto const rhs_value = rhs.get_value(context);
		context.create_store(rhs_value, lhs);
		context.create_store(lhs_value, rhs);
	}
	else
	{
		auto const temp = context.create_alloca_without_lifetime(type);

		generate_value_copy(lhs, temp, context); // temp = lhs
		generate_value_copy(rhs, lhs, context);  // lhs = rhs
		generate_value_copy(temp, rhs, context); // rhs = temp
	}

	create_pointer_compare_end(pointer_compare_info, context);
	return expr_value::get_none();
}

static expr_value generate_expr_code(
	ast::expr_aggregate_assign const &aggregate_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expr_code(aggregate_assign.rhs, context, {});
	auto const lhs = generate_expr_code(aggregate_assign.lhs, context, {});
	auto const is_rhs_rvalue = !aggregate_assign.rhs.get_expr_type().is_reference();
	auto const pointer_compare_info = is_rhs_rvalue
		? pointer_compare_info_t{}
		: create_pointer_compare_begin(lhs, rhs, context);

	for (auto const i : bz::iota(0, aggregate_assign.assign_exprs.size()))
	{
		auto const lhs_member = context.create_struct_gep(lhs, i);
		auto const rhs_member = context.create_struct_gep(rhs, i);

		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(lhs_member);
		auto const rhs_prev_value = context.push_value_reference(rhs_member);
		generate_expr_code(aggregate_assign.assign_exprs[i], context, {});
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}

	if (!is_rhs_rvalue)
	{
		create_pointer_compare_end(pointer_compare_info, context);
	}
	return lhs;
}

static expr_value generate_expr_code(
	ast::expr_array_assign const &array_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expr_code(array_assign.rhs, context, {});
	auto const lhs = generate_expr_code(array_assign.lhs, context, {});
	auto const is_rhs_rvalue = !array_assign.rhs.get_expr_type().is_reference();
	auto const pointer_compare_info = is_rhs_rvalue
		? pointer_compare_info_t{}
		: create_pointer_compare_begin(lhs, rhs, context);

	bz_assert(lhs.get_type()->is_array());
	auto const size = lhs.get_type()->get_array_size();

	auto const loop_info = create_loop_start(size, context);

	auto const lhs_element = context.create_array_gep(lhs, loop_info.index);
	auto const rhs_element = context.create_array_gep(rhs, loop_info.index);
	auto const lhs_prev_value = context.push_value_reference(lhs_element);
	auto const rhs_prev_value = context.push_value_reference(rhs_element);
	generate_expr_code(array_assign.assign_expr, context, {});
	context.pop_value_reference(rhs_prev_value);
	context.pop_value_reference(lhs_prev_value);

	create_loop_end(loop_info, context);

	if (!is_rhs_rvalue)
	{
		create_pointer_compare_end(pointer_compare_info, context);
	}
	return lhs;
}

static basic_block_ref generate_optional_assign_both(
	expr_value lhs,
	expr_value rhs,
	ast::expression const &value_assign_expr,
	codegen_context &context
)
{
	auto const prev_info = context.push_expression_scope();
	auto const lhs_prev_value = context.push_value_reference(get_optional_value(lhs, context));
	auto const rhs_prev_value = context.push_value_reference(get_optional_value(rhs, context));
	generate_expr_code(value_assign_expr, context, {});
	context.pop_value_reference(rhs_prev_value);
	context.pop_value_reference(lhs_prev_value);
	context.pop_expression_scope(prev_info);
	return context.get_current_basic_block();
}

static basic_block_ref generate_optional_assign_lhs(
	expr_value lhs,
	ast::expression const &value_destruct_expr,
	codegen_context &context
)
{
	auto const prev_value = context.push_value_reference(get_optional_value(lhs, context));
	generate_expr_code(value_destruct_expr, context, {});
	context.pop_value_reference(prev_value);

	set_optional_has_value(lhs, false, context);
	return context.get_current_basic_block();
}

static basic_block_ref generate_optional_assign_rhs(
	expr_value lhs,
	expr_value rhs,
	ast::expression const &value_construct_expr,
	codegen_context &context
)
{
	auto const prev_info = context.push_expression_scope();
	auto const lhs_value = get_optional_value(lhs, context);
	auto const prev_value = context.push_value_reference(get_optional_value(rhs, context));
	generate_expr_code(value_construct_expr, context, lhs_value);
	context.pop_value_reference(prev_value);
	context.pop_expression_scope(prev_info);

	set_optional_has_value(lhs, true, context);
	return context.get_current_basic_block();
}

static expr_value generate_expr_code(
	ast::expr_optional_assign const &optional_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expr_code(optional_assign.rhs, context, {});
	auto const lhs = generate_expr_code(optional_assign.lhs, context, {});
	bz_assert(!lhs.get_type()->is_pointer());

	auto const is_rhs_rvalue = !optional_assign.rhs.get_expr_type().is_reference();
	auto const pointer_compare_info = is_rhs_rvalue
		? pointer_compare_info_t{}
		: create_pointer_compare_begin(lhs, rhs, context);

	auto const lhs_has_value = get_optional_has_value(lhs, context).get_value(context);
	auto const rhs_has_value = get_optional_has_value(rhs, context).get_value(context);
	auto const any_has_value = context.create_or(lhs_has_value, rhs_has_value);
	auto const begin_bb = context.get_current_basic_block();

	auto const any_has_value_bb = context.add_basic_block();
	context.set_current_basic_block(any_has_value_bb);

	auto const both_have_value = context.create_and(lhs_has_value, rhs_has_value);

	auto const both_have_value_bb = context.add_basic_block();
	context.set_current_basic_block(both_have_value_bb);
	auto const both_have_value_bb_end = generate_optional_assign_both(lhs, rhs, optional_assign.value_assign_expr, context);

	auto const one_has_value_bb = context.add_basic_block();

	auto const lhs_has_value_bb = context.add_basic_block();
	context.set_current_basic_block(lhs_has_value_bb);
	auto const lhs_has_value_bb_end = generate_optional_assign_lhs(lhs, optional_assign.value_destruct_expr, context);

	auto const rhs_has_value_bb = context.add_basic_block();
	context.set_current_basic_block(rhs_has_value_bb);
	auto const rhs_has_value_bb_end = generate_optional_assign_rhs(lhs, rhs, optional_assign.value_construct_expr, context);

	auto const end_bb = context.add_basic_block();

	context.set_current_basic_block(begin_bb);
	context.create_conditional_jump(any_has_value, any_has_value_bb, end_bb);

	context.set_current_basic_block(any_has_value_bb);
	context.create_conditional_jump(both_have_value, both_have_value_bb, one_has_value_bb);

	context.set_current_basic_block(both_have_value_bb_end);
	context.create_jump(end_bb);

	context.set_current_basic_block(one_has_value_bb);
	context.create_conditional_jump(lhs_has_value, lhs_has_value_bb, rhs_has_value_bb);

	context.set_current_basic_block(lhs_has_value_bb_end);
	context.create_jump(end_bb);

	context.set_current_basic_block(rhs_has_value_bb_end);
	context.create_jump(end_bb);

	context.set_current_basic_block(end_bb);

	if (!is_rhs_rvalue)
	{
		create_pointer_compare_end(pointer_compare_info, context);
	}
	return lhs;
}

static expr_value generate_expr_code(
	ast::expr_optional_null_assign const &optional_null_assign,
	codegen_context &context
)
{
	generate_expr_code(optional_null_assign.rhs, context, {});
	auto const lhs = generate_expr_code(optional_null_assign.lhs, context, {});
	bz_assert(lhs.is_reference());

	if (lhs.get_type()->is_pointer())
	{
		context.create_store(context.create_const_ptr_null(), lhs);
	}
	else if (optional_null_assign.value_destruct_expr.not_null())
	{
		auto const has_value = get_optional_has_value(lhs, context).get_value(context);
		auto const begin_bb = context.get_current_basic_block();

		auto const destruct_bb = context.add_basic_block();
		context.set_current_basic_block(destruct_bb);

		auto const prev_value = context.push_value_reference(get_optional_value(lhs, context));
		generate_expr_code(optional_null_assign.value_destruct_expr, context, {});
		context.pop_value_reference(prev_value);

		set_optional_has_value(lhs, false, context);

		auto const end_bb = context.add_basic_block();
		context.create_jump(end_bb);

		context.set_current_basic_block(begin_bb);
		context.create_conditional_jump(has_value, destruct_bb, end_bb);

		context.set_current_basic_block(end_bb);
	}
	else
	{
		set_optional_has_value(lhs, false, context);
	}

	return lhs;
}

static expr_value generate_expr_code(
	ast::expr_optional_value_assign const &optional_value_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expr_code(optional_value_assign.rhs, context, {});
	auto const lhs = generate_expr_code(optional_value_assign.lhs, context, {});

	if (lhs.get_type()->is_pointer())
	{
		context.create_store(rhs, lhs);
		return lhs;
	}

	auto const has_value = get_optional_has_value(lhs, context).get_value(context);
	auto const begin_bb = context.get_current_basic_block();

	auto const assign_bb = context.add_basic_block();
	context.set_current_basic_block(assign_bb);
	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(get_optional_value(lhs, context));
		auto const rhs_prev_value = context.push_value_reference(rhs);
		generate_expr_code(optional_value_assign.value_assign_expr, context, {});
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}
	auto const assign_bb_end = context.get_current_basic_block();

	auto const construct_bb = context.add_basic_block();
	context.set_current_basic_block(construct_bb);
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(rhs);
		auto const lhs_value = get_optional_value(lhs, context);
		generate_expr_code(optional_value_assign.value_construct_expr, context, lhs_value);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);

		set_optional_has_value(lhs, true, context);
	}

	auto const end_bb = context.add_basic_block();
	context.create_jump(end_bb);

	context.set_current_basic_block(begin_bb);
	context.create_conditional_jump(has_value, assign_bb, construct_bb);

	context.set_current_basic_block(assign_bb_end);
	context.create_jump(end_bb);

	context.set_current_basic_block(end_bb);

	return lhs;
}

static expr_value generate_expr_code(
	ast::expr_optional_reference_value_assign const &optional_reference_value_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expr_code(optional_reference_value_assign.rhs, context, {});
	auto const lhs = generate_expr_code(optional_reference_value_assign.lhs, context, {});
	bz_assert(lhs.is_reference());
	bz_assert(rhs.is_reference());

	auto const rhs_reference_value = expr_value::get_value(rhs.get_reference(), context.get_pointer_type());
	context.create_store(rhs_reference_value, lhs);

	return lhs;
}

static expr_value generate_expr_code(
	ast::expr_base_type_assign const &base_type_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expr_code(base_type_assign.rhs, context, {});
	auto const lhs = generate_expr_code(base_type_assign.lhs, context, {});
	auto const is_rhs_rvalue = !base_type_assign.rhs.get_expr_type().is_reference();
	auto const pointer_compare_info = is_rhs_rvalue
		? pointer_compare_info_t{}
		: create_pointer_compare_begin(lhs, rhs, context);

	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(lhs);
		generate_expr_code(base_type_assign.lhs_destruct_expr, context, {});
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}

	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(rhs);
		generate_expr_code(base_type_assign.rhs_copy_expr, context, lhs);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}

	if (!is_rhs_rvalue)
	{
		create_pointer_compare_end(pointer_compare_info, context);
	}
	return lhs;
}

static expr_value generate_expr_code(
	ast::expr_trivial_assign const &trivial_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expr_code(trivial_assign.rhs, context, {});
	auto const lhs = generate_expr_code(trivial_assign.lhs, context, {});
	bz_assert(lhs.is_reference());

	if (rhs.is_reference())
	{
		auto const pointer_compare_info = create_pointer_compare_begin(lhs, rhs, context);

		generate_value_copy(rhs, lhs, context);

		create_pointer_compare_end(pointer_compare_info, context);
	}
	else
	{
		generate_value_copy(rhs, lhs, context);
	}

	return lhs;
}

static expr_value generate_expr_code(
	ast::expr_member_access const &member_access,
	codegen_context &context
)
{
	auto const base = generate_expr_code(member_access.base, context, {});
	bz_assert(base.is_reference());
	bz_assert(base.get_type()->is_aggregate());

	bz_assert(member_access.base.get_expr_type().remove_mut_reference().is<ast::ts_base_type>());
	auto const info = member_access.base.get_expr_type().remove_mut_reference().get<ast::ts_base_type>().info;
	auto const &accessed_type = info->member_variables[member_access.index]->get_type();
	if (accessed_type.is_reference())
	{
		auto const ref_ref = context.create_struct_gep(base, member_access.index);
		bz_assert(ref_ref.get_type()->is_pointer());
		auto const ref_value = context.create_load(ref_ref);
		return expr_value::get_reference(
			ref_value.get_value_as_instruction(context),
			get_type(accessed_type.remove_reference(), context)
		);
	}
	else
	{
		return context.create_struct_gep(base, member_access.index);
	}
}

static expr_value generate_expr_code(
	lex::src_tokens const &src_tokens,
	ast::expr_optional_extract_value const &optional_extract_value,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const optional_value = generate_expr_code(optional_extract_value.optional_value, context, {});
	context.create_optional_get_value_check(src_tokens, get_optional_has_value(optional_value, context));

	auto const optional_value_type = optional_extract_value.optional_value.get_expr_type().remove_any_mut();
	if (optional_value_type.is_optional_reference())
	{
		bz_assert(optional_value.get_type()->is_pointer());
		auto const reference_value = optional_value.get_value_as_instruction(context);
		auto const type = get_type(optional_value_type.get_optional_reference(), context);
		bz_assert(!result_address.has_value());
		return expr_value::get_reference(reference_value, type);
	}
	else
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(get_optional_value(optional_value, context));
		auto const result_value = generate_expr_code(optional_extract_value.value_move_expr, context, result_address);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);

		return result_value;
	}
}

static expr_value generate_expr_code(
	ast::expr_rvalue_member_access const &rvalue_member_access,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const base = generate_expr_code(rvalue_member_access.base, context, {});

	bz_assert(rvalue_member_access.base.get_expr_type().remove_mut_reference().is<ast::ts_base_type>());
	auto const info = rvalue_member_access.base.get_expr_type().remove_mut_reference().get<ast::ts_base_type>().info;
	auto const &accessed_type = info->member_variables[rvalue_member_access.index]->get_type();
	bz_assert(!result_address.has_value() || !accessed_type.is_reference());

	auto const prev_info = context.push_expression_scope();
	expr_value result = expr_value::get_none();
	for (auto const i : bz::iota(0, rvalue_member_access.member_refs.size()))
	{
		if (rvalue_member_access.member_refs[i].is_null())
		{
			continue;
		}

		auto const member_value = [&]() {
			if (i == rvalue_member_access.index && accessed_type.is_reference())
			{
				auto const ref_ref = context.create_struct_gep(base, i);
				bz_assert(ref_ref.get_type()->is_pointer());
				auto const ref_value = context.create_load(ref_ref);
				return expr_value::get_reference(
					ref_value.get_value_as_instruction(context),
					get_type(accessed_type.remove_reference(), context)
				);
			}
			else
			{
				return context.create_struct_gep(base, i);
			}
		}();

		auto const prev_value = context.push_value_reference(member_value);
		if (i == rvalue_member_access.index)
		{
			auto const prev_info = context.push_expression_scope();
			result = generate_expr_code(rvalue_member_access.member_refs[i], context, result_address);
			context.pop_expression_scope(prev_info);
		}
		else
		{
			generate_expr_code(rvalue_member_access.member_refs[i], context, {});
		}
		context.pop_value_reference(prev_value);
	}
	context.pop_expression_scope(prev_info);

	return result;
}

static expr_value generate_expr_code(
	ast::expr_type_member_access const &type_member_access,
	codegen_context &context
)
{
	auto const result = context.get_variable(type_member_access.var_decl);

	if (result.is_none())
	{
		context.create_error(
			lex::src_tokens::from_single_token(type_member_access.member),
			bz::format("member '{}' cannot be used in a constant expression", type_member_access.member->value)
		);
		auto const type = get_type(type_member_access.var_decl->get_type(), context);
		return context.get_dummy_value(type);
	}

	return result;
}

static expr_value generate_expr_code(
	ast::expr_compound const &compound_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const prev_info = context.push_expression_scope();
	for (auto const &stmt : compound_expr.statements)
	{
		if (context.has_terminator())
		{
			break;
		}
		generate_stmt_code(stmt, context);
	}

	if (compound_expr.final_expr.is_null())
	{
		context.pop_expression_scope(prev_info);
		return expr_value::get_none();
	}
	else
	{
		auto const result = generate_expr_code(compound_expr.final_expr, context, result_address);
		context.pop_expression_scope(prev_info);
		return result;
	}
}

static expr_value generate_expr_code(
	ast::expr_if const &if_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const condition_prev_info = context.push_expression_scope();
	auto const condition = if_expr.condition.is_error()
		? context.get_dummy_value(context.get_builtin_type(builtin_type_kind::i1))
		: generate_expr_code(if_expr.condition, context, {}).get_value(context);
	context.pop_expression_scope(condition_prev_info);

	auto const begin_bb = context.get_current_basic_block();

	auto const then_bb = context.add_basic_block();
	context.set_current_basic_block(then_bb);

	auto const then_prev_info = context.push_expression_scope();
	generate_expr_code(if_expr.then_block, context, result_address);
	context.pop_expression_scope(then_prev_info);

	if (if_expr.else_block.is_null())
	{
		auto const end_bb = context.add_basic_block();
		if (!context.has_terminator())
		{
			context.create_jump(end_bb); // then -> end
		}
		context.set_current_basic_block(begin_bb);
		context.create_conditional_jump(condition, then_bb, end_bb);
		context.set_current_basic_block(end_bb);
		bz_assert(!result_address.has_value());
		return expr_value::get_none();
	}

	auto const then_bb_end = context.get_current_basic_block();
	auto const else_bb = context.add_basic_block();
	context.set_current_basic_block(else_bb);

	auto const else_prev_info = context.push_expression_scope();
	generate_expr_code(if_expr.else_block, context, result_address);
	context.pop_expression_scope(else_prev_info);

	auto const end_bb = context.add_basic_block();
	context.create_jump(end_bb); // else -> end
	context.set_current_basic_block(then_bb_end);
	context.create_jump(end_bb); // then -> end

	context.set_current_basic_block(begin_bb);
	context.create_conditional_jump(condition, then_bb, else_bb);
	context.set_current_basic_block(end_bb);

	if (result_address.has_value())
	{
		return result_address.get();
	}
	else
	{
		return expr_value::get_none();
	}
}

static expr_value generate_expr_code(
	ast::expr_if_consteval const &if_consteval_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	bz_assert(if_consteval_expr.condition.is_constant());
	bz_assert(if_consteval_expr.condition.get_constant_value().is_boolean());
	auto const condition = if_consteval_expr.condition.get_constant_value().get_boolean();
	if (condition)
	{
		return generate_expr_code(if_consteval_expr.then_block, context, result_address);
	}
	else if (if_consteval_expr.else_block.not_null())
	{
		return generate_expr_code(if_consteval_expr.else_block, context, result_address);
	}
	else
	{
		bz_assert(!result_address.has_value());
		return expr_value::get_none();
	}
}

static expr_value generate_integral_switch_code(
	ast::expression const &original_expression,
	ast::expr_switch const &switch_expr,
	expr_value matched_value,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const default_bb = context.add_basic_block();
	auto const has_default = switch_expr.default_case.not_null();
	bz_assert(result_address.has_value() ? switch_expr.is_complete : true);
	auto const begin_bb = context.get_current_basic_block();

	auto const case_count = switch_expr.cases.transform([](auto const &switch_case) { return switch_case.values.size(); }).sum();
	auto cases = bz::vector<unresolved_switch::value_bb_pair>();
	cases.reserve(case_count);
	auto case_bb_ends = bz::vector<basic_block_ref>();
	case_bb_ends.reserve(case_count + 1);

	if (has_default)
	{
		context.set_current_basic_block(default_bb);
		auto const prev_info = context.push_expression_scope();
		generate_expr_code(switch_expr.default_case, context, result_address);
		context.pop_expression_scope(prev_info);
		if (!context.has_terminator())
		{
			case_bb_ends.push_back(context.get_current_basic_block());
		}
	}
	else if (switch_expr.is_complete)
	{
		context.set_current_basic_block(default_bb);
		context.create_error(original_expression.src_tokens, "invalid value used in 'switch'");
		context.create_unreachable();
	}
	else
	{
		case_bb_ends.push_back(default_bb);
	}

	for (auto const &[case_vals, case_expr] : switch_expr.cases)
	{
		auto const bb = context.add_basic_block();
		for (auto const &expr : case_vals)
		{
			bz_assert(expr.is_constant());
			auto const &value = expr.get_constant_value();
			switch (value.kind())
			{
			static_assert(ast::constant_value::variant_count == 19);
			case ast::constant_value_kind::sint:
				cases.push_back({ .value = static_cast<uint64_t>(value.get_sint()), .bb = bb, });
				break;
			case ast::constant_value_kind::uint:
				cases.push_back({ .value = value.get_uint(), .bb = bb, });
				break;
			case ast::constant_value_kind::u8char:
				cases.push_back({ .value = static_cast<uint64_t>(value.get_u8char()), .bb = bb, });
				break;
			case ast::constant_value_kind::boolean:
				cases.push_back({ .value = static_cast<uint64_t>(value.get_boolean()), .bb = bb, });
				break;
			case ast::constant_value_kind::enum_:
				cases.push_back({ .value = value.get_enum().value, .bb = bb, });
				break;
			default:
				bz_unreachable;
			}
		}

		context.set_current_basic_block(bb);
		auto const prev_info = context.push_expression_scope();
		generate_expr_code(case_expr, context, result_address);
		context.pop_expression_scope(prev_info);

		if (!context.has_terminator())
		{
			case_bb_ends.push_back(context.get_current_basic_block());
		}
	}

	auto const end_bb = context.add_basic_block();
	for (auto const case_end_bb : case_bb_ends)
	{
		context.set_current_basic_block(case_end_bb);
		context.create_jump(end_bb);
	}

	context.set_current_basic_block(begin_bb);
	context.create_switch(matched_value, std::move(cases), default_bb);
	context.set_current_basic_block(end_bb);

	if (result_address.has_value())
	{
		return result_address.get();
	}
	else
	{
		return expr_value::get_none();
	}
}

static expr_value generate_string_switch_code(
	ast::expression const &original_expression,
	ast::expr_switch const &switch_expr,
	expr_value begin_ptr,
	expr_value end_ptr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const default_bb = context.add_basic_block();
	auto const has_default = switch_expr.default_case.not_null();
	bz_assert(result_address.has_value() ? switch_expr.is_complete : true);
	auto const begin_bb = context.get_current_basic_block();

	auto const case_count = switch_expr.cases.transform([](auto const &switch_case) { return switch_case.values.size(); }).sum();
	auto cases = bz::vector<unresolved_switch_str::value_bb_pair>();
	cases.reserve(case_count);
	auto case_bb_ends = bz::vector<basic_block_ref>();
	case_bb_ends.reserve(case_count + 1);

	if (has_default)
	{
		context.set_current_basic_block(default_bb);
		auto const prev_info = context.push_expression_scope();
		generate_expr_code(switch_expr.default_case, context, result_address);
		context.pop_expression_scope(prev_info);
		if (!context.has_terminator())
		{
			case_bb_ends.push_back(context.get_current_basic_block());
		}
	}
	else if (switch_expr.is_complete)
	{
		context.set_current_basic_block(default_bb);
		context.create_error(original_expression.src_tokens, "invalid value used in 'switch'");
		context.create_unreachable();
	}
	else
	{
		case_bb_ends.push_back(default_bb);
	}

	for (auto const &[case_vals, case_expr] : switch_expr.cases)
	{
		auto const bb = context.add_basic_block();
		for (auto const &expr : case_vals)
		{
			bz_assert(expr.is_constant());
			auto const &value = expr.get_constant_value();
			bz_assert(value.is_string());
			cases.push_back({ .value = value.get_string(), .bb = bb });
		}

		context.set_current_basic_block(bb);
		auto const prev_info = context.push_expression_scope();
		generate_expr_code(case_expr, context, result_address);
		context.pop_expression_scope(prev_info);

		if (!context.has_terminator())
		{
			case_bb_ends.push_back(context.get_current_basic_block());
		}
	}

	auto const end_bb = context.add_basic_block();
	for (auto const case_end_bb : case_bb_ends)
	{
		context.set_current_basic_block(case_end_bb);
		context.create_jump(end_bb);
	}

	context.set_current_basic_block(begin_bb);
	context.create_string_switch(begin_ptr, end_ptr, std::move(cases), default_bb);
	context.set_current_basic_block(end_bb);

	if (result_address.has_value())
	{
		return result_address.get();
	}
	else
	{
		return expr_value::get_none();
	}
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_switch const &switch_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	auto const matched_value_prev_info = context.push_expression_scope();
	auto const matched_value = generate_expr_code(switch_expr.matched_expr, context, {});

	if (matched_value.get_type()->is_integer_type())
	{
		auto const matched_value_value = matched_value.get_value(context);
		context.pop_expression_scope(matched_value_prev_info);

		return generate_integral_switch_code(original_expression, switch_expr, matched_value_value, context, result_address);
	}
	else
	{
		auto const begin_ptr = context.create_struct_gep(matched_value, 0).get_value(context);
		auto const end_ptr   = context.create_struct_gep(matched_value, 1).get_value(context);
		context.pop_expression_scope(matched_value_prev_info);

		return generate_string_switch_code(original_expression, switch_expr, begin_ptr, end_ptr, context, result_address);
	}
}

static expr_value generate_expr_code(
	lex::src_tokens const &src_tokens,
	ast::expr_break const &,
	codegen_context &context
)
{
	if (!context.loop_info.in_loop)
	{
		context.create_error(src_tokens, "'break' hit in compile time execution without an outer loop");
		context.create_unreachable();
	}
	else
	{
		context.emit_loop_destruct_operations();
		context.create_jump(context.loop_info.break_bb);
	}
	return expr_value::get_none();
}

static expr_value generate_expr_code(
	lex::src_tokens const &src_tokens,
	ast::expr_continue const &,
	codegen_context &context
)
{
	if (!context.loop_info.in_loop)
	{
		context.create_error(src_tokens, "'continue' hit in compile time execution without an outer loop");
		context.create_unreachable();
	}
	else
	{
		context.emit_loop_destruct_operations();
		context.create_jump(context.loop_info.continue_bb);
	}
	return expr_value::get_none();
}

static expr_value generate_expr_code(
	lex::src_tokens const &src_tokens,
	ast::expr_unreachable const &,
	codegen_context &context
)
{
	context.create_error(src_tokens, "'unreachable' hit in compile time execution");
	context.create_unreachable();
	return expr_value::get_none();
}

static expr_value generate_expr_code(
	ast::expr_generic_type_instantiation const &,
	codegen_context &,
	bz::optional<expr_value>
)
{
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expr_bitcode_value_reference const &bitcode_value_reference,
	codegen_context &context
)
{
	return context.get_value_reference(bitcode_value_reference.index);
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::expr_t const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	switch (expr.kind())
	{
	static_assert(ast::expr_t::variant_count == 72);
	case ast::expr_t::index<ast::expr_variable_name>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(original_expression, expr.get<ast::expr_variable_name>(), context);
	case ast::expr_t::index<ast::expr_function_name>:
		return generate_expr_code(expr.get<ast::expr_function_name>(), context, result_address);
	case ast::expr_t::index<ast::expr_function_alias_name>:
		return generate_expr_code(expr.get<ast::expr_function_alias_name>(), context, result_address);
	case ast::expr_t::index<ast::expr_function_overload_set>:
		return generate_expr_code(expr.get<ast::expr_function_overload_set>(), context, result_address);
	case ast::expr_t::index<ast::expr_struct_name>:
		return generate_expr_code(expr.get<ast::expr_struct_name>(), context, result_address);
	case ast::expr_t::index<ast::expr_enum_name>:
		return generate_expr_code(expr.get<ast::expr_enum_name>(), context, result_address);
	case ast::expr_t::index<ast::expr_type_alias_name>:
		return generate_expr_code(expr.get<ast::expr_type_alias_name>(), context, result_address);
	case ast::expr_t::index<ast::expr_integer_literal>:
		return generate_expr_code(expr.get<ast::expr_integer_literal>(), context, result_address);
	case ast::expr_t::index<ast::expr_null_literal>:
		return generate_expr_code(expr.get<ast::expr_null_literal>(), context, result_address);
	case ast::expr_t::index<ast::expr_enum_literal>:
		return generate_expr_code(expr.get<ast::expr_enum_literal>(), context, result_address);
	case ast::expr_t::index<ast::expr_typed_literal>:
		return generate_expr_code(expr.get<ast::expr_typed_literal>(), context, result_address);
	case ast::expr_t::index<ast::expr_placeholder_literal>:
		return generate_expr_code(expr.get<ast::expr_placeholder_literal>(), context, result_address);
	case ast::expr_t::index<ast::expr_typename_literal>:
		return generate_expr_code(expr.get<ast::expr_typename_literal>(), context, result_address);
	case ast::expr_t::index<ast::expr_tuple>:
		return generate_expr_code(expr.get<ast::expr_tuple>(), context, result_address);
	case ast::expr_t::index<ast::expr_unary_op>:
		return generate_expr_code(expr.get<ast::expr_unary_op>(), context, result_address);
	case ast::expr_t::index<ast::expr_binary_op>:
		return generate_expr_code(expr.get<ast::expr_binary_op>(), context, result_address);
	case ast::expr_t::index<ast::expr_tuple_subscript>:
		return generate_expr_code(expr.get<ast::expr_tuple_subscript>(), context, result_address);
	case ast::expr_t::index<ast::expr_rvalue_tuple_subscript>:
		return generate_expr_code(expr.get<ast::expr_rvalue_tuple_subscript>(), context, result_address);
	case ast::expr_t::index<ast::expr_subscript>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(original_expression.src_tokens, expr.get<ast::expr_subscript>(), context);
	case ast::expr_t::index<ast::expr_rvalue_array_subscript>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(original_expression.src_tokens, expr.get<ast::expr_rvalue_array_subscript>(), context);
	case ast::expr_t::index<ast::expr_function_call>:
		return generate_expr_code(original_expression, expr.get<ast::expr_function_call>(), context, result_address);
	case ast::expr_t::index<ast::expr_indirect_function_call>:
		return generate_expr_code(expr.get<ast::expr_indirect_function_call>(), context, result_address);
	case ast::expr_t::index<ast::expr_cast>:
		return generate_expr_code(original_expression, expr.get<ast::expr_cast>(), context, result_address);
	case ast::expr_t::index<ast::expr_bit_cast>:
		return generate_expr_code(original_expression, expr.get<ast::expr_bit_cast>(), context, result_address);
	case ast::expr_t::index<ast::expr_optional_cast>:
		return generate_expr_code(original_expression, expr.get<ast::expr_optional_cast>(), context, result_address);
	case ast::expr_t::index<ast::expr_take_reference>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_take_reference>(), context);
	case ast::expr_t::index<ast::expr_take_move_reference>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(original_expression, expr.get<ast::expr_take_move_reference>(), context);
	case ast::expr_t::index<ast::expr_aggregate_init>:
		return generate_expr_code(original_expression, expr.get<ast::expr_aggregate_init>(), context, result_address);
	case ast::expr_t::index<ast::expr_array_value_init>:
		return generate_expr_code(original_expression, expr.get<ast::expr_array_value_init>(), context, result_address);
	case ast::expr_t::index<ast::expr_aggregate_default_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_aggregate_default_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_array_default_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_array_default_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_optional_default_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_optional_default_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_builtin_default_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_builtin_default_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_aggregate_copy_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_aggregate_copy_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_array_copy_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_array_copy_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_optional_copy_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_optional_copy_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_trivial_copy_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_trivial_copy_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_aggregate_move_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_aggregate_move_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_array_move_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_array_move_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_optional_move_construct>:
		return generate_expr_code(original_expression, expr.get<ast::expr_optional_move_construct>(), context, result_address);
	case ast::expr_t::index<ast::expr_trivial_relocate>:
		return generate_expr_code(original_expression, expr.get<ast::expr_trivial_relocate>(), context, result_address);
	case ast::expr_t::index<ast::expr_aggregate_destruct>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_aggregate_destruct>(), context);
	case ast::expr_t::index<ast::expr_array_destruct>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_array_destruct>(), context);
	case ast::expr_t::index<ast::expr_optional_destruct>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_optional_destruct>(), context);
	case ast::expr_t::index<ast::expr_base_type_destruct>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_base_type_destruct>(), context);
	case ast::expr_t::index<ast::expr_destruct_value>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(original_expression, expr.get<ast::expr_destruct_value>(), context);
	case ast::expr_t::index<ast::expr_aggregate_swap>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_aggregate_swap>(), context);
	case ast::expr_t::index<ast::expr_array_swap>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_array_swap>(), context);
	case ast::expr_t::index<ast::expr_optional_swap>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_optional_swap>(), context);
	case ast::expr_t::index<ast::expr_base_type_swap>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(original_expression, expr.get<ast::expr_base_type_swap>(), context);
	case ast::expr_t::index<ast::expr_trivial_swap>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_trivial_swap>(), context);
	case ast::expr_t::index<ast::expr_aggregate_assign>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_aggregate_assign>(), context);
	case ast::expr_t::index<ast::expr_array_assign>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_array_assign>(), context);
	case ast::expr_t::index<ast::expr_optional_assign>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_optional_assign>(), context);
	case ast::expr_t::index<ast::expr_optional_null_assign>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_optional_null_assign>(), context);
	case ast::expr_t::index<ast::expr_optional_value_assign>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_optional_value_assign>(), context);
	case ast::expr_t::index<ast::expr_optional_reference_value_assign>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_optional_reference_value_assign>(), context);
	case ast::expr_t::index<ast::expr_base_type_assign>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_base_type_assign>(), context);
	case ast::expr_t::index<ast::expr_trivial_assign>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_trivial_assign>(), context);
	case ast::expr_t::index<ast::expr_member_access>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_member_access>(), context);
	case ast::expr_t::index<ast::expr_optional_extract_value>:
		return generate_expr_code(original_expression.src_tokens, expr.get<ast::expr_optional_extract_value>(), context, result_address);
	case ast::expr_t::index<ast::expr_rvalue_member_access>:
		return generate_expr_code(expr.get<ast::expr_rvalue_member_access>(), context, result_address);
	case ast::expr_t::index<ast::expr_type_member_access>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_type_member_access>(), context);
	case ast::expr_t::index<ast::expr_compound>:
		return generate_expr_code(expr.get<ast::expr_compound>(), context, result_address);
	case ast::expr_t::index<ast::expr_if>:
		return generate_expr_code(expr.get<ast::expr_if>(), context, result_address);
	case ast::expr_t::index<ast::expr_if_consteval>:
		return generate_expr_code(expr.get<ast::expr_if_consteval>(), context, result_address);
	case ast::expr_t::index<ast::expr_switch>:
		return generate_expr_code(original_expression, expr.get<ast::expr_switch>(), context, result_address);
	case ast::expr_t::index<ast::expr_break>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(original_expression.src_tokens, expr.get<ast::expr_break>(), context);
	case ast::expr_t::index<ast::expr_continue>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(original_expression.src_tokens, expr.get<ast::expr_continue>(), context);
	case ast::expr_t::index<ast::expr_unreachable>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(original_expression.src_tokens, expr.get<ast::expr_unreachable>(), context);
	case ast::expr_t::index<ast::expr_generic_type_instantiation>:
		return generate_expr_code(expr.get<ast::expr_generic_type_instantiation>(), context, result_address);
	case ast::expr_t::index<ast::expr_bitcode_value_reference>:
		bz_assert(!result_address.has_value());
		return generate_expr_code(expr.get<ast::expr_bitcode_value_reference>(), context);
	default:
		bz_unreachable;
	}
}

static expr_value get_constant_value(
	lex::src_tokens const &src_tokens,
	ast::constant_value const &value,
	ast::typespec_view type,
	ast::constant_expression const *const_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
);

static bool is_zero_value(ast::constant_value const &value)
{
	switch (value.kind())
	{
	static_assert(ast::constant_value::variant_count == 19);
	case ast::constant_value_kind::sint:
		return value.get_sint() == 0;
	case ast::constant_value_kind::uint:
		return value.get_uint() == 0;
	case ast::constant_value_kind::float32:
		return bit_cast<uint32_t>(value.get_float32()) == 0;
	case ast::constant_value_kind::float64:
		return bit_cast<uint64_t>(value.get_float64()) == 0;
	case ast::constant_value_kind::u8char:
		return value.get_u8char() == 0;
	case ast::constant_value_kind::string:
		return value.get_string() == "";
	case ast::constant_value_kind::boolean:
		return value.get_boolean() == false;
	case ast::constant_value_kind::null:
		return true;
	case ast::constant_value_kind::void_:
		return true;
	case ast::constant_value_kind::enum_:
		return value.get_enum().value == 0;
	case ast::constant_value_kind::array:
		return value.get_array().is_all([](auto const &value) { return is_zero_value(value); });
	case ast::constant_value_kind::sint_array:
		return value.get_sint_array().is_all([](auto const value) { return value == 0; });
	case ast::constant_value_kind::uint_array:
		return value.get_sint_array().is_all([](auto const value) { return value == 0; });
	case ast::constant_value_kind::float32_array:
		return value.get_float32_array().is_all([](auto const value) { return bit_cast<uint32_t>(value) == 0; });
	case ast::constant_value_kind::float64_array:
		return value.get_float64_array().is_all([](auto const value) { return bit_cast<uint64_t>(value) == 0; });
	case ast::constant_value_kind::tuple:
		return value.get_tuple().is_all([](auto const &value) { return is_zero_value(value); });
	case ast::constant_value_kind::function:
		return false;
	case ast::constant_value_kind::aggregate:
		return value.get_aggregate().is_all([](auto const &value) { return is_zero_value(value); });
	case ast::constant_value_kind::type:
		bz_unreachable;
	default:
		bz_unreachable;
	}
}

static ast::typespec_view flattened_array_elem_type(ast::ts_array const &array_t)
{
	ast::typespec_view result = array_t.elem_type;
	while (result.is<ast::ts_array>())
	{
		result = result.get<ast::ts_array>().elem_type;
	}
	return result;
}

static void get_nonzero_constant_array_value(
	lex::src_tokens const &src_tokens,
	bz::array_view<ast::constant_value const> values,
	ast::ts_array const &array_type,
	codegen_context &context,
	expr_value result_address
)
{
	bz_assert(result_address.get_type()->is_array());
	if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(result_address.get_type()->get_array_element_type()->is_array());
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		for (auto const i : bz::iota(0, array_type.size))
		{
			auto const begin_index = i * stride;
			auto const sub_array = values.slice(begin_index, begin_index + stride);
			auto const elem_result_address = context.create_struct_gep(result_address, i);
			get_nonzero_constant_array_value(src_tokens, sub_array, array_type.elem_type.get<ast::ts_array>(), context, elem_result_address);
		}
	}
	else
	{
		for (auto const i : bz::iota(0, array_type.size))
		{
			auto const elem_result_address = context.create_struct_gep(result_address, i);
			get_constant_value(src_tokens, values[i], array_type.elem_type, nullptr, context, elem_result_address);
		}
	}
}

static void get_constant_array_value(
	lex::src_tokens const &src_tokens,
	bz::array_view<ast::constant_value const> values,
	ast::ts_array const &array_type,
	codegen_context &context,
	expr_value result_address
)
{
	if (values.is_all([](auto const &value) { return is_zero_value(value); }))
	{
		context.create_const_memset_zero(result_address);
		context.create_start_lifetime(result_address);
	}
	else
	{
		get_nonzero_constant_array_value(src_tokens, values, array_type, context, result_address);
	}
}

template<typename ValueType, expr_value (codegen_context::*create_const_int)(ValueType value), typename T>
static void get_nonzero_constant_numeric_array_value(
	bz::array_view<T const> values,
	ast::ts_array const &array_type,
	codegen_context &context,
	expr_value result_address
)
{
	static_assert(
		(std::is_floating_point_v<ValueType> && std::is_same_v<ValueType, T>)
		|| (std::is_integral_v<ValueType> && std::is_integral_v<T> && std::is_signed_v<ValueType> == std::is_signed_v<T>)
	);
	bz_assert(result_address.get_type()->is_array());
	if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(result_address.get_type()->get_array_element_type()->is_array());
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		for (auto const i : bz::iota(0, array_type.size))
		{
			auto const begin_index = i * stride;
			auto const sub_array = values.slice(begin_index, begin_index + stride);
			auto const elem_result_address = context.create_struct_gep(result_address, i);
			get_nonzero_constant_numeric_array_value<ValueType, create_const_int>(
				sub_array,
				array_type.elem_type.get<ast::ts_array>(),
				context,
				elem_result_address
			);
		}
	}
	else
	{
		for (auto const i : bz::iota(0, array_type.size))
		{
			auto const elem_result_address = context.create_struct_gep(result_address, i);
			context.create_store((context.*create_const_int)(static_cast<ValueType>(values[i])), elem_result_address);
		}
	}
}

static void get_constant_sint_array_value(
	bz::array_view<int64_t const> values,
	ast::ts_array const &array_type,
	codegen_context &context,
	expr_value result_address
)
{
	if (values.is_all([](auto const value) { return value == 0; }))
	{
		context.create_const_memset_zero(result_address);
		context.create_start_lifetime(result_address);
	}
	else
	{
		auto const elem_type = flattened_array_elem_type(array_type);
		bz_assert(elem_type.is<ast::ts_base_type>());
		switch (elem_type.get<ast::ts_base_type>().info->kind)
		{
		case ast::type_info::int8_:
			get_nonzero_constant_numeric_array_value<
				int8_t, &codegen_context::create_const_i8
			>(values, array_type, context, result_address);
			break;
		case ast::type_info::int16_:
			get_nonzero_constant_numeric_array_value<
				int16_t, &codegen_context::create_const_i16
			>(values, array_type, context, result_address);
			break;
		case ast::type_info::int32_:
			get_nonzero_constant_numeric_array_value<
				int32_t, &codegen_context::create_const_i32
			>(values, array_type, context, result_address);
			break;
		case ast::type_info::int64_:
			get_nonzero_constant_numeric_array_value<
				int64_t, &codegen_context::create_const_i64
			>(values, array_type, context, result_address);
			break;
		default:
			bz_unreachable;
		}
		context.create_start_lifetime(result_address);
	}
}

static void get_constant_uint_array_value(
	bz::array_view<uint64_t const> values,
	ast::ts_array const &array_type,
	codegen_context &context,
	expr_value result_address
)
{
	if (values.is_all([](auto const value) { return value == 0; }))
	{
		context.create_const_memset_zero(result_address);
		context.create_start_lifetime(result_address);
	}
	else
	{
		auto const elem_type = flattened_array_elem_type(array_type);
		bz_assert(elem_type.is<ast::ts_base_type>());
		switch (elem_type.get<ast::ts_base_type>().info->kind)
		{
		case ast::type_info::uint8_:
			get_nonzero_constant_numeric_array_value<
				uint8_t, &codegen_context::create_const_u8
			>(values, array_type, context, result_address);
			break;
		case ast::type_info::uint16_:
			get_nonzero_constant_numeric_array_value<
				uint16_t, &codegen_context::create_const_u16
			>(values, array_type, context, result_address);
			break;
		case ast::type_info::uint32_:
			get_nonzero_constant_numeric_array_value<
				uint32_t, &codegen_context::create_const_u32
			>(values, array_type, context, result_address);
			break;
		case ast::type_info::uint64_:
			get_nonzero_constant_numeric_array_value<
				uint64_t, &codegen_context::create_const_u64
			>(values, array_type, context, result_address);
			break;
		default:
			bz_unreachable;
		}
		context.create_start_lifetime(result_address);
	}
}

static void get_constant_float32_array_value(
	bz::array_view<float32_t const> values,
	ast::ts_array const &array_type,
	codegen_context &context,
	expr_value result_address
)
{
	if (values.is_all([](auto const value) { return bit_cast<uint32_t>(value) == 0; }))
	{
		context.create_const_memset_zero(result_address);
		context.create_start_lifetime(result_address);
	}
	else
	{
		get_nonzero_constant_numeric_array_value<
			float32_t, &codegen_context::create_const_f32
		>(values, array_type, context, result_address);
		context.create_start_lifetime(result_address);
	}
}

static void get_constant_float64_array_value(
	bz::array_view<float64_t const> values,
	ast::ts_array const &array_type,
	codegen_context &context,
	expr_value result_address
)
{
	if (values.is_all([](auto const value) { return bit_cast<uint64_t>(value) == 0; }))
	{
		context.create_const_memset_zero(result_address);
		context.create_start_lifetime(result_address);
	}
	else
	{
		get_nonzero_constant_numeric_array_value<
			float64_t, &codegen_context::create_const_f64
		>(values, array_type, context, result_address);
		context.create_start_lifetime(result_address);
	}
}

static type const *get_tuple_type(ast::typespec_view type, ast::constant_expression const *const_expr, codegen_context &context)
{
	if (type.not_empty())
	{
		return get_type(type, context);
	}

	bz_assert(const_expr != nullptr && const_expr->expr.is<ast::expr_tuple>());
	auto const types = const_expr->expr.get<ast::expr_tuple>().elems
		.transform([](auto const &elem) { return elem.get_constant(); })
		.transform([&context](auto const &const_elem) { return get_tuple_type(const_elem.type, &const_elem, context); })
		.collect();
	return context.get_aggregate_type(types);
}

static expr_value get_constant_value_helper(
	lex::src_tokens const &src_tokens,
	ast::constant_value const &value,
	ast::typespec_view type,
	ast::constant_expression const *const_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	switch (value.kind())
	{
	static_assert(ast::constant_value::variant_count == 19);
	case ast::constant_value_kind::sint:
	{
		auto int_value = expr_value::get_none();
		bz_assert(type.is<ast::ts_base_type>());
		switch (type.get<ast::ts_base_type>().info->kind)
		{
		case ast::type_info::int8_:
			int_value = context.create_const_i8(static_cast<int8_t>(value.get_sint()));
			break;
		case ast::type_info::int16_:
			int_value = context.create_const_i16(static_cast<int16_t>(value.get_sint()));
			break;
		case ast::type_info::int32_:
			int_value = context.create_const_i32(static_cast<int32_t>(value.get_sint()));
			break;
		case ast::type_info::int64_:
			int_value = context.create_const_i64(static_cast<int64_t>(value.get_sint()));
			break;
		default:
			bz_unreachable;
		}
		return value_or_result_address(int_value, result_address, context);
	}
	case ast::constant_value_kind::uint:
	{
		auto int_value = expr_value::get_none();
		bz_assert(type.is<ast::ts_base_type>());
		switch (type.get<ast::ts_base_type>().info->kind)
		{
		case ast::type_info::uint8_:
			int_value = context.create_const_u8(static_cast<uint8_t>(value.get_uint()));
			break;
		case ast::type_info::uint16_:
			int_value = context.create_const_u16(static_cast<uint16_t>(value.get_uint()));
			break;
		case ast::type_info::uint32_:
			int_value = context.create_const_u32(static_cast<uint32_t>(value.get_uint()));
			break;
		case ast::type_info::uint64_:
			int_value = context.create_const_u64(static_cast<uint64_t>(value.get_uint()));
			break;
		default:
			bz_unreachable;
		}
		return value_or_result_address(int_value, result_address, context);
	}
	case ast::constant_value_kind::float32:
		return value_or_result_address(context.create_const_f32(value.get_float32()), result_address, context);
	case ast::constant_value_kind::float64:
		return value_or_result_address(context.create_const_f64(value.get_float64()), result_address, context);
	case ast::constant_value_kind::u8char:
		return value_or_result_address(context.create_const_u32(value.get_u8char()), result_address, context);
	case ast::constant_value_kind::string:
	{
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(src_tokens, context.get_str_t());
		}

		auto const &result_value = result_address.get();

		auto const str = value.get_string();

		// if the string is empty, we make a zero initialized string, so
		// structs with a default value of "" get to be zero initialized
		if (str == "")
		{
			context.create_const_memset_zero(result_value);
			context.create_start_lifetime(result_value);
		}
		else
		{
			context.create_string(src_tokens, str, result_value);
		}
		return result_value;
	}
	case ast::constant_value_kind::boolean:
		return value_or_result_address(context.create_const_i1(value.get_boolean()), result_address, context);
	case ast::constant_value_kind::null:
		if (
			auto const bare_type = type.remove_any_mut();
			bare_type.is_optional_pointer_like() && !result_address.has_value()
		)
		{
			return value_or_result_address(context.create_const_ptr_null(), result_address, context);
		}
		else
		{
			if (!result_address.has_value())
			{
				result_address = context.create_alloca(src_tokens, get_type(bare_type, context));
			}

			auto const &result_value = result_address.get();
			context.create_const_memset_zero(result_value);
			context.create_start_lifetime(result_value);
			return result_value;
		}
	case ast::constant_value_kind::void_:
		bz_unreachable;
	case ast::constant_value_kind::enum_:
	{
		auto const [decl, enum_value] = value.get_enum();
		auto const signed_enum_value = bit_cast<int64_t>(enum_value);

		auto enum_int_value = expr_value::get_none();
		switch (decl->underlying_type.get<ast::ts_base_type>().info->kind)
		{
		case ast::type_info::int8_:
			enum_int_value = context.create_const_i8(static_cast<int8_t>(signed_enum_value));
			break;
		case ast::type_info::int16_:
			enum_int_value = context.create_const_i16(static_cast<int16_t>(signed_enum_value));
			break;
		case ast::type_info::int32_:
			enum_int_value = context.create_const_i32(static_cast<int32_t>(signed_enum_value));
			break;
		case ast::type_info::int64_:
			enum_int_value = context.create_const_i64(static_cast<int64_t>(signed_enum_value));
			break;
		case ast::type_info::uint8_:
			enum_int_value = context.create_const_u8(static_cast<uint8_t>(enum_value));
			break;
		case ast::type_info::uint16_:
			enum_int_value = context.create_const_u16(static_cast<uint16_t>(enum_value));
			break;
		case ast::type_info::uint32_:
			enum_int_value = context.create_const_u32(static_cast<uint32_t>(enum_value));
			break;
		case ast::type_info::uint64_:
			enum_int_value = context.create_const_u64(static_cast<uint64_t>(enum_value));
			break;
		default:
			bz_unreachable;
		}

		return value_or_result_address(enum_int_value, result_address, context);
	}
	case ast::constant_value_kind::array:
	{
		auto const array_type = type.remove_any_mut();
		bz_assert(array_type.is<ast::ts_array>());
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(src_tokens, get_type(array_type, context));
		}
		get_constant_array_value(
			src_tokens,
			value.get_array(),
			array_type.get<ast::ts_array>(),
			context,
			result_address.get()
		);
		return result_address.get();
	}
	case ast::constant_value_kind::sint_array:
	{
		auto const array_type = type.remove_any_mut();
		bz_assert(array_type.is<ast::ts_array>());
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(src_tokens, get_type(array_type, context));
		}
		get_constant_sint_array_value(
			value.get_sint_array(),
			array_type.get<ast::ts_array>(),
			context,
			result_address.get()
		);
		return result_address.get();
	}
	case ast::constant_value_kind::uint_array:
	{
		auto const array_type = type.remove_any_mut();
		bz_assert(array_type.is<ast::ts_array>());
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(src_tokens, get_type(array_type, context));
		}
		get_constant_uint_array_value(
			value.get_uint_array(),
			array_type.get<ast::ts_array>(),
			context,
			result_address.get()
		);
		return result_address.get();
	}
	case ast::constant_value_kind::float32_array:
	{
		auto const array_type = type.remove_any_mut();
		bz_assert(array_type.is<ast::ts_array>());
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(src_tokens, get_type(array_type, context));
		}
		get_constant_float32_array_value(
			value.get_float32_array(),
			array_type.get<ast::ts_array>(),
			context,
			result_address.get()
		);
		return result_address.get();
	}
	case ast::constant_value_kind::float64_array:
	{
		auto const array_type = type.remove_any_mut();
		bz_assert(array_type.is<ast::ts_array>());
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(src_tokens, get_type(array_type, context));
		}
		get_constant_float64_array_value(
			value.get_float64_array(),
			array_type.get<ast::ts_array>(),
			context,
			result_address.get()
		);
		return result_address.get();
	}
	case ast::constant_value_kind::tuple:
	{
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(src_tokens, get_tuple_type(type, const_expr, context));
		}

		auto const &result_value = result_address.get();

		if (const_expr != nullptr && const_expr->expr.is<ast::expr_tuple>())
		{
			auto &tuple = const_expr->expr.get<ast::expr_tuple>();
			bz_assert(tuple.elems.size() == result_value.get_type()->get_aggregate_types().size());
			for (auto const i : bz::iota(0, tuple.elems.size()))
			{
				bz_assert(tuple.elems[i].is_constant());
				auto const &const_elem = tuple.elems[i].get_constant();
				auto const elem_result_address = context.create_struct_gep(result_value, i);
				get_constant_value(src_tokens, const_elem.value, const_elem.type, &const_elem, context, elem_result_address);
			}
		}
		else
		{
			auto const tuple_values = value.get_tuple();
			bz_assert(type.remove_any_mut().is<ast::ts_tuple>());
			auto const &tuple_t = type.remove_any_mut().get<ast::ts_tuple>();
			bz_assert(
				tuple_t.types.size() == tuple_values.size()
				&& tuple_t.types.size() == result_value.get_type()->get_aggregate_types().size()
			);
			if (tuple_values.empty())
			{
				context.create_start_lifetime(result_value);
			}
			else
			{
				for (auto const i : bz::iota(0, tuple_values.size()))
				{
					auto const elem_result_address = context.create_struct_gep(result_value, i);
					get_constant_value(src_tokens, tuple_values[i], tuple_t.types[i], nullptr, context, elem_result_address);
				}
			}
		}
		return result_value;
	}
	case ast::constant_value_kind::function:
	{
		auto const func = context.get_function(value.get_function());
		auto const func_ptr = context.create_const_function_pointer(func);
		return value_or_result_address(func_ptr, result_address, context);
	}
	case ast::constant_value_kind::aggregate:
	{
		auto const aggregate = value.get_aggregate();
		bz_assert(type.remove_any_mut().is<ast::ts_base_type>());
		auto const info = type.remove_any_mut().get<ast::ts_base_type>().info;
		if (!result_address.has_value())
		{
			result_address = context.create_alloca(src_tokens, get_type(type, context));
		}

		auto const &result_value = result_address.get();
		bz_assert(aggregate.size() == result_value.get_type()->get_aggregate_types().size());
		for (auto const i : bz::iota(0, aggregate.size()))
		{
			auto const member_result_address = context.create_struct_gep(result_value, i);
			get_constant_value(src_tokens, aggregate[i], info->member_variables[i]->get_type(), nullptr, context, member_result_address);
		}
		return result_value;
	}
	case ast::constant_value_kind::type:
	{
		bz_assert(result_address.has_value());
		auto const &result_value = result_address.get();
		context.create_store(context.create_typename(value.get_type()), result_value);
		return result_value;
	}
	default:
		bz_unreachable;
	}
}

static expr_value get_constant_value(
	lex::src_tokens const &src_tokens,
	ast::constant_value const &value,
	ast::typespec_view type,
	ast::constant_expression const *const_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	type = type.remove_any_mut();
	if (type.is<ast::ts_optional>() && value.is_null_constant())
	{
		if (type.is_optional_pointer_like() && !result_address.has_value())
		{
			return context.create_const_ptr_null();
		}
		else if (type.is_optional_pointer_like())
		{
			return value_or_result_address(context.create_const_ptr_null(), result_address, context);
		}
		else
		{
			if (!result_address.has_value())
			{
				result_address = context.create_alloca(src_tokens, get_type(type, context));
			}

			auto const &result_value = result_address.get();
			context.create_const_memset_zero(result_value);
			context.create_start_lifetime(get_optional_has_value_ref(result_value, context));
			return result_value;
		}
	}
	else if (type.is<ast::ts_optional>())
	{
		if (type.is_optional_pointer_like())
		{
			return get_constant_value_helper(src_tokens, value, type.get<ast::ts_optional>(), const_expr, context, result_address);
		}
		else
		{
			if (!result_address.has_value())
			{
				result_address = context.create_alloca(src_tokens, get_type(type, context));
			}

			auto const &result_value = result_address.get();
			get_constant_value_helper(
				src_tokens,
				value,
				type.get<ast::ts_optional>(),
				const_expr,
				context,
				get_optional_value(result_value, context)
			);
			set_optional_has_value(result_value, true, context);
			context.create_start_lifetime(get_optional_has_value_ref(result_value, context));
			return result_value;
		}
	}
	else
	{
		return get_constant_value_helper(src_tokens, value, type, const_expr, context, result_address);
	}
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::constant_expression const &const_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	bz_assert(const_expr.kind != ast::expression_type_kind::noreturn);
	if (const_expr.kind == ast::expression_type_kind::type_name)
	{
		if (result_address.has_value())
		{
			return get_constant_value(
				original_expression.src_tokens,
				const_expr.value,
				const_expr.type,
				&const_expr,
				context,
				result_address
			);
		}
		else
		{
			return expr_value::get_none();
		}
	}
	else if (const_expr.kind == ast::expression_type_kind::none)
	{
		bz_assert(!result_address.has_value());
		return expr_value::get_none();
	}

	auto const result = const_expr.kind == ast::expression_type_kind::lvalue
		? generate_expr_code(original_expression, const_expr.expr, context, {})
		: get_constant_value(original_expression.src_tokens, const_expr.value, const_expr.type, &const_expr, context, result_address);

	bz_assert(!result_address.has_value() || result == result_address.get());
	return result;
}

static expr_value generate_expr_code(
	ast::expression const &original_expression,
	ast::dynamic_expression const &dyn_expr,
	codegen_context &context,
	bz::optional<expr_value> result_address
)
{
	if (
		!result_address.has_value()
		&& dyn_expr.kind == ast::expression_type_kind::rvalue
		&& !dyn_expr.type.is_any_reference()
		&& (
			(dyn_expr.destruct_op.not_null() && !dyn_expr.destruct_op.is<ast::trivial_destruct_self>())
			|| dyn_expr.expr.is<ast::expr_compound>()
			|| dyn_expr.expr.is<ast::expr_if>()
			|| dyn_expr.expr.is<ast::expr_switch>()
			|| dyn_expr.expr.is<ast::expr_tuple>()
		)
	)
	{
		result_address = context.create_alloca(original_expression.src_tokens, get_type(dyn_expr.type, context));
		if (dyn_expr.destruct_op.is_null())
		{
			context.push_end_lifetime(result_address.get());
		}
	}

	// noreturn expressions (e.g. 'unreachable') can match to any type, but cannot have a result,
	// so we clear result_address in this case
	if (dyn_expr.kind == ast::expression_type_kind::noreturn)
	{
		result_address.clear();
	}

	auto const result = generate_expr_code(original_expression, dyn_expr.expr, context, result_address);

	if ((result.is_reference() && dyn_expr.destruct_op.not_null()) || dyn_expr.destruct_op.move_destructed_decl != nullptr)
	{
		context.push_self_destruct_operation(dyn_expr.destruct_op, result);
	}

	if (
		dyn_expr.type.is<ast::ts_lvalue_reference>()
		&& (
			dyn_expr.expr.is<ast::expr_compound>()
			|| dyn_expr.expr.is<ast::expr_function_call>()
			|| dyn_expr.expr.is<ast::expr_indirect_function_call>()
		)
	)
	{
		bz_assert(result.is_reference());
		context.create_memory_access_check(original_expression.src_tokens, result, dyn_expr.type.remove_reference());
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
		return generate_expr_code(expr, expr.get_constant(), context, result_address);
	case ast::expression::index_of<ast::dynamic_expression>:
		return generate_expr_code(expr, expr.get_dynamic(), context, result_address);
	case ast::expression::index_of<ast::error_expression>:
		context.create_error(expr.src_tokens, "failed to resolve expression");
		return expr_value::get_none();
	default:
		context.create_error(expr.src_tokens, "failed to resolve expression");
		return expr_value::get_none();
	}
}

static void generate_stmt_code(ast::stmt_while const &while_stmt, codegen_context &context)
{
	auto const cond_check_bb = context.add_basic_block();
	auto const break_bb = context.add_basic_block();

	auto const prev_loop_info = context.push_loop(break_bb, cond_check_bb);

	context.create_jump(cond_check_bb);
	context.set_current_basic_block(cond_check_bb);
	auto const cond_prev_info = context.push_expression_scope();
	auto const condition = while_stmt.condition.is_error()
		? context.get_dummy_value(context.get_builtin_type(builtin_type_kind::i1))
		: generate_expr_code(while_stmt.condition, context, {}).get_value(context);
	context.pop_expression_scope(cond_prev_info);
	auto const cond_check_bb_end = context.get_current_basic_block();

	auto const while_bb = context.add_basic_block();
	context.set_current_basic_block(while_bb);

	auto const while_prev_info = context.push_expression_scope();
	generate_expr_code(while_stmt.while_block, context, {});
	context.pop_expression_scope(while_prev_info);

	context.create_jump(cond_check_bb);

	auto const end_bb = context.add_basic_block();

	context.set_current_basic_block(break_bb);
	context.create_jump(end_bb);

	context.set_current_basic_block(cond_check_bb_end);
	context.create_conditional_jump(condition, while_bb, end_bb);

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

	auto const break_bb = context.add_basic_block();
	auto const iteration_bb = context.add_basic_block();
	auto const prev_loop_info = context.push_loop(break_bb, iteration_bb);

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

	auto condition = expr_value::get_none();
	if (for_stmt.condition.not_null() && for_stmt.condition.not_error())
	{
		auto const prev_info = context.push_expression_scope();
		condition = generate_expr_code(for_stmt.condition, context, {}).get_value(context);
		context.pop_expression_scope(prev_info);
	}
	auto const cond_check_bb_end = context.get_current_basic_block();

	auto const for_bb = context.add_basic_block();
	context.set_current_basic_block(for_bb);

	auto const for_prev_info = context.push_expression_scope();
	generate_expr_code(for_stmt.for_block, context, {});
	context.pop_expression_scope(for_prev_info);

	context.create_jump(iteration_bb);

	auto const end_bb = context.add_basic_block();

	context.set_current_basic_block(break_bb);
	context.create_jump(end_bb);

	context.set_current_basic_block(cond_check_bb_end);
	if (!condition.is_none())
	{
		context.create_conditional_jump(condition, for_bb, end_bb);
	}
	else
	{
		context.create_jump(for_bb);
	}

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
	auto const iteration_prev_info = context.push_expression_scope();
	generate_expr_code(foreach_stmt.iteration, context, {});
	context.pop_expression_scope(iteration_prev_info);

	auto const condition_check_bb = context.add_basic_block();
	context.create_jump(condition_check_bb);
	context.set_current_basic_block(begin_bb);
	context.create_jump(condition_check_bb);

	context.set_current_basic_block(condition_check_bb);
	auto const condition_prev_info = context.push_expression_scope();
	auto const condition = foreach_stmt.condition.is_error()
		? context.get_dummy_value(context.get_builtin_type(builtin_type_kind::i1))
		: generate_expr_code(foreach_stmt.condition, context, {}).get_value(context);
	context.pop_expression_scope(condition_prev_info);

	auto const foreach_bb = context.add_basic_block();
	context.create_conditional_jump(condition, foreach_bb, end_bb);

	context.set_current_basic_block(foreach_bb);
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
	if (context.current_function_info.func->func_body == nullptr)
	{
		auto const src_tokens = return_stmt.expr.is_null()
			? lex::src_tokens::from_single_token(return_stmt.return_pos)
			: lex::src_tokens::from_range({ return_stmt.return_pos, return_stmt.expr.src_tokens.end });
		context.create_error(src_tokens, "return statement not allowed in top level compile time execution");
		context.create_unreachable();
	}
	else if (return_stmt.expr.is_null())
	{
		context.emit_all_destruct_operations();
		context.create_ret_void();
	}
	else if (return_stmt.expr.is_error())
	{
		generate_expr_code(return_stmt.expr, context, {});
		context.create_unreachable();
	}
	else if (context.current_function_info.return_address.has_value())
	{
		bz_assert(return_stmt.expr.not_null());
		generate_expr_code(return_stmt.expr, context, context.current_function_info.return_address);
		context.emit_all_destruct_operations();
		context.create_ret_void();
	}
	else if (context.current_function_info.func->func_body->return_type.is<ast::ts_lvalue_reference>())
	{
		auto const result_value = generate_expr_code(return_stmt.expr, context, {});
		bz_assert(result_value.is_reference());
		context.emit_all_destruct_operations();
		context.create_ret(result_value.get_reference());
	}
	else
	{
		auto const result_value = generate_expr_code(return_stmt.expr, context, {}).get_value_as_instruction(context);
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
	if (expr_stmt.expr.is<ast::expanded_variadic_expression>())
	{
		for (auto const &expr : expr_stmt.expr.get<ast::expanded_variadic_expression>().exprs)
		{
			auto const prev_info = context.push_expression_scope();
			generate_expr_code(expr, context, {});
			context.pop_expression_scope(prev_info);
		}
	}
	else
	{
		auto const prev_info = context.push_expression_scope();
		generate_expr_code(expr_stmt.expr, context, {});
		context.pop_expression_scope(prev_info);
	}
}

static void add_variable_helper(
	ast::decl_variable const &var_decl,
	expr_value value,
	bool is_global_storage,
	codegen_context &context
)
{
	if (var_decl.tuple_decls.empty())
	{
		context.add_variable(&var_decl, value);
		if (!is_global_storage)
		{
			if (var_decl.is_ever_moved_from())
			{
				auto const indicator = context.add_move_destruct_indicator(&var_decl);
				context.push_variable_destruct_operation(var_decl.destruction, value, indicator);
			}
			else if (!var_decl.get_type().is_any_reference() && !var_decl.is_tuple_outer_ref())
			{
				context.push_variable_destruct_operation(var_decl.destruction, value);
			}
		}
	}
	else
	{
		bz_assert(value.get_type()->is_aggregate() || value.get_type()->is_array());
		for (auto const &[decl, i] : var_decl.tuple_decls.enumerate())
		{
			if (decl.get_type().is_any_reference())
			{
				auto const elem_ptr = context.create_struct_gep(value, i).get_value_as_instruction(context);
				auto const elem_type = get_type(decl.get_type().get_any_reference(), context);
				auto const elem_value = expr_value::get_reference(elem_ptr, elem_type);
				add_variable_helper(decl, elem_value, is_global_storage, context);
			}
			else
			{
				auto const elem_value = context.create_struct_gep(value, i);
				add_variable_helper(decl, elem_value, is_global_storage, context);
			}
		}
	}
}

static void generate_stmt_code(ast::decl_variable const &var_decl, codegen_context &context)
{
	if (var_decl.get_type().is_empty() || var_decl.init_expr.is_error() || var_decl.state == ast::resolve_state::error)
	{
		context.create_error(var_decl.src_tokens, "failed to resolve variable declaration");
		context.create_unreachable();
		return;
	}

	if (var_decl.is_global_storage())
	{
		if (var_decl.global_tuple_decl_parent != nullptr)
		{
			generate_stmt_code(*var_decl.global_tuple_decl_parent, context);
			return;
		}

		bz_assert(var_decl.init_expr.is_constant());
		bz_assert(var_decl.get_type().is<ast::ts_consteval>());

		auto const current_bb = context.get_current_basic_block();
		context.set_current_basic_block(context.current_function_info.constants_bb);

		if (auto const global_index = context.get_global_variable(&var_decl); global_index.has_value())
		{
			auto const value = context.create_get_global_object(global_index.get());
			add_variable_helper(var_decl, value, true, context);
		}
		else
		{
			auto const &init_value = var_decl.init_expr.get_constant_value();
			auto const type = get_type(var_decl.get_type(), context);
			auto data = memory::object_from_constant_value(
				var_decl.src_tokens,
				init_value,
				type,
				context
			);
			auto const [value, index] = context.create_global_object(var_decl.src_tokens, type, std::move(data));
			context.add_global_variable(&var_decl, index);
			add_variable_helper(var_decl, value, true, context);
		}

		context.set_current_basic_block(current_bb);
	}
	else if (var_decl.get_type().is_typename())
	{
		return;
	}
	else if (var_decl.get_type().is_any_reference())
	{
		bz_assert(var_decl.init_expr.not_null());
		auto const prev_info = context.push_expression_scope();
		auto const init_val = generate_expr_code(var_decl.init_expr, context, {});
		context.pop_expression_scope(prev_info);
		auto const ref_value = init_val.is_none()
			? expr_value::get_reference(instruction_ref{}, get_type(var_decl.get_type().get<ast::ts_lvalue_reference>(), context))
			: init_val;
		add_variable_helper(var_decl, ref_value, false, context);
	}
	else
	{
		auto const type = get_type(var_decl.get_type(), context);
		auto const alloca = context.create_alloca(var_decl.src_tokens, type);
		if (var_decl.init_expr.not_null())
		{
			auto const prev_info = context.push_expression_scope();
			generate_expr_code(var_decl.init_expr, context, alloca);
			context.pop_expression_scope(prev_info);
		}
		add_variable_helper(var_decl, alloca, false, context);
	}
}

static void generate_stmt_code(ast::statement const &stmt, codegen_context &context)
{
	switch (stmt.kind())
	{
	static_assert(ast::statement::variant_count == 17);
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
	case ast::statement::index<ast::decl_function_alias>:
	case ast::statement::index<ast::decl_operator_alias>:
	case ast::statement::index<ast::decl_struct>:
	case ast::statement::index<ast::decl_enum>:
	case ast::statement::index<ast::decl_import>:
	case ast::statement::index<ast::decl_type_alias>:
		break;
	default:
		if (context.current_function_info.func->func_body != nullptr)
		{
			context.create_error(context.current_function_info.func->func_body->src_tokens, "failed to resolve a statement in the function");
		}
		context.create_unreachable();
		break;
	}
}

void generate_code(function &func, codegen_context &context)
{
	bz_assert(func.func_body != nullptr);
	auto &body = *func.func_body;
	bz_assert(!body.is_comptime_bitcode_emitted());

	context.initialize_function(&func);

	if (body.state == ast::resolve_state::error)
	{
		context.create_error(body.src_tokens, bz::format("'{}' could not be resolved", body.get_signature()));
		context.create_unreachable();
		context.finalize_function();
		body.flags |= ast::function_body::comptime_bitcode_emitted;
		return;
	}

	bz_assert(body.state == ast::resolve_state::all);

	auto const prev_info = context.push_expression_scope();

	size_t i = 0;
	for (auto const &param : body.params)
	{
		if (ast::is_generic_parameter(param))
		{
			generate_stmt_code(param, context);
			continue;
		}

		if (param.get_type().is_any_reference())
		{
			auto const inner_type = param.get_type().get_any_reference();
			auto const type = get_type(inner_type, context);
			auto const value = expr_value::get_reference(context.create_get_function_arg(i), type);
			add_variable_helper(param, value, false, context);
		}
		else if (auto const type = func.arg_types[i]; type->is_simple_value_type())
		{
			auto const alloca = context.create_alloca(param.src_tokens, type);
			auto const value = expr_value::get_value(context.create_get_function_arg(i), type);
			context.create_store(value, alloca);
			context.create_start_lifetime(alloca);
			add_variable_helper(param, alloca, false, context);
		}
		else
		{
			auto const value = expr_value::get_reference(context.create_get_function_arg(i), type);
			add_variable_helper(param, value, false, context);
		}
		++i;
	}
	bz_assert(i == func.arg_types.size());

	for (auto const &stmt : body.get_statements())
	{
		if (context.has_terminator())
		{
			break;
		}
		generate_stmt_code(stmt, context);
	}

	context.pop_expression_scope(prev_info);

	if (!context.has_terminator())
	{
		auto const return_type = func.return_type;
		if (return_type->is_void())
		{
			context.create_ret_void();
		}
		else if (body.is_main())
		{
			bz_assert(return_type->is_builtin() && return_type->get_builtin_kind() == builtin_type_kind::i32);
			context.create_ret(context.create_const_i32(0).get_value_as_instruction(context));
		}
		else
		{
			context.create_error(body.src_tokens, "end of function reached without returning a value");
			context.create_unreachable();
		}
	}

	context.finalize_function();
	body.flags |= ast::function_body::comptime_bitcode_emitted;
}

std::unique_ptr<function> generate_from_symbol(ast::function_body &body, codegen_context &context)
{
	auto result = std::make_unique<function>();
	result->func_body = &body;

	result->return_type = get_type(body.return_type, context);
	result->arg_types = bz::fixed_vector(
		body.params
			.filter([](auto const &param) { return !ast::is_generic_parameter(param); })
			.transform([&context](auto const &param) { return get_type(param.get_type(), context); })
			.collect(body.params.size())
			.as_array_view()
	);

	return result;
}

function generate_code_for_expression(ast::expression const &expr, codegen_context &context)
{
	auto func = function();

	auto const expr_type = expr.get_expr_type();
	bz_assert(!expr_type.is_empty());

	if (expr_type.is<ast::ts_void>())
	{
		func.return_type = context.get_builtin_type(builtin_type_kind::void_);
		context.initialize_function(&func);

		auto const prev_info = context.push_expression_scope();
		generate_expr_code(expr, context, {});
		context.pop_expression_scope(prev_info);

		if (!context.has_terminator())
		{
			context.create_ret_void();
		}
	}
	else if (expr_type.is_typename())
	{
		func.return_type = context.get_builtin_type(builtin_type_kind::i32);
		context.initialize_function(&func);

		auto const result_address = context.create_alloca(expr.src_tokens, func.return_type);
		auto const prev_info = context.push_expression_scope();
		generate_expr_code(expr, context, result_address);
		context.pop_expression_scope(prev_info);

		if (!context.has_terminator())
		{
			context.create_ret(result_address.get_value_as_instruction(context));
		}
	}
	else
	{
		func.return_type = context.get_pointer_type();
		context.initialize_function(&func);

		auto const result_address = context.create_alloca(expr.src_tokens, get_type(expr_type, context));
		auto const prev_info = context.push_expression_scope();
		generate_expr_code(expr, context, result_address);
		context.pop_expression_scope(prev_info);

		if (!context.has_terminator())
		{
			context.create_ret(result_address.get_reference());
		}
	}

	context.finalize_function();
	return func;
}

static void generate_rvalue_array_destruct(
	ast::expression const &elem_destruct_expr,
	expr_value array_value,
	expr_value rvalue_array_elem_ptr_value,
	codegen_context &context
)
{
	bz_assert(array_value.get_type()->is_array());
	auto const size = array_value.get_type()->get_array_size();
	auto const elem_type = array_value.get_type()->get_array_element_type();

	auto const begin_elem_ptr = context.create_struct_gep(array_value, 0);
	auto const end_elem_ptr = context.create_struct_gep(array_value, size);

	auto const begin_elem_ptr_value = expr_value::get_value(begin_elem_ptr.get_reference(), context.get_pointer_type());

	auto const it_elem_ptr_ref = context.create_alloca_without_lifetime(context.get_pointer_type());
	context.create_store(expr_value::get_value(end_elem_ptr.get_reference(), context.get_pointer_type()), it_elem_ptr_ref);

	auto const loop_begin_bb = context.add_basic_block();
	context.create_jump(loop_begin_bb);
	context.set_current_basic_block(loop_begin_bb);

	auto const prev_elem_ptr = it_elem_ptr_ref.get_value(context);
	auto const elem_ptr_value = context.create_ptr_add_const_unchecked(prev_elem_ptr, -1, elem_type);
	context.create_store(elem_ptr_value, it_elem_ptr_ref);

	auto const skip_elem = context.create_pointer_cmp_eq(elem_ptr_value, rvalue_array_elem_ptr_value);

	auto const destruct_bb = context.add_basic_block();
	context.set_current_basic_block(destruct_bb);

	auto const elem_ptr = expr_value::get_reference(elem_ptr_value.get_value_as_instruction(context), elem_type);
	auto const prev_value = context.push_value_reference(elem_ptr);
	generate_expr_code(elem_destruct_expr, context, {});
	context.pop_value_reference(prev_value);

	auto const loop_end_bb = context.add_basic_block();
	context.create_jump(loop_end_bb);

	context.set_current_basic_block(loop_begin_bb);
	context.create_conditional_jump(skip_elem, loop_end_bb, destruct_bb);
	context.set_current_basic_block(loop_end_bb);

	auto const end_loop = context.create_pointer_cmp_eq(elem_ptr_value, begin_elem_ptr_value);

	auto const end_bb = context.add_basic_block();
	context.create_conditional_jump(end_loop, end_bb, loop_begin_bb);
	context.set_current_basic_block(end_bb);
}

void generate_destruct_operation(destruct_operation_info_t const &destruct_op_info, codegen_context &context)
{
	auto const &condition = destruct_op_info.condition;
	// pop_expression_scope() can invalidate the reference to destruct_op_info
	auto const move_destruct_indicator = destruct_op_info.move_destruct_indicator;

	if (destruct_op_info.destruct_op == nullptr)
	{
		auto const &value = destruct_op_info.value;
		if (!value.is_none())
		{
			if (condition.has_value())
			{
				auto const condition_value = expr_value::get_reference(
					condition.get(),
					context.get_builtin_type(builtin_type_kind::i1)
				).get_value(context);

				auto const begin_bb = context.get_current_basic_block();

				auto const destruct_bb = context.add_basic_block();
				context.set_current_basic_block(destruct_bb);
				context.create_end_lifetime(value);

				auto const end_bb = context.add_basic_block();
				context.create_jump(end_bb);

				context.set_current_basic_block(begin_bb);
				context.create_conditional_jump(condition_value, destruct_bb, end_bb);

				context.set_current_basic_block(end_bb);
			}
			else
			{
				context.create_end_lifetime(value);
			}
		}
	}
	else if (auto const &destruct_op = *destruct_op_info.destruct_op; destruct_op.is<ast::destruct_variable>())
	{
		bz_assert(destruct_op.get<ast::destruct_variable>().destruct_call->not_null());
		if (condition.has_value())
		{
			auto const condition_value = expr_value::get_reference(
				condition.get(),
				context.get_builtin_type(builtin_type_kind::i1)
			).get_value(context);

			auto const begin_bb = context.get_current_basic_block();

			auto const destruct_bb = context.add_basic_block();
			context.set_current_basic_block(destruct_bb);
			generate_expr_code(*destruct_op.get<ast::destruct_variable>().destruct_call, context, {});

			auto const end_bb = context.add_basic_block();
			context.create_jump(end_bb);

			context.set_current_basic_block(begin_bb);
			context.create_conditional_jump(condition_value, destruct_bb, end_bb);

			context.set_current_basic_block(end_bb);
		}
		else
		{
			generate_expr_code(*destruct_op.get<ast::destruct_variable>().destruct_call, context, {});
		}
	}
	else if (destruct_op.is<ast::destruct_self>())
	{
		auto const &value = destruct_op_info.value;
		bz_assert(destruct_op.get<ast::destruct_self>().destruct_call->not_null());
		bz_assert(!value.is_none());
		if (condition.has_value())
		{
			auto const condition_value = expr_value::get_reference(
				condition.get(),
				context.get_builtin_type(builtin_type_kind::i1)
			).get_value(context);

			auto const begin_bb = context.get_current_basic_block();

			auto const destruct_bb = context.add_basic_block();
			context.set_current_basic_block(destruct_bb);

			auto const prev_value = context.push_value_reference(value);
			generate_expr_code(*destruct_op.get<ast::destruct_self>().destruct_call, context, {});
			context.pop_value_reference(prev_value);

			auto const end_bb = context.add_basic_block();
			context.create_jump(end_bb);

			context.set_current_basic_block(begin_bb);
			context.create_conditional_jump(condition_value, destruct_bb, end_bb);

			context.set_current_basic_block(end_bb);
		}
		else
		{
			auto const prev_value = context.push_value_reference(value);
			generate_expr_code(*destruct_op.get<ast::destruct_self>().destruct_call, context, {});
			context.pop_value_reference(prev_value);
		}
	}
	else if (destruct_op.is<ast::trivial_destruct_self>())
	{
		auto const &value = destruct_op_info.value;
		bz_assert(!value.is_none());
		if (condition.has_value())
		{
			auto const condition_value = expr_value::get_reference(
				condition.get(),
				context.get_builtin_type(builtin_type_kind::i1)
			).get_value(context);

			auto const begin_bb = context.get_current_basic_block();

			auto const destruct_bb = context.add_basic_block();
			context.set_current_basic_block(destruct_bb);
			context.create_end_lifetime(value);

			auto const end_bb = context.add_basic_block();
			context.create_jump(end_bb);

			context.set_current_basic_block(begin_bb);
			context.create_conditional_jump(condition_value, destruct_bb, end_bb);

			context.set_current_basic_block(end_bb);
		}
		else
		{
			context.create_end_lifetime(value);
		}
	}
	else if (destruct_op.is<ast::defer_expression>())
	{
		bz_assert(!condition.has_value());
		auto const prev_info = context.push_expression_scope();
		generate_expr_code(*destruct_op.get<ast::defer_expression>().expr, context, {});
		context.pop_expression_scope(prev_info);
	}
	else if (destruct_op.is<ast::destruct_rvalue_array>())
	{
		auto const &value = destruct_op_info.value;
		bz_assert(destruct_op_info.rvalue_array_elem_ptr.has_value());
		if (condition.has_value())
		{
			auto const condition_value = expr_value::get_reference(
				condition.get(),
				context.get_builtin_type(builtin_type_kind::i1)
			).get_value(context);

			auto const begin_bb = context.get_current_basic_block();

			auto const destruct_bb = context.add_basic_block();
			context.set_current_basic_block(destruct_bb);

			auto const rvalue_array_elem_ptr = expr_value::get_value(
				destruct_op_info.rvalue_array_elem_ptr.get(),
				context.get_pointer_type()
			);
			generate_rvalue_array_destruct(
				*destruct_op.get<ast::destruct_rvalue_array>().elem_destruct_call,
				value,
				rvalue_array_elem_ptr,
				context
			);

			auto const end_bb = context.add_basic_block();
			context.create_jump(end_bb);

			context.set_current_basic_block(begin_bb);
			context.create_conditional_jump(condition_value, destruct_bb, end_bb);

			context.set_current_basic_block(end_bb);
		}
		else
		{
			auto const rvalue_array_elem_ptr = expr_value::get_value(
				destruct_op_info.rvalue_array_elem_ptr.get(),
				context.get_pointer_type()
			);
			generate_rvalue_array_destruct(
				*destruct_op.get<ast::destruct_rvalue_array>().elem_destruct_call,
				value,
				rvalue_array_elem_ptr,
				context
			);
		}
	}
	else
	{
		static_assert(ast::destruct_operation::variant_count == 5);
		// nothing
	}

	if (move_destruct_indicator.has_value())
	{
		auto const move_destruct_indicator_ref = expr_value::get_reference(
			move_destruct_indicator.get(),
			context.get_builtin_type(builtin_type_kind::i1)
		);
		context.create_store(context.create_const_i1(false), move_destruct_indicator_ref);
	}
}

void generate_consteval_variable(ast::decl_variable const &var_decl, codegen_context &context)
{
	generate_stmt_code(var_decl, context);
}

} // namespace comptime
