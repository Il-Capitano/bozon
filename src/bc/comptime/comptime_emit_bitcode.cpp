#include <llvm/IR/Verifier.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/Attributes.h>

#include "ast/typespec.h"
#include "bz/meta.h"
#include "comptime_emit_bitcode.h"
#include "ctx/builtin_operators.h"
#include "colors.h"
#include "abi/calling_conventions.h"
#include "abi/platform_function_call.h"

namespace bc::comptime
{

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expression const &expr,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
);

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::statement const &stmt,
	ctx::comptime_executor_context &context
);

template<abi::platform_abi abi>
static llvm::Constant *get_value(
	ast::constant_value const &value,
	ast::typespec_view type,
	ast::constant_expression const *const_expr,
	ctx::comptime_executor_context &context
);


static llvm::Value *get_constant_zero(
	ast::typespec_view type,
	llvm::Type *llvm_type,
	ctx::comptime_executor_context &context
)
{
	switch (type.kind())
	{
	case ast::typespec_node_t::index_of<ast::ts_base_type>:
	{
		auto const type_kind = type.get<ast::ts_base_type>().info->kind;
		switch (type_kind)
		{
		case ast::type_info::int8_:
		case ast::type_info::int16_:
		case ast::type_info::int32_:
		case ast::type_info::int64_:
		case ast::type_info::uint8_:
		case ast::type_info::uint16_:
		case ast::type_info::uint32_:
		case ast::type_info::uint64_:
		case ast::type_info::char_:
		case ast::type_info::bool_:
			return llvm::ConstantInt::get(llvm_type, 0);
		case ast::type_info::float32_:
		case ast::type_info::float64_:
			return llvm::ConstantFP::get(llvm_type, 0.0);
		case ast::type_info::str_:
		case ast::type_info::null_t_:
		case ast::type_info::aggregate:
		{
			auto const struct_type = llvm::dyn_cast<llvm::StructType>(llvm_type);
			bz_assert(struct_type != nullptr);
			return llvm::ConstantStruct::getNullValue(struct_type);
		}
		default:
			bz_unreachable;
		}
	}
	case ast::typespec_node_t::index_of<ast::ts_const>:
		return get_constant_zero(type.get<ast::ts_const>(), llvm_type, context);
	case ast::typespec_node_t::index_of<ast::ts_consteval>:
		return get_constant_zero(type.get<ast::ts_consteval>(), llvm_type, context);
	case ast::typespec_node_t::index_of<ast::ts_pointer>:
	{
		auto const ptr_type = llvm::dyn_cast<llvm::PointerType>(llvm_type);
		bz_assert(ptr_type != nullptr);
		return llvm::ConstantPointerNull::get(ptr_type);
	}
	case ast::typespec_node_t::index_of<ast::ts_function>:
	{
		auto const ptr_type = llvm::dyn_cast<llvm::PointerType>(llvm_type);
		bz_assert(ptr_type != nullptr);
		return llvm::ConstantPointerNull::get(ptr_type);
	}
	case ast::typespec_node_t::index_of<ast::ts_array>:
		return llvm::ConstantArray::getNullValue(llvm_type);
	case ast::typespec_node_t::index_of<ast::ts_array_slice>:
		return llvm::ConstantStruct::getNullValue(llvm_type);
	case ast::typespec_node_t::index_of<ast::ts_tuple>:
		return llvm::ConstantAggregate::getNullValue(llvm_type);

	case ast::typespec_node_t::index_of<ast::ts_unresolved>:
	case ast::typespec_node_t::index_of<ast::ts_void>:
	case ast::typespec_node_t::index_of<ast::ts_lvalue_reference>:
	case ast::typespec_node_t::index_of<ast::ts_auto>:
	default:
		bz_unreachable;
	}
}

static void push_destructor_call(
	lex::src_tokens src_tokens,
	llvm::Value *ptr,
	ast::typespec_view type,
	ctx::comptime_executor_context &context
)
{
	type = ast::remove_const_or_consteval(type);
	if (type.is<ast::ts_base_type>())
	{
		auto const &info = *type.get<ast::ts_base_type>().info;
		for (auto const &[member, i] : info.member_variables.enumerate())
		{
			auto const member_ptr = context.builder.CreateStructGEP(ptr, i);
			push_destructor_call(src_tokens, member_ptr, member.type, context);
		}
		if (info.destructor != nullptr)
		{
			context.push_destructor_call(src_tokens, info.destructor.get(), ptr);
		}
	}
	else if (type.is<ast::ts_tuple>())
	{
		for (auto const &[member_type, i] : type.get<ast::ts_tuple>().types.enumerate())
		{
			auto const member_ptr = context.builder.CreateStructGEP(ptr, i);
			push_destructor_call(src_tokens, member_ptr, member_type, context);
		}
	}
	else if (type.is<ast::ts_array>())
	{
		auto const array_size = type.get<ast::ts_array>().size;
		auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
		for (auto const i : bz::iota(0, array_size))
		{
			auto const elem_ptr = context.builder.CreateStructGEP(ptr, i);
			push_destructor_call(src_tokens, elem_ptr, elem_type, context);
		}
	}
	else
	{
		// nothing
	}
}

static void emit_destructor_call(
	lex::src_tokens src_tokens,
	llvm::Value *ptr,
	ast::typespec_view type,
	ctx::comptime_executor_context &context
)
{
	type = ast::remove_const_or_consteval(type);
	if (type.is<ast::ts_base_type>())
	{
		auto const &info = *type.get<ast::ts_base_type>().info;
		if (info.destructor != nullptr)
		{
			auto const dtor_func = context.get_function(info.destructor.get());
			emit_push_call(src_tokens, info.destructor.get(), context);
			context.builder.CreateCall(dtor_func, ptr);
			emit_pop_call(context);
		}
		auto const members_count = info.member_variables.size();
		for (auto const &[member, i] : info.member_variables.reversed().enumerate())
		{
			auto const member_ptr = context.builder.CreateStructGEP(ptr, members_count - i - 1);
			emit_destructor_call(src_tokens, member_ptr, member.type, context);
		}
	}
	else if (type.is<ast::ts_tuple>())
	{
		auto const members_count = type.get<ast::ts_tuple>().types.size();
		for (auto const &[member_type, i] : type.get<ast::ts_tuple>().types.reversed().enumerate())
		{
			auto const member_ptr = context.builder.CreateStructGEP(ptr, members_count - i - 1);
			emit_destructor_call(src_tokens, member_ptr, member_type, context);
		}
	}
	else if (type.is<ast::ts_array>())
	{
		auto const array_size = type.get<ast::ts_array>().size;
		auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
		for (auto const i : bz::iota(0, array_size))
		{
			auto const elem_ptr = context.builder.CreateStructGEP(ptr, array_size - i - 1);
			emit_destructor_call(src_tokens, elem_ptr, elem_type, context);
		}
	}
	else
	{
		// nothing
	}
}

static void emit_error_check(ctx::comptime_executor_context &context)
{
	if (context.current_function.first != nullptr && context.current_function.first->is_no_comptime_checking())
	{
		return;
	}
	bz_assert(context.error_bb != nullptr);
	auto const has_error_val = context.builder.CreateCall(context.get_comptime_function(ctx::comptime_function_kind::has_errors));
	auto const continue_bb = context.add_basic_block("error_check_continue");
	context.builder.CreateCondBr(has_error_val, context.error_bb, continue_bb);
	context.builder.SetInsertPoint(continue_bb);
}

static void emit_error_assert(
	llvm::Value *bool_val,
	lex::src_tokens src_tokens,
	bz::u8string message,
	ctx::comptime_executor_context &context
)
{
	if (context.current_function.first != nullptr && context.current_function.first->is_no_comptime_checking())
	{
		return;
	}
	bz_assert(context.error_bb != nullptr);
	auto const fail_bb = context.add_basic_block("error_assert_fail");
	auto const continue_bb = context.add_basic_block("error_assert_continue");
	context.builder.CreateCondBr(bool_val, continue_bb, fail_bb);
	context.builder.SetInsertPoint(fail_bb);
	auto const error_kind_val = llvm::ConstantInt::get(context.get_uint32_t(), static_cast<uint32_t>(ctx::warning_kind::_last));
	auto const error_ptr = context.insert_error(src_tokens, std::move(message));
	static_assert(sizeof(void *) == 8);
	auto const error_ptr_int_val = llvm::ConstantInt::get(
		context.get_uint64_t(),
		reinterpret_cast<uint64_t>(error_ptr)
	);
	context.builder.CreateCall(context.get_comptime_function(ctx::comptime_function_kind::add_error), { error_kind_val, error_ptr_int_val });
	context.builder.CreateBr(context.error_bb);
	context.builder.SetInsertPoint(continue_bb);
}

static void emit_error(
	lex::src_tokens src_tokens,
	bz::u8string message,
	ctx::comptime_executor_context &context
)
{
	if (context.current_function.first != nullptr && context.current_function.first->is_no_comptime_checking())
	{
		return;
	}
	auto const error_kind_val = llvm::ConstantInt::get(context.get_uint32_t(), static_cast<uint32_t>(ctx::warning_kind::_last));
	auto const error_ptr = context.insert_error(src_tokens, std::move(message));
	static_assert(sizeof(void *) == 8);
	auto const error_ptr_int_val = llvm::ConstantInt::get(
		context.get_uint64_t(),
		reinterpret_cast<uint64_t>(error_ptr)
	);
	context.builder.CreateCall(context.get_comptime_function(ctx::comptime_function_kind::add_error), { error_kind_val, error_ptr_int_val });
	auto const continue_bb = context.add_basic_block("error_dummy_continue");
	context.builder.CreateCondBr(llvm::ConstantInt::getFalse(context.get_llvm_context()), continue_bb, context.error_bb);
	context.builder.SetInsertPoint(continue_bb);
}

void emit_push_call(
	lex::src_tokens src_tokens,
	ast::function_body const *func_body,
	ctx::comptime_executor_context &context
)
{
	if (context.current_function.first != nullptr && context.current_function.first->is_no_comptime_checking())
	{
		return;
	}
	auto const call_ptr = context.insert_call(src_tokens, func_body);
	auto const call_ptr_int_val = llvm::ConstantInt::get(
		context.get_uint64_t(),
		reinterpret_cast<uint64_t>(call_ptr)
	);
	context.builder.CreateCall(context.get_comptime_function(ctx::comptime_function_kind::push_call), { call_ptr_int_val });
}

void emit_pop_call(ctx::comptime_executor_context &context)
{
	if (context.current_function.first != nullptr && context.current_function.first->is_no_comptime_checking())
	{
		return;
	}
	context.builder.CreateCall(context.get_comptime_function(ctx::comptime_function_kind::pop_call));
}

// ================================================================
// -------------------------- expression --------------------------
// ================================================================

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_identifier const &id,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	bz_assert(id.decl != nullptr);
	auto const val_ptr = context.get_variable(id.decl);
	if (val_ptr == nullptr && !id.decl->var_type.is<ast::ts_consteval>())
	{
		emit_error(
			{ id.id.tokens.begin, id.id.tokens.begin, id.id.tokens.end },
			bz::format("variable '{}' cannot be used in a constant expression", id.id.format_as_unqualified()),
			context
		);
		if (result_address == nullptr)
		{
			auto const result_type = llvm::PointerType::get(get_llvm_type(id.decl->var_type, context), 0);
			return { val_ptr::reference, llvm::UndefValue::get(result_type) };
		}
		else
		{
			return { val_ptr::reference, result_address };
		}
	}
	else if (val_ptr == nullptr /* && consteval */)
	{
		bz_assert(id.decl->init_expr.not_error() && id.decl->init_expr.is<ast::constant_expression>());
		auto &const_expr = id.decl->init_expr.get<ast::constant_expression>();
		auto const value = get_value<abi>(const_expr.value, const_expr.type, &const_expr, context);
		if (result_address == nullptr)
		{
			return { val_ptr::value, value };
		}
		else
		{
			auto const loaded_var = context.builder.CreateLoad(val_ptr);
			context.builder.CreateStore(loaded_var, result_address);
			return { val_ptr::reference, result_address, value };
		}
	}
	else
	{
		if (result_address == nullptr)
		{
			return { val_ptr::reference, val_ptr };
		}
		else
		{
			auto const loaded_var = context.builder.CreateLoad(val_ptr);
			context.builder.CreateStore(loaded_var, result_address);
			return { val_ptr::reference, result_address };
		}
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_literal const &,
	ctx::comptime_executor_context &,
	llvm::Value *
)
{
	// this should never be called, as a literal will always be an rvalue constant expression
	bz_unreachable;
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_tuple const &tuple_expr,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	if (result_address == nullptr)
	{
		auto const types = tuple_expr.elems
			.transform([](auto const &expr) { return expr.get_expr_type_and_kind().first; })
			.transform([&context](auto const ts) { return get_llvm_type(ts, context); })
			.template collect<ast::arena_vector>();
		auto const result_type = context.get_tuple_t(types);
		result_address = context.create_alloca(result_type);
	}

	for (unsigned i = 0; i < tuple_expr.elems.size(); ++i)
	{
		auto const elem_result_address = context.builder.CreateStructGEP(result_address, i);
		emit_bitcode<abi>(tuple_expr.elems[i], context, elem_result_address);
	}
	return { val_ptr::reference, result_address };
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_unary_op const &unary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	switch (unary_op.op)
	{
	// ==== non-overloadable ====
	case lex::token::address_of:         // '&'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context, nullptr);
		if (val.kind != val_ptr::reference)
		{
			if (auto const id_expr = unary_op.expr.get_expr().get_if<ast::expr_identifier>(); id_expr && id_expr->decl != nullptr)
			{
				emit_error(
					unary_op.expr.src_tokens,
					bz::format("unable to take address of variable '{}'", id_expr->decl->id.format_as_unqualified()),
					context
				);
			}
			else
			{
				emit_error(unary_op.expr.src_tokens, "unable to take address of value", context);
			}
			// just make sure the returned value is valid
			if (result_address == nullptr)
			{
				auto const ptr_type = llvm::PointerType::get(val.val->getType(), 0);
				return { val_ptr::value, llvm::Constant::getNullValue(ptr_type) };
			}
			else
			{
				return { val_ptr::reference, result_address };
			}
		}
		else
		{
			if (result_address == nullptr)
			{
				return { val_ptr::value, val.val };
			}
			else
			{
				context.builder.CreateStore(val.val, result_address);
				return { val_ptr::reference, result_address };
			}
		}
	}
	case lex::token::kw_sizeof:          // 'sizeof'
		bz_unreachable;
		return {};

	// ==== overloadable ====
	case lex::token::plus:               // '+'
	{
		return emit_bitcode<abi>(unary_op.expr, context, result_address);
	}
	case lex::token::minus:              // '-'
	{
		auto const expr_t = ast::remove_const_or_consteval(unary_op.expr.get_expr_type_and_kind().first);
		bz_assert(expr_t.is<ast::ts_base_type>());
		auto const expr_kind = expr_t.get<ast::ts_base_type>().info->kind;
		auto const val = emit_bitcode<abi>(unary_op.expr, context, nullptr).get_value(context.builder);
		auto const res = ctx::is_floating_point_kind(expr_kind)
			? context.builder.CreateFNeg(val, "unary_minus_tmp")
			: context.builder.CreateNeg(val, "unary_minus_tmp");
		if (result_address == nullptr)
		{
			return { val_ptr::value, res };
		}
		else
		{
			context.builder.CreateStore(res, result_address);
			return { val_ptr::reference, result_address };
		}
	}
	case lex::token::dereference:        // '*'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context, nullptr).get_value(context.builder);
		if (result_address == nullptr)
		{
			return { val_ptr::reference, val };
		}
		else
		{
			auto const loaded_val = context.builder.CreateLoad(val);
			context.builder.CreateStore(loaded_val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
	case lex::token::bit_not:            // '~'
	case lex::token::bool_not:           // '!'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context, nullptr).get_value(context.builder);
		auto const res = context.builder.CreateNot(val, "unary_bit_not_tmp");
		if (result_address == nullptr)
		{
			return { val_ptr::value, res };
		}
		else
		{
			context.builder.CreateStore(res, result_address);
			return { val_ptr::reference, result_address };
		}
	}

	case lex::token::plus_plus:          // '++'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context, nullptr);
		bz_assert(val.kind == val_ptr::reference);
		auto const original_value = val.get_value(context.builder);
		if (original_value->getType()->isPointerTy())
		{
			auto const incremented_value = context.builder.CreateConstGEP1_64(original_value, 1);
			context.builder.CreateStore(incremented_value, val.val);
			if (result_address == nullptr)
			{
				return val;
			}
			else
			{
				context.builder.CreateStore(incremented_value, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else
		{
			bz_assert(original_value->getType()->isIntegerTy());
			auto const incremented_value = context.builder.CreateAdd(
				original_value,
				llvm::ConstantInt::get(original_value->getType(), 1)
			);
			context.builder.CreateStore(incremented_value, val.val);
			if (result_address == nullptr)
			{
				return val;
			}
			else
			{
				context.builder.CreateStore(incremented_value, result_address);
				return { val_ptr::reference, result_address };
			}
		}
	}
	case lex::token::minus_minus:        // '--'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context, nullptr);
		bz_assert(val.kind == val_ptr::reference);
		auto const original_value = val.get_value(context.builder);
		if (original_value->getType()->isPointerTy())
		{
			auto const incremented_value = context.builder.CreateConstGEP1_64(original_value, uint64_t(-1));
			context.builder.CreateStore(incremented_value, val.val);
			if (result_address == nullptr)
			{
				return val;
			}
			else
			{
				context.builder.CreateStore(incremented_value, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else
		{
			bz_assert(original_value->getType()->isIntegerTy());
			auto const incremented_value = context.builder.CreateAdd(
				original_value,
				llvm::ConstantInt::get(original_value->getType(), uint64_t(-1))
			);
			context.builder.CreateStore(incremented_value, val.val);
			if (result_address == nullptr)
			{
				return val;
			}
			else
			{
				context.builder.CreateStore(incremented_value, result_address);
				return { val_ptr::reference, result_address };
			}
		}
	}
	default:
		bz_unreachable;
		return {};
	}
}


template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_assign(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode<abi>(binary_op.rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val = emit_bitcode<abi>(binary_op.lhs, context, nullptr);
	bz_assert(lhs_val.kind == val_ptr::reference);
	context.builder.CreateStore(rhs_val, lhs_val.val);
	if (result_address == nullptr)
	{
		return lhs_val;
	}
	else
	{
		context.builder.CreateStore(rhs_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_plus(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind))
		{
			auto const lhs_val = emit_bitcode<abi>(binary_op.lhs, context, nullptr).get_value(context.builder);
			auto const rhs_val = emit_bitcode<abi>(binary_op.rhs, context, nullptr).get_value(context.builder);
			auto const result_val = ctx::is_floating_point_kind(lhs_kind)
				? context.builder.CreateFAdd(lhs_val, rhs_val, "add_tmp")
				: context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			if (result_address == nullptr)
			{
				return { val_ptr::value, result_val };
			}
			else
			{
				context.builder.CreateStore(result_val, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else if (lhs_kind == ast::type_info::char_)
		{
			auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
			auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_uint32_t(),
				ctx::is_signed_integer_kind(rhs_kind)
			);
			auto const result_val = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			if (result_address == nullptr)
			{
				return { val_ptr::value, result_val };
			}
			else
			{
				context.builder.CreateStore(result_val, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else // if (rhs_kind == ast::type_info::char_)
		{
			bz_assert(rhs_kind == ast::type_info::char_);
			auto lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
			lhs_val = context.builder.CreateIntCast(
				lhs_val,
				context.get_uint32_t(),
				ctx::is_signed_integer_kind(lhs_kind)
			);
			auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const result_val = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			if (result_address == nullptr)
			{
				return { val_ptr::value, result_val };
			}
			else
			{
				context.builder.CreateStore(result_val, result_address);
				return { val_ptr::reference, result_address };
			}
		}
	}
	else if (lhs_t.is<ast::ts_pointer>())
	{
		bz_assert(rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_uint64_t(), false);
		}
		auto const result_val = context.builder.CreateGEP(lhs_val, rhs_val, "ptr_add_tmp");
		if (result_address == nullptr)
		{
			return { val_ptr::value, result_val };
		}
		else
		{
			context.builder.CreateStore(result_val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_pointer>());
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(lhs_kind))
		{
			lhs_val = context.builder.CreateIntCast(lhs_val, context.get_uint64_t(), false);
		}
		auto const result_val = context.builder.CreateGEP(rhs_val, lhs_val, "ptr_add_tmp");
		if (result_address == nullptr)
		{
			return { val_ptr::value, result_val };
		}
		else
		{
			context.builder.CreateStore(result_val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_plus_eq(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind))
		{
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context.builder);
			llvm::Value *res;
			if (ctx::is_integer_kind(lhs_kind))
			{
				res = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			}
			else
			{
				bz_assert(ctx::is_floating_point_kind(lhs_kind));
				bz_assert(lhs_kind == rhs_kind);
				res = context.builder.CreateFAdd(lhs_val, rhs_val, "add_tmp");
			}
			context.builder.CreateStore(res, lhs_val_ref.val);
			if (result_address == nullptr)
			{
				return lhs_val_ref;
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else
		{
			bz_assert(lhs_kind == ast::type_info::char_);
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context.builder);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_uint32_t(),
				ctx::is_signed_integer_kind(rhs_kind)
			);
			auto const res = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			context.builder.CreateStore(res, lhs_val_ref.val);
			if (result_address == nullptr)
			{
				return lhs_val_ref;
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return { val_ptr::reference, result_address };
			}
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		// we calculate the right hand side first
		auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_uint64_t(), false);
		}
		auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
		bz_assert(lhs_val_ref.kind == val_ptr::reference);
		auto const lhs_val = lhs_val_ref.get_value(context.builder);
		auto const res = context.builder.CreateGEP(lhs_val, rhs_val, "ptr_add_tmp");
		context.builder.CreateStore(res, lhs_val_ref.val);
		if (result_address == nullptr)
		{
			return lhs_val_ref;
		}
		else
		{
			context.builder.CreateStore(res, result_address);
			return { val_ptr::reference, result_address };
		}
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_minus(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind))
		{
			auto const lhs_val = emit_bitcode<abi>(binary_op.lhs, context, nullptr).get_value(context.builder);
			auto const rhs_val = emit_bitcode<abi>(binary_op.rhs, context, nullptr).get_value(context.builder);
			auto const result_val = ctx::is_floating_point_kind(lhs_kind)
				? context.builder.CreateFSub(lhs_val, rhs_val, "sub_tmp")
				: context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			if (result_address == nullptr)
			{
				return { val_ptr::value, result_val };
			}
			else
			{
				context.builder.CreateStore(result_val, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else if (
			lhs_kind == ast::type_info::char_
			&& rhs_kind == ast::type_info::char_
		)
		{
			auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
			auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const result_val = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			if (result_address == nullptr)
			{
				return { val_ptr::value, result_val };
			}
			else
			{
				context.builder.CreateStore(result_val, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else
		{
			bz_assert(
				lhs_kind == ast::type_info::char_
				&& ctx::is_integer_kind(rhs_kind)
			);
			auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
			auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_int32_t(),
				ctx::is_signed_integer_kind(rhs_kind)
			);
			auto const result_val = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			if (result_address == nullptr)
			{
				return { val_ptr::value, result_val };
			}
			else
			{
				context.builder.CreateStore(result_val, result_address);
				return { val_ptr::reference, result_address };
			}
		}
	}
	else if (rhs_t.is<ast::ts_base_type>())
	{
		bz_assert(lhs_t.is<ast::ts_pointer>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_uint64_t(), false);
		}
		// negate rhs_val
		rhs_val = context.builder.CreateNeg(rhs_val);
		auto const result_val = context.builder.CreateGEP(lhs_val, rhs_val, "ptr_sub_tmp");
		if (result_address == nullptr)
		{
			return { val_ptr::value, result_val };
		}
		else
		{
			context.builder.CreateStore(result_val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_pointer>());
		auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		auto const result_val = context.builder.CreatePtrDiff(lhs_val, rhs_val, "ptr_diff_tmp");
		if (result_address == nullptr)
		{
			return { val_ptr::value, result_val };
		}
		else
		{
			context.builder.CreateStore(result_val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_minus_eq(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind))
		{
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context.builder);
			llvm::Value *res;
			if (ctx::is_integer_kind(lhs_kind))
			{
				rhs_val = context.builder.CreateIntCast(
					rhs_val,
					lhs_val->getType(),
					ctx::is_signed_integer_kind(rhs_kind)
				);
				res = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			}
			else
			{
				bz_assert(ctx::is_floating_point_kind(lhs_kind));
				bz_assert(lhs_kind == rhs_kind);
				res = context.builder.CreateFSub(lhs_val, rhs_val, "sub_tmp");
			}
			context.builder.CreateStore(res, lhs_val_ref.val);
			if (result_address == nullptr)
			{
				return lhs_val_ref;
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else
		{
			bz_assert(lhs_kind == ast::type_info::char_);
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context.builder);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_uint32_t(),
				ctx::is_signed_integer_kind(rhs_kind)
			);
			auto const res = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			context.builder.CreateStore(res, lhs_val_ref.val);
			if (result_address == nullptr)
			{
				return lhs_val_ref;
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return { val_ptr::reference, result_address };
			}
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		// we calculate the right hand side first
		auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_uint64_t(), false);
		}
		// negate rhs_val
		rhs_val = context.builder.CreateNeg(rhs_val);
		auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
		bz_assert(lhs_val_ref.kind == val_ptr::reference);
		auto const lhs_val = lhs_val_ref.get_value(context.builder);
		auto const res = context.builder.CreateGEP(lhs_val, rhs_val, "ptr_sub_tmp");
		context.builder.CreateStore(res, lhs_val_ref.val);
		if (result_address == nullptr)
		{
			return lhs_val_ref;
		}
		else
		{
			context.builder.CreateStore(res, result_address);
			return { val_ptr::reference, result_address };
		}
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_multiply(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind));
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = ctx::is_floating_point_kind(lhs_kind)
		? context.builder.CreateFMul(lhs_val, rhs_val, "mul_tmp")
		: context.builder.CreateMul(lhs_val, rhs_val, "mul_tmp");
	if (result_address == nullptr)
	{
		return { val_ptr::value, result_val };
	}
	else
	{
		context.builder.CreateStore(result_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_multiply_eq(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind));
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = ctx::is_integer_kind(lhs_kind)
		? context.builder.CreateMul(lhs_val, rhs_val, "mul_tmp")
		: context.builder.CreateFMul(lhs_val, rhs_val, "mul_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	if (result_address == nullptr)
	{
		return lhs_val_ref;
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_divide(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind));
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = ctx::is_signed_integer_kind(lhs_kind) ? context.builder.CreateSDiv(lhs_val, rhs_val, "div_tmp")
		: ctx::is_unsigned_integer_kind(lhs_kind) ? context.builder.CreateUDiv(lhs_val, rhs_val, "div_tmp")
		: context.builder.CreateFDiv(lhs_val, rhs_val, "div_tmp");
	if (result_address == nullptr)
	{
		return { val_ptr::value, result_val };
	}
	else
	{
		context.builder.CreateStore(result_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_divide_eq(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind));
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = ctx::is_signed_integer_kind(lhs_kind) ? context.builder.CreateSDiv(lhs_val, rhs_val, "div_tmp")
		: ctx::is_unsigned_integer_kind(lhs_kind) ? context.builder.CreateUDiv(lhs_val, rhs_val, "div_tmp")
		: context.builder.CreateFDiv(lhs_val, rhs_val, "div_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	if (result_address == nullptr)
	{
		return lhs_val_ref;
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_modulo(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ctx::is_integer_kind(lhs_kind) && ctx::is_integer_kind(rhs_kind));
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = ctx::is_signed_integer_kind(lhs_kind)
		? context.builder.CreateSRem(lhs_val, rhs_val, "mod_tmp")
		: context.builder.CreateURem(lhs_val, rhs_val, "mod_tmp");
	if (result_address == nullptr)
	{
		return { val_ptr::value, result_val };
	}
	else
	{
		context.builder.CreateStore(result_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_modulo_eq(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ctx::is_integer_kind(lhs_kind) && ctx::is_integer_kind(rhs_kind));
	// we calculate the right hand side first
	auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = ctx::is_signed_integer_kind(lhs_kind)
		? context.builder.CreateSRem(lhs_val, rhs_val, "mod_tmp")
		: context.builder.CreateURem(lhs_val, rhs_val, "mod_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	if (result_address == nullptr)
	{
		return lhs_val_ref;
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_cmp(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const op = binary_op.op;
	bz_assert(
		op == lex::token::equals
		|| op == lex::token::not_equals
		|| op == lex::token::less_than
		|| op == lex::token::less_than_eq
		|| op == lex::token::greater_than
		|| op == lex::token::greater_than_eq
	);
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);
	// 0: signed int
	// 1: unsigned int
	// 2: float
	auto const get_cmp_predicate = [op](int kind)
	{
		constexpr llvm::CmpInst::Predicate preds[3][6] = {
			{
				llvm::CmpInst::ICMP_EQ,
				llvm::CmpInst::ICMP_NE,
				llvm::CmpInst::ICMP_SLT,
				llvm::CmpInst::ICMP_SLE,
				llvm::CmpInst::ICMP_SGT,
				llvm::CmpInst::ICMP_SGE
			},
			{
				llvm::CmpInst::ICMP_EQ,
				llvm::CmpInst::ICMP_NE,
				llvm::CmpInst::ICMP_ULT,
				llvm::CmpInst::ICMP_ULE,
				llvm::CmpInst::ICMP_UGT,
				llvm::CmpInst::ICMP_UGE
			},
			{
				llvm::CmpInst::FCMP_OEQ,
				llvm::CmpInst::FCMP_ONE,
				llvm::CmpInst::FCMP_OLT,
				llvm::CmpInst::FCMP_OLE,
				llvm::CmpInst::FCMP_OGT,
				llvm::CmpInst::FCMP_OGE
			},
		};
		auto const pred = [op]() {
			switch (op)
			{
			case lex::token::equals:
				return 0;
			case lex::token::not_equals:
				return 1;
			case lex::token::less_than:
				return 2;
			case lex::token::less_than_eq:
				return 3;
			case lex::token::greater_than:
				return 4;
			case lex::token::greater_than_eq:
				return 5;
			default:
				return -1;
			}
		}();
		bz_assert(pred != -1);
		return preds[kind][pred];
	};

	if (lhs_t.is<ast::ts_base_type>())
	{
		bz_assert(rhs_t.is<ast::ts_base_type>());
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		bz_assert(lhs_kind != ast::type_info::str_);
		auto const pred = ctx::is_floating_point_kind(lhs_kind) ? get_cmp_predicate(2)
			: ctx::is_signed_integer_kind(lhs_kind) ? get_cmp_predicate(0)
			: get_cmp_predicate(1);
		auto const result_val = ctx::is_floating_point_kind(lhs_kind)
			? context.builder.CreateFCmp(pred, lhs_val, rhs_val)
			: context.builder.CreateICmp(pred, lhs_val, rhs_val);
		if (result_address == nullptr)
		{
			return { val_ptr::value, result_val };
		}
		else
		{
			context.builder.CreateStore(result_val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
	else // if pointer
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_pointer>());
		auto const lhs_ptr_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_ptr_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		auto const lhs_val = context.builder.CreatePtrToInt(lhs_ptr_val, context.get_uint64_t());
		auto const rhs_val = context.builder.CreatePtrToInt(rhs_ptr_val, context.get_uint64_t());
		auto const p = get_cmp_predicate(1); // unsigned compare
		auto const result_val = context.builder.CreateICmp(p, lhs_val, rhs_val, "cmp_tmp");
		if (result_address == nullptr)
		{
			return { val_ptr::value, result_val };
		}
		else
		{
			context.builder.CreateStore(result_val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_bit_and(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateAnd(lhs_val, rhs_val, "bit_and_tmp");
	if (result_address == nullptr)
	{
		return { val_ptr::value, result_val };
	}
	else
	{
		context.builder.CreateStore(result_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_bit_and_eq(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = context.builder.CreateAnd(lhs_val, rhs_val, "bit_and_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	if (result_address == nullptr)
	{
		return lhs_val_ref;
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_bit_xor(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateXor(lhs_val, rhs_val, "bit_xor_tmp");
	if (result_address == nullptr)
	{
		return { val_ptr::value, result_val };
	}
	else
	{
		context.builder.CreateStore(result_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_bit_xor_eq(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = context.builder.CreateXor(lhs_val, rhs_val, "bit_xor_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	if (result_address == nullptr)
	{
		return lhs_val_ref;
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_bit_or(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateOr(lhs_val, rhs_val, "bit_or_tmp");
	if (result_address == nullptr)
	{
		return { val_ptr::value, result_val };
	}
	else
	{
		context.builder.CreateStore(result_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_bit_or_eq(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = context.builder.CreateOr(lhs_val, rhs_val, "bit_or_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	if (result_address == nullptr)
	{
		return lhs_val_ref;
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_left_shift(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const cast_rhs_val = context.builder.CreateIntCast(rhs_val, context.get_builtin_type(lhs_kind), false);
	auto const result_val = context.builder.CreateShl(lhs_val, cast_rhs_val, "lshift_tmp");
	if (result_address == nullptr)
	{
		return { val_ptr::value, result_val };
	}
	else
	{
		context.builder.CreateStore(result_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_left_shift_eq(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const cast_rhs_val = context.builder.CreateIntCast(rhs_val, context.get_builtin_type(lhs_kind), false);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = context.builder.CreateShl(lhs_val, cast_rhs_val, "lshift_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	if (result_address == nullptr)
	{
		return lhs_val_ref;
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_right_shift(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const cast_rhs_val = context.builder.CreateIntCast(rhs_val, context.get_builtin_type(lhs_kind), false);
	auto const result_val = context.builder.CreateLShr(lhs_val, cast_rhs_val, "rshift_tmp");
	if (result_address == nullptr)
	{
		return { val_ptr::value, result_val };
	}
	else
	{
		context.builder.CreateStore(result_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_right_shift_eq(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const cast_rhs_val = context.builder.CreateIntCast(rhs_val, context.get_builtin_type(lhs_kind), false);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = context.builder.CreateLShr(lhs_val, cast_rhs_val, "rshift_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	if (result_address == nullptr)
	{
		return lhs_val_ref;
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_bool_and(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(lhs_kind == ast::type_info::bool_ && rhs_kind == ast::type_info::bool_);

	// generate computation of lhs
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const lhs_bb_end = context.builder.GetInsertBlock();

	// generate computation of rhs
	auto const rhs_bb = context.add_basic_block("bool_and_rhs");
	context.builder.SetInsertPoint(rhs_bb);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const rhs_bb_end = context.builder.GetInsertBlock();

	auto const end_bb = context.add_basic_block("bool_and_end");
	// generate branches for lhs_bb and rhs_bb
	context.builder.SetInsertPoint(lhs_bb_end);
	// if lhs_val is true we need to check rhs
	// if lhs_val is false we are done and the result is false
	context.builder.CreateCondBr(lhs_val, rhs_bb, end_bb);
	context.builder.SetInsertPoint(rhs_bb_end);
	context.builder.CreateBr(end_bb);

	// create a phi node to get the final value
	context.builder.SetInsertPoint(end_bb);
	auto const phi = context.builder.CreatePHI(lhs_val->getType(), 2, "bool_and_tmp");
	// coming from lhs always gives false
	phi->addIncoming(context.builder.getFalse(), lhs_bb_end);
	phi->addIncoming(rhs_val, rhs_bb_end);

	if (result_address == nullptr)
	{
		return { val_ptr::value, phi };
	}
	else
	{
		context.builder.CreateStore(phi, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_bool_xor(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(lhs_kind == ast::type_info::bool_ && rhs_kind == ast::type_info::bool_);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateXor(lhs_val, rhs_val, "bool_xor_tmp");
	if (result_address == nullptr)
	{
		return { val_ptr::value, result_val };
	}
	else
	{
		context.builder.CreateStore(result_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_binary_bool_or(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(lhs_kind == ast::type_info::bool_ && rhs_kind == ast::type_info::bool_);

	// generate computation of lhs
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const lhs_bb_end = context.builder.GetInsertBlock();

	// generate computation of rhs
	auto const rhs_bb = context.add_basic_block("bool_or_rhs");
	context.builder.SetInsertPoint(rhs_bb);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const rhs_bb_end = context.builder.GetInsertBlock();

	auto const end_bb = context.add_basic_block("bool_or_end");
	// generate branches for lhs_bb and rhs_bb
	context.builder.SetInsertPoint(lhs_bb_end);
	// if lhs_val is true we are done and the result if true
	// if lhs_val is false we need to check rhs
	context.builder.CreateCondBr(lhs_val, end_bb, rhs_bb);
	context.builder.SetInsertPoint(rhs_bb_end);
	context.builder.CreateBr(end_bb);

	// create a phi node to get the final value
	context.builder.SetInsertPoint(end_bb);
	auto const phi = context.builder.CreatePHI(lhs_val->getType(), 2, "bool_or_tmp");
	// coming from lhs always gives true
	phi->addIncoming(context.builder.getTrue(), lhs_bb_end);
	phi->addIncoming(rhs_val, rhs_bb_end);

	if (result_address == nullptr)
	{
		return { val_ptr::value, phi };
	}
	else
	{
		context.builder.CreateStore(phi, result_address);
		return { val_ptr::reference, result_address };
	}
}


template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_binary_op const &binary_op,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	switch (binary_op.op)
	{
	// ==== non-overloadable ====
	case lex::token::comma:              // ','
	{
		// treat the lhs of the comma expression as seperate, so destructors are called
		// before rhs is emitted
		context.push_expression_scope();
		emit_bitcode<abi>(binary_op.lhs, context, nullptr);
		context.pop_expression_scope();
		return emit_bitcode<abi>(binary_op.rhs, context, result_address);
	}

	// ==== overloadable ====
	case lex::token::assign:             // '='
		return emit_builtin_binary_assign<abi>(binary_op, context, result_address);
	case lex::token::plus:               // '+'
		return emit_builtin_binary_plus<abi>(binary_op, context, result_address);
	case lex::token::plus_eq:            // '+='
		return emit_builtin_binary_plus_eq<abi>(binary_op, context, result_address);
	case lex::token::minus:              // '-'
		return emit_builtin_binary_minus<abi>(binary_op, context, result_address);
	case lex::token::minus_eq:           // '-='
		return emit_builtin_binary_minus_eq<abi>(binary_op, context, result_address);
	case lex::token::multiply:           // '*'
		return emit_builtin_binary_multiply<abi>(binary_op, context, result_address);
	case lex::token::multiply_eq:        // '*='
		return emit_builtin_binary_multiply_eq<abi>(binary_op, context, result_address);
	case lex::token::divide:             // '/'
		return emit_builtin_binary_divide<abi>(binary_op, context, result_address);
	case lex::token::divide_eq:          // '/='
		return emit_builtin_binary_divide_eq<abi>(binary_op, context, result_address);
	case lex::token::modulo:             // '%'
		return emit_builtin_binary_modulo<abi>(binary_op, context, result_address);
	case lex::token::modulo_eq:          // '%='
		return emit_builtin_binary_modulo_eq<abi>(binary_op, context, result_address);
	case lex::token::equals:             // '=='
	case lex::token::not_equals:         // '!='
	case lex::token::less_than:          // '<'
	case lex::token::less_than_eq:       // '<='
	case lex::token::greater_than:       // '>'
	case lex::token::greater_than_eq:    // '>='
		return emit_builtin_binary_cmp<abi>(binary_op, context, result_address);
	case lex::token::bit_and:            // '&'
		return emit_builtin_binary_bit_and<abi>(binary_op, context, result_address);
	case lex::token::bit_and_eq:         // '&='
		return emit_builtin_binary_bit_and_eq<abi>(binary_op, context, result_address);
	case lex::token::bit_xor:            // '^'
		return emit_builtin_binary_bit_xor<abi>(binary_op, context, result_address);
	case lex::token::bit_xor_eq:         // '^='
		return emit_builtin_binary_bit_xor_eq<abi>(binary_op, context, result_address);
	case lex::token::bit_or:             // '|'
		return emit_builtin_binary_bit_or<abi>(binary_op, context, result_address);
	case lex::token::bit_or_eq:          // '|='
		return emit_builtin_binary_bit_or_eq<abi>(binary_op, context, result_address);
	case lex::token::bit_left_shift:     // '<<'
		return emit_builtin_binary_left_shift<abi>(binary_op, context, result_address);
	case lex::token::bit_left_shift_eq:  // '<<='
		return emit_builtin_binary_left_shift_eq<abi>(binary_op, context, result_address);
	case lex::token::bit_right_shift:    // '>>'
		return emit_builtin_binary_right_shift<abi>(binary_op, context, result_address);
	case lex::token::bit_right_shift_eq: // '>>='
		return emit_builtin_binary_right_shift_eq<abi>(binary_op, context, result_address);
	case lex::token::bool_and:           // '&&'
		return emit_builtin_binary_bool_and<abi>(binary_op, context, result_address);
	case lex::token::bool_xor:           // '^^'
		return emit_builtin_binary_bool_xor<abi>(binary_op, context, result_address);
	case lex::token::bool_or:            // '||'
		return emit_builtin_binary_bool_or<abi>(binary_op, context, result_address);

	// these have no built-in operations
	case lex::token::dot_dot:            // '..'
	case lex::token::dot_dot_eq:         // '..='
	default:
		bz_unreachable;
		return {};
	}
}

template<abi::platform_abi abi>
static void create_function_call(
	ast::function_body *body,
	val_ptr lhs,
	val_ptr rhs,
	ctx::comptime_executor_context &context
)
{
	bz_assert(lhs.kind == val_ptr::reference);
	bz_assert(rhs.kind == val_ptr::reference);
	auto const fn = context.get_function(body);
	bz_assert(fn != nullptr);
	auto const result_type = get_llvm_type(body->return_type, context);
	auto const result_pass_kind = abi::get_pass_kind<abi>(result_type, context.get_data_layout(), context.get_llvm_context());

	bz_assert(result_pass_kind != abi::pass_kind::reference);
	bz_assert(body->params[0].var_type.is<ast::ts_lvalue_reference>());

	ast::arena_vector<llvm::Value *> params;
	bool is_rhs_pass_by_ref = false;
	params.reserve(2);
	params.push_back(lhs.val);

	{
		auto const &rhs_p_t = body->params[1].var_type;
		if (rhs_p_t.is<ast::ts_lvalue_reference>())
		{
			bz_assert(rhs.kind == val_ptr::reference);
			params.push_back(rhs.val);
		}
		else
		{
			auto const rhs_llvm_type = get_llvm_type(rhs_p_t, context);
			auto const rhs_pass_kind = abi::get_pass_kind<abi>(rhs_llvm_type, context.get_data_layout(), context.get_llvm_context());

			switch (rhs_pass_kind)
			{
			case abi::pass_kind::reference:
				// there's no need to provide a seperate copy for a byval argument,
				// as a copy is made at the call site automatically
				// see: https://reviews.llvm.org/D79636
				params.push_back(rhs.val);
				is_rhs_pass_by_ref = true;
				break;
			case abi::pass_kind::value:
				params.push_back(rhs.get_value(context.builder));
				break;
			case abi::pass_kind::one_register:
				params.push_back(context.create_bitcast(
					rhs,
					abi::get_one_register_type<abi>(rhs_llvm_type, context.get_data_layout(), context.get_llvm_context())
				));
				break;
			case abi::pass_kind::two_registers:
			{
				auto const [first_type, second_type] = abi::get_two_register_types<abi>(rhs_llvm_type, context.get_data_layout(), context.get_llvm_context());
				auto const cast_val = context.create_bitcast(rhs, llvm::StructType::get(first_type, second_type));
				auto const first_val = context.builder.CreateExtractValue(cast_val, 0);
				auto const second_val = context.builder.CreateExtractValue(cast_val, 1);
				params.push_back(first_val);
				params.push_back(second_val);
				break;
			}
			}
		}
	}

	auto const call = context.builder.CreateCall(fn, llvm::ArrayRef(params.data(), params.size()));
	call->setCallingConv(fn->getCallingConv());
	if (is_rhs_pass_by_ref)
	{
		auto const i = call->arg_size() - 1;
		call->addParamAttr(i, llvm::Attribute::ByVal);
		call->addParamAttr(i, llvm::Attribute::NoAlias);
		call->addParamAttr(i, llvm::Attribute::NoCapture);
		call->addParamAttr(i, llvm::Attribute::NonNull);
	}
}

template<abi::platform_abi abi>
static void emit_assign(
	ast::type_info const *info,
	val_ptr lhs,
	val_ptr rhs,
	ctx::comptime_executor_context &context
)
{
	bz_assert(rhs.kind != val_ptr::value);
	if (info->kind != ast::type_info::aggregate)
	{
		context.builder.CreateStore(rhs.get_value(context.builder), lhs.val);
	}
	else if (info->op_assign == nullptr)
	{
		for (auto const [member, i] : info->member_variables.enumerate())
		{
			auto const lhs_ptr = context.builder.CreateStructGEP(lhs.val, i);
			auto const rhs_ptr = context.builder.CreateStructGEP(rhs.val, i);
			auto const member_t = ast::remove_const_or_consteval(member.type);
			if (member_t.is<ast::ts_base_type>())
			{
				emit_assign<abi>(
					member_t.get<ast::ts_base_type>().info,
					{ val_ptr::reference, lhs_ptr },
					{ val_ptr::reference, rhs_ptr },
					context
				);
			}
			else
			{
				auto const rhs_val = context.builder.CreateLoad(rhs_ptr);
				context.builder.CreateStore(rhs_val, lhs_ptr);
			}
		}
	}
	else
	{
		create_function_call<abi>(info->op_assign, lhs, rhs, context);
	}
}

template<abi::platform_abi abi>
static void emit_move_assign(
	ast::type_info const *info,
	val_ptr lhs,
	val_ptr rhs,
	ctx::comptime_executor_context &context
)
{
	if (rhs.kind == val_ptr::value || info->kind != ast::type_info::aggregate)
	{
		context.builder.CreateStore(rhs.get_value(context.builder), lhs.val);
	}
	else if (info->op_move_assign == nullptr)
	{
		for (auto const [member, i] : info->member_variables.enumerate())
		{
			auto const lhs_ptr = context.builder.CreateStructGEP(lhs.val, i);
			auto const rhs_ptr = context.builder.CreateStructGEP(rhs.val, i);
			auto const member_t = ast::remove_const_or_consteval(member.type);
			if (member_t.is<ast::ts_base_type>())
			{
				emit_move_assign<abi>(
					member_t.get<ast::ts_base_type>().info,
					{ val_ptr::reference, lhs_ptr },
					{ val_ptr::reference, rhs_ptr },
					context
				);
			}
			else
			{
				auto const rhs_val = context.builder.CreateLoad(rhs_ptr);
				context.builder.CreateStore(rhs_val, lhs_ptr);
			}
		}
	}
	else
	{
		create_function_call<abi>(info->op_move_assign, lhs, rhs, context);
	}
}

template<abi::platform_abi abi>
static val_ptr emit_default_assign(
	ast::expression const &lhs,
	ast::expression const &rhs,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, result_address);
	bz_assert(rhs_val.kind == val_ptr::reference);
	bz_assert(lhs_val.kind == val_ptr::reference);

	auto const base_type = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	bz_assert(base_type.is<ast::ts_base_type>());
	auto const info = base_type.get<ast::ts_base_type>().info;
	emit_assign<abi>(info, lhs_val, rhs_val, context);

	return lhs_val;
}

template<abi::platform_abi abi>
static val_ptr emit_default_move_assign(
	ast::expression const &lhs,
	ast::expression const &rhs,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, result_address);
	bz_assert(lhs_val.kind == val_ptr::reference);

	auto const base_type = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	bz_assert(base_type.is<ast::ts_base_type>());
	auto const info = base_type.get<ast::ts_base_type>().info;
	emit_move_assign<abi>(info, lhs_val, rhs_val, context);

	return lhs_val;
}

template<abi::platform_abi abi>
static val_ptr emit_copy_constructor(
	lex::src_tokens src_tokens,
	val_ptr expr_val,
	ast::typespec_view expr_type,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	if (expr_val.kind == val_ptr::value && result_address == nullptr)
	{
		return expr_val;
	}
	else if (expr_val.kind == val_ptr::value)
	{
		context.builder.CreateStore(expr_val.get_value(context.builder), result_address);
		return { val_ptr::reference, result_address };
	}

	if (result_address == nullptr)
	{
		result_address = context.create_alloca(get_llvm_type(expr_type, context));
	}

	if (expr_type.is<ast::ts_base_type>())
	{
		auto const info = expr_type.get<ast::ts_base_type>().info;
		if (info->copy_constructor != nullptr)
		{
			emit_push_call(src_tokens, info->copy_constructor, context);
			auto const fn = context.get_function(info->copy_constructor);
			auto const ret_kind = abi::get_pass_kind<abi>(expr_val.get_type(), context.get_data_layout(), context.get_llvm_context());
			switch (ret_kind)
			{
			case abi::pass_kind::value:
			{
				emit_push_call(src_tokens, info->copy_constructor, context);
				auto const call = context.builder.CreateCall(fn, expr_val.val);
				emit_pop_call(context);
				context.builder.CreateStore(call, result_address);
				break;
			}
			case abi::pass_kind::reference:
			{
				context.builder.CreateCall(fn, { result_address, expr_val.val });
				break;
			}
			case abi::pass_kind::one_register:
			case abi::pass_kind::two_registers:
			{
				auto const call = context.builder.CreateCall(fn, expr_val.val);
				auto const cast_result_address = context.builder.CreatePointerCast(
					result_address, llvm::PointerType::get(call->getType(), 0)
				);
				context.builder.CreateStore(call, cast_result_address);
				break;
			}
			}
			emit_pop_call(context);
		}
		else if (info->default_copy_constructor != nullptr)
		{
			for (auto const &[member, i] : info->member_variables.enumerate())
			{
				emit_copy_constructor<abi>(
					{ val_ptr::reference, context.builder.CreateStructGEP(expr_val.val, i) },
					member.type,
					context,
					context.builder.CreateStructGEP(result_address, i)
				);
			}
		}
		else
		{
			context.builder.CreateStore(expr_val.get_value(context.builder), result_address);
		}
	}
	else if (expr_type.is<ast::ts_array>())
	{
		auto const type = expr_type.get<ast::ts_array>().elem_type.as_typespec_view();
		for (auto const i : bz::iota(0, expr_type.get<ast::ts_array>().size))
		{
			emit_copy_constructor<abi>(
				{ val_ptr::reference, context.builder.CreateStructGEP(expr_val.val, i) },
				type,
				context,
				context.builder.CreateStructGEP(result_address, i)
			);
		}
	}
	else if (expr_type.is<ast::ts_tuple>())
	{
		for (auto const &[member_type, i] : expr_type.get<ast::ts_tuple>().types.enumerate())
		{
			emit_copy_constructor<abi>(
				{ val_ptr::reference, context.builder.CreateStructGEP(expr_val.val, i) },
				member_type,
				context,
				context.builder.CreateStructGEP(result_address, i)
			);
		}
	}
	else
	{
		context.builder.CreateStore(expr_val.get_value(context.builder), result_address);
	}
	return { val_ptr::reference, result_address };
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_function_call const &func_call,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	if (func_call.func_body->is_intrinsic())
	{
		switch (func_call.func_body->intrinsic_kind)
		{
		static_assert(ast::function_body::_builtin_last - ast::function_body::_builtin_first == 83);
		case ast::function_body::builtin_str_begin_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const arg = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const begin_ptr = context.builder.CreateExtractValue(arg, 0);
			if (result_address != nullptr)
			{
				context.builder.CreateStore(begin_ptr, result_address);
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				return val_ptr{ val_ptr::value, begin_ptr };
			}
		}
		case ast::function_body::builtin_str_end_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const arg = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const end_ptr = context.builder.CreateExtractValue(arg, 1);
			if (result_address != nullptr)
			{
				context.builder.CreateStore(end_ptr, result_address);
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				return val_ptr{ val_ptr::value, end_ptr };
			}
		}
		case ast::function_body::builtin_str_size:
		{
			bz_assert(func_call.params.size() == 1);
			auto const str = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			bz_assert(str->getType()->isStructTy());
			auto const begin_ptr = context.builder.CreateExtractValue(str, 0);
			auto const end_ptr   = context.builder.CreateExtractValue(str, 1);
			auto const size_ptr_diff = context.builder.CreatePtrDiff(end_ptr, begin_ptr);
			auto const size = context.builder.CreateIntCast(size_ptr_diff, context.get_usize_t(), false);
			if (result_address != nullptr)
			{
				context.builder.CreateStore(size, result_address);
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				return val_ptr{ val_ptr::value, size };
			}
		}
		case ast::function_body::builtin_str_from_ptrs:
		{
			bz_assert(func_call.params.size() == 2);
			auto const begin_ptr = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const end_ptr   = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
			if (result_address != nullptr)
			{
				auto const result_begin_ptr = context.builder.CreateStructGEP(result_address, 0);
				auto const result_end_ptr   = context.builder.CreateStructGEP(result_address, 1);
				context.builder.CreateStore(begin_ptr, result_begin_ptr);
				context.builder.CreateStore(end_ptr, result_end_ptr);
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				bz_assert(context.get_str_t()->isStructTy());
				auto const str_t = static_cast<llvm::StructType *>(context.get_str_t());
				auto const str_member_t= str_t->getElementType(0);
				llvm::Value *result = llvm::ConstantStruct::get(
					str_t,
					{ llvm::UndefValue::get(str_member_t), llvm::UndefValue::get(str_member_t) }
				);
				result = context.builder.CreateInsertValue(result, begin_ptr, 0);
				result = context.builder.CreateInsertValue(result, end_ptr,   1);
				return val_ptr{ val_ptr::value, result };
			}
		}
		case ast::function_body::builtin_slice_begin_ptr:
		case ast::function_body::builtin_slice_begin_const_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const slice = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const begin_ptr = context.builder.CreateExtractValue(slice, 0);
			if (result_address != nullptr)
			{
				context.builder.CreateStore(begin_ptr, result_address);
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				return val_ptr{ val_ptr::value, begin_ptr };
			}
		}
		case ast::function_body::builtin_slice_end_ptr:
		case ast::function_body::builtin_slice_end_const_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const slice = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const end_ptr = context.builder.CreateExtractValue(slice, 1);
			if (result_address != nullptr)
			{
				context.builder.CreateStore(end_ptr, result_address);
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				return val_ptr{ val_ptr::value, end_ptr };
			}
		}
		case ast::function_body::builtin_slice_size:
		{
			bz_assert(func_call.params.size() == 1);
			auto const slice = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			bz_assert(slice->getType()->isStructTy());
			auto const begin_ptr = context.builder.CreateExtractValue(slice, 0);
			auto const end_ptr   = context.builder.CreateExtractValue(slice, 1);
			auto const size_ptr_diff = context.builder.CreatePtrDiff(end_ptr, begin_ptr);
			auto const size = context.builder.CreateIntCast(size_ptr_diff, context.get_usize_t(), false);
			if (result_address != nullptr)
			{
				context.builder.CreateStore(size, result_address);
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				return val_ptr{ val_ptr::value, size };
			}
		}
		case ast::function_body::builtin_slice_from_ptrs:
		case ast::function_body::builtin_slice_from_const_ptrs:
		{
			bz_assert(func_call.params.size() == 2);
			auto const begin_ptr = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const end_ptr   = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
			if (result_address != nullptr)
			{
				auto const result_begin_ptr = context.builder.CreateStructGEP(result_address, 0);
				auto const result_end_ptr   = context.builder.CreateStructGEP(result_address, 1);
				context.builder.CreateStore(begin_ptr, result_begin_ptr);
				context.builder.CreateStore(end_ptr, result_end_ptr);
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				bz_assert(begin_ptr->getType()->isPointerTy());
				auto const slice_elem_t = static_cast<llvm::PointerType *>(begin_ptr->getType())->getElementType();
				auto const slice_t = context.get_slice_t(slice_elem_t);
				auto const slice_member_t = slice_t->getElementType(0);
				llvm::Value *result = llvm::ConstantStruct::get(
					slice_t,
					{ llvm::UndefValue::get(slice_member_t), llvm::UndefValue::get(slice_member_t) }
				);
				result = context.builder.CreateInsertValue(result, begin_ptr, 0);
				result = context.builder.CreateInsertValue(result, end_ptr,   1);
				return val_ptr{ val_ptr::value, result };
			}
		}
		case ast::function_body::builtin_pointer_cast:
		{
			bz_assert(func_call.params.size() == 2);
			bz_assert(func_call.params[0].is_typename());
			auto const dest_type = get_llvm_type(func_call.params[0].get_typename(), context);
			bz_assert(dest_type->isPointerTy());
			auto const ptr = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
			bz_assert(ptr->getType()->isPointerTy());
			auto const result = context.builder.CreatePointerCast(ptr, dest_type);
			if (result_address != nullptr)
			{
				context.builder.CreateStore(result, result_address);
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				return { val_ptr::value, result };
			}
		}
		case ast::function_body::builtin_pointer_to_int:
		case ast::function_body::builtin_int_to_pointer:
		{
			emit_error(
				func_call.src_tokens,
				bz::format("'{}' cannot be used in a constant expression", func_call.func_body->get_signature()), context
			);
			if (result_address != nullptr)
			{
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				return { val_ptr::value, llvm::UndefValue::get(get_llvm_type(func_call.func_body->return_type, context)) };
			}
		}
		case ast::function_body::builtin_call_destructor:
		{
			bz_assert(func_call.params.size() == 1);
			auto const type = func_call.params[0].get_expr_type_and_kind().first;
			auto const arg = emit_bitcode<abi>(func_call.params[0], context, nullptr);
			bz_assert(arg.kind == val_ptr::reference);
			emit_destructor_call(func_call.src_tokens, arg.val, type, context);
			return {};
		}
		case ast::function_body::print_stdout:
		case ast::function_body::print_stderr:
		case ast::function_body::println_stdout:
		case ast::function_body::println_stderr:
		{
			emit_error(
				func_call.src_tokens,
				bz::format("'{}' cannot be used in a constant expression", func_call.func_body->get_signature()), context
			);
			if (result_address != nullptr)
			{
				return val_ptr{ val_ptr::reference, result_address };
			}
			else
			{
				return { val_ptr::value, llvm::UndefValue::get(get_llvm_type(func_call.func_body->return_type, context)) };
			}
		}

		default:
			break;
		}
	}
	else if (func_call.func_body->is_default_op_assign())
	{
		return emit_default_assign<abi>(func_call.params[0], func_call.params[1], context, result_address);
	}
	else if (func_call.func_body->is_default_op_move_assign())
	{
		return emit_default_move_assign<abi>(func_call.params[0], func_call.params[1], context, result_address);
	}
	else if (func_call.func_body->is_default_copy_constructor())
	{
		auto const expr_val = emit_bitcode<abi>(func_call.params[0], context, nullptr);
		auto const expr_type = ast::remove_const_or_consteval(func_call.params[0].get_expr_type_and_kind().first);
		return emit_copy_constructor<abi>(func_call.src_tokens, expr_val, expr_type, context, result_address);
	}

	auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
	auto const result_kind = abi::get_pass_kind<abi>(result_type, context.get_data_layout(), context.get_llvm_context());

	bz_assert(func_call.func_body != nullptr);
	if (!func_call.func_body->is_intrinsic() && func_call.func_body->body.is_null())
	{
		emit_error(
			func_call.src_tokens,
			bz::format("unable to call external function '{}' in compile time execution", func_call.func_body->get_signature()),
			context
		);
		if (result_address != nullptr)
		{
			return val_ptr{ val_ptr::reference, result_address };
		}
		else if (result_type->isVoidTy())
		{
			return {};
		}
		else
		{
			return val_ptr{ val_ptr::value, llvm::UndefValue::get(result_type) };
		}
	}
	auto const fn = context.get_function(func_call.func_body);
	bz_assert(fn != nullptr);

	ast::arena_vector<llvm::Value *> params = {};
	ast::arena_vector<bool> params_is_pass_by_ref = {};
	params.reserve(func_call.params.size() + (result_kind == abi::pass_kind::reference ? 1 : 0));
	params_is_pass_by_ref.reserve(func_call.params.size() + (result_kind == abi::pass_kind::reference ? 1 : 0));

	if (result_kind == abi::pass_kind::reference)
	{
		auto const output_ptr = result_address != nullptr
			? result_address
			: context.create_alloca(result_type);
		params.push_back(output_ptr);
		params_is_pass_by_ref.push_back(false);
	}

	auto const emit_arg = [&]<bool push_to_front>(size_t i, bz::meta::integral_constant<bool, push_to_front>)
	{
		using params_push_type = decltype(params)::value_type &(bz::meta::remove_cv_reference<decltype(params)>::*)(decltype(params)::value_type const &);
		using ref_push_type = decltype(params_is_pass_by_ref)::value_type &(bz::meta::remove_cv_reference<decltype(params_is_pass_by_ref)>::*)(decltype(params_is_pass_by_ref)::value_type const &);
		constexpr auto params_push = push_to_front
			? static_cast<params_push_type>(&decltype(params)::push_front)
			: static_cast<params_push_type>(&decltype(params)::push_back);
		constexpr auto ref_push = push_to_front
			? static_cast<ref_push_type>(&decltype(params_is_pass_by_ref)::push_front)
			: static_cast<ref_push_type>(&decltype(params_is_pass_by_ref)::push_back);
		auto &p = func_call.params[i];
		auto &p_t = func_call.func_body->params[i].var_type;
		auto const param_val = emit_bitcode<abi>(p, context, nullptr);
		if (p_t.is_typename())
		{
			// do nothing for typename args
			return;
		}
		else if (p_t.is<ast::ts_lvalue_reference>())
		{
			bz_assert(param_val.kind == val_ptr::reference);
			(params.*params_push)(param_val.val);
			(params_is_pass_by_ref.*ref_push)(false);
		}
		// *void and *const void
		else if (ast::remove_const_or_consteval(ast::remove_pointer(p_t)).is<ast::ts_void>())
		{
			auto const void_ptr_val = context.builder.CreatePointerCast(
				param_val.get_value(context.builder),
				llvm::PointerType::getInt8PtrTy(context.get_llvm_context())
			);
			(params.*params_push)(void_ptr_val);
			(params_is_pass_by_ref.*ref_push)(false);
		}
		else
		{
			auto const param_llvm_type = get_llvm_type(p_t, context);
			auto const pass_kind = abi::get_pass_kind<abi>(param_llvm_type, context.get_data_layout(), context.get_llvm_context());

			switch (pass_kind)
			{
			case abi::pass_kind::reference:
				// there's no need to provide a seperate copy for a byval argument,
				// as a copy is made at the call site automatically
				// see: https://reviews.llvm.org/D79636
				if (param_val.kind == val_ptr::reference)
				{
					(params.*params_push)(param_val.val);
				}
				else
				{
					auto const val = param_val.get_value(context.builder);
					auto const alloca = context.create_alloca(param_llvm_type);
					context.builder.CreateStore(val, alloca);
					(params.*params_push)(alloca);
				}
				(params_is_pass_by_ref.*ref_push)(true);
				break;
			case abi::pass_kind::value:
				(params.*params_push)(param_val.get_value(context.builder));
				(params_is_pass_by_ref.*ref_push)(false);
				break;
			case abi::pass_kind::one_register:
				(params.*params_push)(context.create_bitcast(
					param_val,
					abi::get_one_register_type<abi>(param_llvm_type, context.get_data_layout(), context.get_llvm_context())
				));
				(params_is_pass_by_ref.*ref_push)(false);
				break;
			case abi::pass_kind::two_registers:
			{
				auto const [first_type, second_type] = abi::get_two_register_types<abi>(param_llvm_type, context.get_data_layout(), context.get_llvm_context());
				auto const cast_val = context.create_bitcast(param_val, llvm::StructType::get(first_type, second_type));
				auto const first_val = context.builder.CreateExtractValue(cast_val, 0);
				auto const second_val = context.builder.CreateExtractValue(cast_val, 1);
				if constexpr (push_to_front)
				{
					params.push_front(second_val);
					params_is_pass_by_ref.push_front(false);
					params.push_front(first_val);
					params_is_pass_by_ref.push_front(false);
				}
				else
				{
					params.push_back(first_val);
					params_is_pass_by_ref.push_back(false);
					params.push_back(second_val);
					params_is_pass_by_ref.push_back(false);
				}
				break;
			}
			}
		}
	};

	if (func_call.param_resolve_order == ast::resolve_order::reversed)
	{
		auto const size = func_call.params.size();
		for (size_t i = size; i != 0; --i)
		{
			emit_arg(i - 1, bz::meta::integral_constant<bool, true>{});
		}
	}
	else
	{
		auto const size = func_call.params.size();
		for (size_t i = 0; i < size; ++i)
		{
			emit_arg(i, bz::meta::integral_constant<bool, false>{});
		}
	}

	if (!func_call.func_body->is_no_comptime_checking())
	{
		emit_push_call(func_call.src_tokens, func_call.func_body, context);
	}
	auto const call = context.builder.CreateCall(fn, llvm::ArrayRef(params.data(), params.size()));
	call->setCallingConv(fn->getCallingConv());
	auto is_pass_by_ref_it = params_is_pass_by_ref.begin();
	auto const is_pass_by_ref_end = params_is_pass_by_ref.end();
	unsigned i = 0;
	bz_assert(fn->arg_size() == call->arg_size());
	if (result_kind == abi::pass_kind::reference)
	{
		call->addParamAttr(0, llvm::Attribute::StructRet);
		bz_assert(is_pass_by_ref_it != is_pass_by_ref_end);
		++is_pass_by_ref_it, ++i;
	}
	for (; is_pass_by_ref_it != is_pass_by_ref_end; ++is_pass_by_ref_it, ++i)
	{
		auto const is_pass_by_ref = *is_pass_by_ref_it;
		if (is_pass_by_ref)
		{
			call->addParamAttr(i, llvm::Attribute::ByVal);
			call->addParamAttr(i, llvm::Attribute::NoAlias);
			call->addParamAttr(i, llvm::Attribute::NoCapture);
			call->addParamAttr(i, llvm::Attribute::NonNull);
		}
	}

	if (!func_call.func_body->is_no_comptime_checking())
	{
		emit_pop_call(context);
		emit_error_check(context);
	}

	switch (result_kind)
	{
	case abi::pass_kind::reference:
		bz_assert(result_address == nullptr || params.front() == result_address);
		return { val_ptr::reference, params.front() };
	case abi::pass_kind::value:
		if (call->getType()->isVoidTy())
		{
			return {};
		}
		else if (func_call.func_body->return_type.is<ast::ts_lvalue_reference>())
		{
			if (result_address == nullptr)
			{
				return { val_ptr::reference, call };
			}
			else
			{
				auto const loaded_val = context.builder.CreateLoad(call);
				context.builder.CreateStore(loaded_val, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		if (result_address == nullptr)
		{
			return { val_ptr::value, call };
		}
		else
		{
			context.builder.CreateStore(call, result_address);
			return { val_ptr::reference, result_address };
		}
	case abi::pass_kind::one_register:
	case abi::pass_kind::two_registers:
	{
		auto const call_result_type = call->getType();
		if (result_address != nullptr)
		{
			auto const result_ptr = context.builder.CreateBitCast(
				result_address,
				llvm::PointerType::get(call_result_type, 0)
			);
			context.builder.CreateStore(call, result_ptr);
			return { val_ptr::reference, result_address };
		}
		else if (result_type == call_result_type)
		{
			return { val_ptr::value, call };
		}
		else
		{
			auto const result_ptr = context.create_alloca(result_type);
			auto const result_ptr_cast = context.builder.CreateBitCast(
				result_ptr,
				llvm::PointerType::get(call_result_type, 0)
			);
			context.builder.CreateStore(call, result_ptr_cast);
			return { val_ptr::reference, result_ptr };
		}
	}
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_subscript const &subscript,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const base_type = ast::remove_const_or_consteval(subscript.base.get_expr_type_and_kind().first);
	if (base_type.is<ast::ts_array>())
	{
		auto const array = emit_bitcode<abi>(subscript.base, context, nullptr);
		auto index_val = emit_bitcode<abi>(subscript.index, context, nullptr).get_value(context.builder);
		bz_assert(ast::remove_const_or_consteval(subscript.index.get_expr_type_and_kind().first).is<ast::ts_base_type>());
		auto const kind = ast::remove_const_or_consteval(subscript.index.get_expr_type_and_kind().first).get<ast::ts_base_type>().info->kind;
		bz_assert(context.get_register_size() == 8);
		// cast to pointer-size integers
		if (ctx::is_integer_kind(kind))
		{
			index_val = context.builder.CreateIntCast(index_val, context.get_int64_t(), ctx::is_signed_integer_kind(kind));
		}

		// array bounds check
		{
			auto const array_size = base_type.get<ast::ts_array>().size;
			auto const array_size_val = llvm::ConstantInt::get(context.get_uint64_t(), array_size);
			auto const is_in_bounds = [&]() -> llvm::Value * {
				if (ctx::is_unsigned_integer_kind(kind))
				{
					return context.builder.CreateICmp(llvm::CmpInst::ICMP_ULT, index_val, array_size_val);
				}
				else
				{
					auto const is_less_than = context.builder.CreateICmp(llvm::CmpInst::ICMP_SLT, index_val, array_size_val);
					auto const is_positive_or_zero = context.builder.CreateICmp(llvm::CmpInst::ICMP_SGE, index_val, llvm::ConstantInt::get(array_size_val->getType(), 0));
					return context.builder.CreateAnd(is_less_than, is_positive_or_zero);
				}
			}();
			emit_error_assert(is_in_bounds, subscript.index.src_tokens, "index value is out-of-bounds", context);
		}

		llvm::Value *result_ptr;
		if (array.kind == val_ptr::reference)
		{
			std::array<llvm::Value *, 2> indicies = { llvm::ConstantInt::get(context.get_uint64_t(), 0), index_val };
			result_ptr = context.builder.CreateGEP(array.val, llvm::ArrayRef(indicies));
		}
		else
		{
			auto const array_value = array.get_value(context.builder);
			auto const array_type = array_value->getType();
			auto const array_address = context.create_alloca(array_type);
			std::array<llvm::Value *, 2> indicies = { llvm::ConstantInt::get(context.get_uint64_t(), 0), index_val };
			result_ptr = context.builder.CreateGEP(array_address, llvm::ArrayRef(indicies));
		}

		if (result_address == nullptr)
		{
			return { val_ptr::reference, result_ptr };
		}
		else
		{
			auto const loaded_val = context.builder.CreateLoad(result_ptr);
			context.builder.CreateStore(loaded_val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
	else if (base_type.is<ast::ts_array_slice>())
	{
		auto const array = emit_bitcode<abi>(subscript.base, context, nullptr);
		auto const array_val = array.get_value(context.builder);
		auto const begin_ptr = context.builder.CreateExtractValue(array_val, 0);
		bz_assert(ast::remove_const_or_consteval(subscript.index.get_expr_type_and_kind().first).is<ast::ts_base_type>());
		auto const kind = ast::remove_const_or_consteval(subscript.index.get_expr_type_and_kind().first).get<ast::ts_base_type>().info->kind;
		auto index_val = emit_bitcode<abi>(subscript.index, context, nullptr).get_value(context.builder);
		if (ctx::is_unsigned_integer_kind(kind))
		{
			index_val = context.builder.CreateIntCast(index_val, context.get_uint64_t(), false);
		}

		// array bounds check
		{
			auto const end_ptr = context.builder.CreateExtractValue(array_val, 1);
			auto const array_size = context.builder.CreatePtrDiff(end_ptr, begin_ptr);
			auto const is_in_bounds = [&]() -> llvm::Value * {
				if (ctx::is_unsigned_integer_kind(kind))
				{
					return context.builder.CreateICmp(llvm::CmpInst::ICMP_ULT, index_val, array_size);
				}
				else
				{
					auto const is_less_than = context.builder.CreateICmp(llvm::CmpInst::ICMP_SLT, index_val, array_size);
					auto const is_positive_or_zero = context.builder.CreateICmp(llvm::CmpInst::ICMP_SGE, index_val, llvm::ConstantInt::get(array_size->getType(), 0));
					return context.builder.CreateAnd(is_less_than, is_positive_or_zero);
				}
			}();
			emit_error_assert(is_in_bounds, subscript.index.src_tokens, "index value is out-of-bounds", context);
		}

		auto const result_ptr = context.builder.CreateGEP(begin_ptr, index_val);

		if (result_address == nullptr)
		{
			return { val_ptr::reference, result_ptr };
		}
		else
		{
			auto const loaded_val = context.builder.CreateLoad(result_ptr);
			context.builder.CreateStore(loaded_val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
	else
	{
		bz_assert(base_type.is<ast::ts_tuple>());
		auto const tuple = emit_bitcode<abi>(subscript.base, context, nullptr);
		bz_assert(subscript.index.is<ast::constant_expression>());
		auto const &index_value = subscript.index.get<ast::constant_expression>().value;
		bz_assert(index_value.is<ast::constant_value::uint>() || index_value.is<ast::constant_value::sint>());
		auto const index_int_value = index_value.is<ast::constant_value::uint>()
			? index_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(index_value.get<ast::constant_value::sint>());

		llvm::Value *result_ptr;
		if (tuple.kind == val_ptr::reference)
		{
			result_ptr = context.builder.CreateStructGEP(tuple.val, index_int_value);
		}
		else
		{
			result_ptr = context.builder.CreateExtractValue(tuple.get_value(context.builder), index_int_value);
		}

		if (result_address == nullptr)
		{
			return { tuple.kind, result_ptr };
		}
		else
		{
			auto const loaded_val = context.builder.CreateLoad(result_ptr);
			context.builder.CreateStore(loaded_val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_cast const &cast,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const expr_t = ast::remove_const_or_consteval(cast.expr.get_expr_type_and_kind().first);
	auto &dest_t = cast.type;

	if (expr_t.is<ast::ts_base_type>() && dest_t.is<ast::ts_base_type>())
	{
		auto const llvm_dest_t = get_llvm_type(dest_t, context);
		auto const expr = emit_bitcode<abi>(cast.expr, context, nullptr).get_value(context.builder);
		auto const expr_kind = expr_t.get<ast::ts_base_type>().info->kind;
		auto const dest_kind = dest_t.get<ast::ts_base_type>().info->kind;

		if (ctx::is_integer_kind(expr_kind) && ctx::is_integer_kind(dest_kind))
		{
			auto const res = context.builder.CreateIntCast(
				expr,
				llvm_dest_t,
				ctx::is_signed_integer_kind(expr_kind),
				"cast_tmp"
			);
			if (result_address == nullptr)
			{
				return { val_ptr::value, res };
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else if (ctx::is_floating_point_kind(expr_kind) && ctx::is_floating_point_kind(dest_kind))
		{
			auto const res = context.builder.CreateFPCast(expr, llvm_dest_t, "cast_tmp");
			if (result_address == nullptr)
			{
				return { val_ptr::value, res };
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else if (ctx::is_floating_point_kind(expr_kind))
		{
			bz_assert(ctx::is_integer_kind(dest_kind));
			auto const res = ctx::is_signed_integer_kind(dest_kind)
				? context.builder.CreateFPToSI(expr, llvm_dest_t, "cast_tmp")
				: context.builder.CreateFPToUI(expr, llvm_dest_t, "cast_tmp");
			if (result_address == nullptr)
			{
				return { val_ptr::value, res };
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else if (ctx::is_integer_kind(expr_kind) && ctx::is_floating_point_kind(dest_kind))
		{
			auto const res = ctx::is_signed_integer_kind(dest_kind)
				? context.builder.CreateSIToFP(expr, llvm_dest_t, "cast_tmp")
				: context.builder.CreateUIToFP(expr, llvm_dest_t, "cast_tmp");
			if (result_address == nullptr)
			{
				return { val_ptr::value, res };
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return { val_ptr::reference, result_address };
			}
		}
		else
		{
			// this is a cast from i32 to i32 in IR, so we return the original value
			bz_assert((
				expr_kind == ast::type_info::char_
				&& (dest_kind == ast::type_info::uint32_ || dest_kind == ast::type_info::int32_)
			)
			|| (
				(expr_kind == ast::type_info::uint32_ || expr_kind == ast::type_info::int32_)
				&& dest_kind == ast::type_info::char_
			));
			if (result_address == nullptr)
			{
				return { val_ptr::value, expr };
			}
			else
			{
				context.builder.CreateStore(expr, result_address);
				return { val_ptr::reference, result_address };
			}
		}
	}
	else if (expr_t.is<ast::ts_pointer>() && dest_t.is<ast::ts_pointer>())
	{
		auto const llvm_dest_t = get_llvm_type(dest_t, context);
		auto const expr = emit_bitcode<abi>(cast.expr, context, nullptr).get_value(context.builder);
		auto const cast_result = context.builder.CreatePointerCast(expr, llvm_dest_t);
		if (result_address == nullptr)
		{
			return { val_ptr::value, cast_result };
		}
		else
		{
			context.builder.CreateStore(cast_result, result_address);
			return { val_ptr::reference, result_address };
		}
	}
	else if (expr_t.is<ast::ts_array>() && dest_t.is<ast::ts_array_slice>())
	{
		auto const expr_val = emit_bitcode<abi>(cast.expr, context, nullptr);
		auto const [begin_ptr, end_ptr] = [&]() {
			if (expr_val.kind == val_ptr::reference)
			{
				auto const begin_ptr = context.builder.CreateConstGEP2_64(expr_val.val, 0, 0);
				auto const end_ptr   = context.builder.CreateConstGEP2_64(expr_val.val, 0, expr_t.get<ast::ts_array>().size);
				return std::make_pair(begin_ptr, end_ptr);
			}
			else
			{
				auto const alloca = context.create_alloca(expr_val.get_type());
				context.builder.CreateStore(expr_val.get_value(context.builder), alloca);
				auto const begin_ptr = context.builder.CreateConstGEP2_64(alloca, 0, 0);
				auto const end_ptr   = context.builder.CreateConstGEP2_64(alloca, 0, expr_t.get<ast::ts_array>().size);
				return std::make_pair(begin_ptr, end_ptr);
			}
		}();
		if (result_address == nullptr)
		{
			bz_assert(begin_ptr->getType()->isPointerTy());
			auto const slice_t = get_llvm_type(dest_t, context);
			bz_assert(slice_t->isStructTy());
			auto const slice_struct_t = static_cast<llvm::StructType *>(slice_t);
			auto const slice_member_t = slice_struct_t->getElementType(0);
			llvm::Value *result = llvm::ConstantStruct::get(
				slice_struct_t,
				{ llvm::UndefValue::get(slice_member_t), llvm::UndefValue::get(slice_member_t) }
			);
			result = context.builder.CreateInsertValue(result, begin_ptr, 0);
			result = context.builder.CreateInsertValue(result, end_ptr,   1);
			return val_ptr{ val_ptr::value, result };
		}
		else
		{
			auto const result_begin_ptr = context.builder.CreateStructGEP(result_address, 0);
			auto const result_end_ptr   = context.builder.CreateStructGEP(result_address, 1);
			context.builder.CreateStore(begin_ptr, result_begin_ptr);
			context.builder.CreateStore(end_ptr, result_end_ptr);
			return val_ptr{ val_ptr::reference, result_address };
		}
	}
	else
	{
		bz_unreachable;
		return {};
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_take_reference const &take_ref,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const result = emit_bitcode<abi>(take_ref.expr, context, result_address);
	if (result.kind != val_ptr::reference)
	{
		if (auto const id_expr = take_ref.expr.get_expr().get_if<ast::expr_identifier>(); id_expr && id_expr->decl != nullptr)
		{
			emit_error(
				take_ref.expr.src_tokens,
				bz::format("unable to take reference to variable '{}'", id_expr->decl->id.format_as_unqualified()),
				context
			);
		}
		else
		{
			emit_error(take_ref.expr.src_tokens, "unable to take refernce to value", context);
		}
		// just make sure the returned value is valid
		bz_assert(result_address == nullptr);
		auto const alloca = context.create_alloca(result.get_type());
		return { val_ptr::reference, alloca };
	}
	else
	{
		return result;
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_struct_init const &struct_init,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const type = get_llvm_type(struct_init.type, context);
	auto const result_ptr = result_address != nullptr ? result_address : context.create_alloca(type);
	for (size_t i = 0; i < struct_init.exprs.size(); ++i)
	{
		auto const member_ptr = context.builder.CreateStructGEP(result_ptr, i);
		emit_bitcode<abi>(struct_init.exprs[i], context, member_ptr);
	}
	return { val_ptr::reference, result_ptr };
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_member_access const &member_access,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const base = emit_bitcode<abi>(member_access.base, context, nullptr);
	if (base.kind == val_ptr::reference)
	{
		auto const ptr = context.builder.CreateStructGEP(base.val, member_access.index);
		if (result_address == nullptr)
		{
			return { val_ptr::reference, ptr };
		}
		else
		{
			auto const val = context.builder.CreateLoad(ptr);
			context.builder.CreateStore(val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
	else
	{
		auto const val = context.builder.CreateExtractValue(
			base.get_value(context.builder), member_access.index
		);
		if (result_address == nullptr)
		{
			return { val_ptr::value, val };
		}
		else
		{
			context.builder.CreateStore(val, result_address);
			return { val_ptr::reference, result_address };
		}
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_compound const &compound_expr,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	context.push_expression_scope();
	for (auto &stmt : compound_expr.statements)
	{
		emit_bitcode<abi>(stmt, context);
	}
	if (compound_expr.final_expr.is_null())
	{
		context.pop_expression_scope();
		return {};
	}
	else
	{
		auto const result = emit_bitcode<abi>(compound_expr.final_expr, context, result_address);
		context.pop_expression_scope();
		return result;
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_if const &if_expr,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	context.push_expression_scope();
	auto const condition = emit_bitcode<abi>(if_expr.condition, context, nullptr).get_value(context.builder);
	context.pop_expression_scope();
	// assert that the condition is an i1 (bool)
	bz_assert(condition->getType()->isIntegerTy() && condition->getType()->getIntegerBitWidth() == 1);
	// the original block
	auto const entry_bb = context.builder.GetInsertBlock();

	// emit code for the then block
	auto const then_bb = context.add_basic_block("then");
	context.builder.SetInsertPoint(then_bb);
	auto const then_val = emit_bitcode<abi>(if_expr.then_block, context, result_address);
	auto const then_bb_end = context.builder.GetInsertBlock();

	// emit code for the else block if there's any
	auto const else_bb = if_expr.else_block.is_null() ? nullptr : context.add_basic_block("else");
	val_ptr else_val = {};
	if (else_bb)
	{
		context.builder.SetInsertPoint(else_bb);
		else_val = emit_bitcode<abi>(if_expr.else_block, context, result_address);
	}
	auto const else_bb_end = else_bb ? context.builder.GetInsertBlock() : nullptr;

	// if both branches have a return at the end, then don't create the end block
	if (else_bb_end && context.has_terminator(then_bb_end) && context.has_terminator(else_bb_end))
	{
		context.builder.SetInsertPoint(entry_bb);
		// else_bb must be valid here
		context.builder.CreateCondBr(condition, then_bb, else_bb);
		return {};
	}

	llvm::Value *then_val_value = nullptr;
	llvm::Value *else_val_value = nullptr;
	if (
		then_val.has_value() && else_val.has_value()
		&& (then_val.kind != val_ptr::reference || else_val.kind != val_ptr::reference)
	)
	{
		context.builder.SetInsertPoint(then_bb_end);
		then_val_value = then_val.get_value(context.builder);
		context.builder.SetInsertPoint(else_bb_end);
		else_val_value = else_val.get_value(context.builder);
	}

	auto const end_bb = context.add_basic_block("endif");
	// create branches for the entry block
	context.builder.SetInsertPoint(entry_bb);
	context.builder.CreateCondBr(condition, then_bb, else_bb ? else_bb : end_bb);

	// create branches for the then and else blocks, if there's no return at the end
	if (!context.has_terminator(then_bb_end))
	{
		context.builder.SetInsertPoint(then_bb_end);
		context.builder.CreateBr(end_bb);
	}
	if (else_bb_end && !context.has_terminator(else_bb_end))
	{
		context.builder.SetInsertPoint(else_bb_end);
		context.builder.CreateBr(end_bb);
	}

	context.builder.SetInsertPoint(end_bb);
	if (!then_val.has_value() || !else_val.has_value())
	{
		return {};
	}

	if (result_address != nullptr)
	{
		return { val_ptr::reference, result_address };
	}
	else if (then_val.kind == val_ptr::reference && else_val.kind == val_ptr::reference)
	{
		auto const result = context.builder.CreatePHI(then_val.val->getType(), 2);
		bz_assert(then_val.val != nullptr);
		bz_assert(else_val.val != nullptr);
		result->addIncoming(then_val.val, then_bb_end);
		result->addIncoming(else_val.val, else_bb_end);
		return { val_ptr::reference, result };
	}
	else
	{
		bz_assert(then_val_value != nullptr && else_val_value != nullptr);
		auto const result = context.builder.CreatePHI(then_val_value->getType(), 2);
		result->addIncoming(then_val_value, then_bb_end);
		result->addIncoming(else_val_value, else_bb_end);
		return { val_ptr::value, result };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_switch const &switch_expr,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const matched_value = emit_bitcode<abi>(switch_expr.matched_expr, context, nullptr).get_value(context.builder);
	bz_assert(matched_value->getType()->isIntegerTy());
	auto const default_bb = context.add_basic_block("switch_else");
	auto const has_default = switch_expr.default_case.not_null();
	bz_assert(result_address == nullptr || has_default);

	auto const case_count = switch_expr.cases.transform([](auto const &switch_case) { return switch_case.values.size(); }).sum();

	auto const switch_inst = context.builder.CreateSwitch(matched_value, default_bb, static_cast<unsigned>(case_count));
	ast::arena_vector<std::pair<llvm::BasicBlock *, val_ptr>> case_result_vals;
	case_result_vals.reserve(switch_expr.cases.size() + 1);
	if (has_default)
	{
		context.builder.SetInsertPoint(default_bb);
		auto const default_val = emit_bitcode<abi>(switch_expr.default_case, context, result_address);
		case_result_vals.push_back({ context.builder.GetInsertBlock(), default_val });
	}
	for (auto const &[case_vals, case_expr] : switch_expr.cases)
	{
		auto const bb = context.add_basic_block("case");
		for (auto const &expr : case_vals)
		{
			bz_assert(expr.is<ast::constant_expression>());
			auto const val = emit_bitcode<abi>(expr, context, nullptr).get_value(context.builder);
			bz_assert(llvm::dyn_cast<llvm::ConstantInt>(val) != nullptr);
			auto const const_int_val = static_cast<llvm::ConstantInt *>(val);
			switch_inst->addCase(const_int_val, bb);
		}
		context.builder.SetInsertPoint(bb);
		auto const case_val = emit_bitcode<abi>(case_expr, context, result_address);
		case_result_vals.push_back({ context.builder.GetInsertBlock(), case_val });
	}
	auto const end_bb = has_default ? context.add_basic_block("switch_end") : default_bb;
	auto const has_value = case_result_vals.is_any([](auto const &pair) {
		return pair.second.val != nullptr || pair.second.consteval_val != nullptr;
	});
	if (result_address == nullptr && has_default && has_value)
	{
		auto const is_all_ref = case_result_vals.is_all([&](auto const &pair) {
			return context.has_terminator(pair.first) || (pair.second.val != nullptr && pair.second.kind == val_ptr::reference);
		});
		context.builder.SetInsertPoint(end_bb);
		auto const phi_type = is_all_ref
			? case_result_vals.filter([](auto const &pair) { return pair.second.val != nullptr; }).front().second.val->getType()
			: case_result_vals.filter([](auto const &pair) { return pair.second.val != nullptr; }).front().second.get_type();
		auto const phi = context.builder.CreatePHI(phi_type, case_result_vals.size());
		if (is_all_ref)
		{
			for (auto const &[bb, val] : case_result_vals)
			{
				if (context.has_terminator(bb))
				{
					continue;
				}
				context.builder.SetInsertPoint(bb);
				context.builder.CreateBr(end_bb);
				phi->addIncoming(val.val, bb);
			}
		}
		else
		{
			for (auto const &[bb, val] : case_result_vals)
			{
				if (context.has_terminator(bb))
				{
					continue;
				}
				context.builder.SetInsertPoint(bb);
				phi->addIncoming(val.get_value(context.builder), bb);
				context.builder.CreateBr(end_bb);
				bz_assert(context.builder.GetInsertBlock() == bb);
			}
		}
		context.builder.SetInsertPoint(end_bb);
		return is_all_ref
			? val_ptr{ val_ptr::reference, phi }
			: val_ptr{ val_ptr::value, phi };
	}
	else
	{
		for (auto const &[bb, _] : case_result_vals)
		{
			if (context.has_terminator(bb))
			{
				continue;
			}
			context.builder.SetInsertPoint(bb);
			context.builder.CreateBr(end_bb);
		}
		context.builder.SetInsertPoint(end_bb);
		if (result_address != nullptr)
		{
			return { val_ptr::reference, result_address };
		}
		else
		{
			return {};
		}
	}
}

template<abi::platform_abi abi>
static llvm::Constant *get_value(
	ast::constant_value const &value,
	ast::typespec_view type,
	ast::constant_expression const *const_expr,
	ctx::comptime_executor_context &context
)
{
	switch (value.kind())
	{
	case ast::constant_value::sint:
		bz_assert(!type.is_empty());
		return llvm::ConstantInt::get(
			get_llvm_type(type, context),
			bit_cast<uint64_t>(value.get<ast::constant_value::sint>()),
			true
		);
	case ast::constant_value::uint:
		bz_assert(!type.is_empty());
		return llvm::ConstantInt::get(
			get_llvm_type(type, context),
			value.get<ast::constant_value::uint>(),
			false
		);
	case ast::constant_value::float32:
		return llvm::ConstantFP::get(
			context.get_float32_t(),
			static_cast<double>(value.get<ast::constant_value::float32>())
		);
	case ast::constant_value::float64:
		return llvm::ConstantFP::get(
			context.get_float64_t(),
			value.get<ast::constant_value::float64>()
		);
	case ast::constant_value::u8char:
		return llvm::ConstantInt::get(
			context.get_char_t(),
			value.get<ast::constant_value::u8char>()
		);
	case ast::constant_value::string:
	{
		auto const str = value.get<ast::constant_value::string>().as_string_view();
		auto const str_t = llvm::dyn_cast<llvm::StructType>(context.get_str_t());
		bz_assert(str_t != nullptr);

		// if the string is empty, we make a zero initialized string, so
		// structs with a default value of "" get to be zero initialized
		if (str == "")
		{
			return llvm::ConstantStruct::getNullValue(str_t);
		}

		auto const string_constant = context.create_string(str);

		auto const begin_ptr = context.builder.CreateConstGEP2_64(string_constant, 0, 0);
		auto const const_begin_ptr = llvm::dyn_cast<llvm::Constant>(begin_ptr);
		bz_assert(const_begin_ptr != nullptr);

		auto const end_ptr = context.builder.CreateConstGEP2_64(string_constant, 0, str.length());
		auto const const_end_ptr = llvm::dyn_cast<llvm::Constant>(end_ptr);
		bz_assert(const_end_ptr != nullptr);
		llvm::Constant *elems[] = { const_begin_ptr, const_end_ptr };

		return llvm::ConstantStruct::get(str_t, elems);
	}
	case ast::constant_value::boolean:
		return llvm::ConstantInt::get(
			context.get_bool_t(),
			static_cast<uint64_t>(value.get<ast::constant_value::boolean>())
		);
	case ast::constant_value::null:
		if (ast::remove_const_or_consteval(type).is<ast::ts_pointer>())
		{
			auto const ptr_t = llvm::dyn_cast<llvm::PointerType>(get_llvm_type(type, context));
			bz_assert(ptr_t != nullptr);
			return llvm::ConstantPointerNull::get(ptr_t);
		}
		else
		{
			return llvm::ConstantStruct::get(
				llvm::dyn_cast<llvm::StructType>(context.get_null_t())
			);
		}
	case ast::constant_value::void_:
		return nullptr;
	case ast::constant_value::array:
	{
		bz_assert(ast::remove_const_or_consteval(type).is<ast::ts_array>());
		auto const elem_type = ast::remove_const_or_consteval(type)
			.get<ast::ts_array>().elem_type.as_typespec_view();
		auto const array_type = llvm::dyn_cast<llvm::ArrayType>(get_llvm_type(type, context));
		bz_assert(array_type != nullptr);
		auto const &array_values = value.get<ast::constant_value::array>();
		ast::arena_vector<llvm::Constant *> elems = {};
		elems.reserve(array_values.size());
		for (auto const &val : array_values)
		{
			elems.push_back(get_value<abi>(val, elem_type, nullptr, context));
		}
		return llvm::ConstantArray::get(array_type, llvm::ArrayRef(elems.data(), elems.size()));
	}
	case ast::constant_value::tuple:
	{
		auto const &tuple_values = value.get<ast::constant_value::tuple>();
		ast::arena_vector<llvm::Type     *> types = {};
		ast::arena_vector<llvm::Constant *> elems = {};
		types.reserve(tuple_values.size());
		elems.reserve(tuple_values.size());
		if (const_expr != nullptr && const_expr->expr.is<ast::expr_tuple>())
		{
			auto &tuple = const_expr->expr.get<ast::expr_tuple>();
			for (auto const &elem : tuple.elems)
			{
				bz_assert(elem.is<ast::constant_expression>());
				auto const &const_elem = elem.get<ast::constant_expression>();
				elems.push_back(get_value<abi>(const_elem.value, const_elem.type, &const_elem, context));
				types.push_back(elems.back()->getType());
			}
		}
		else
		{
			bz_assert(ast::remove_const_or_consteval(type).is<ast::ts_tuple>());
			auto const &tuple_t = ast::remove_const_or_consteval(type).get<ast::ts_tuple>();
			for (auto const &[val, t] : bz::zip(tuple_values, tuple_t.types))
			{
				elems.push_back(get_value<abi>(val, t, nullptr, context));
				types.push_back(elems.back()->getType());
			}
		}
		auto const tuple_type = context.get_tuple_t(types);
		return llvm::ConstantStruct::get(tuple_type, llvm::ArrayRef(elems.data(), elems.size()));
	}
	case ast::constant_value::function:
	{
		auto const decl = value.get<ast::constant_value::function>();
		return context.get_function(decl);
	}
	case ast::constant_value::aggregate:
	{
		auto const &aggregate = value.get<ast::constant_value::aggregate>();
		bz_assert(ast::remove_const_or_consteval(type).is<ast::ts_base_type>());
		auto const info = ast::remove_const_or_consteval(type).get<ast::ts_base_type>().info;
		auto const val_type = get_llvm_type(type, context);
		bz_assert(val_type->isStructTy());
		auto const val_struct_type = static_cast<llvm::StructType *>(val_type);
		auto const members = bz::zip(aggregate, info->member_variables)
			.transform([&context](auto const pair) {
				return get_value<abi>(pair.first, pair.second.type, nullptr, context);
			})
			.collect();
		return llvm::ConstantStruct::get(val_struct_type, llvm::ArrayRef(members.data(), members.size()));
	}
	case ast::constant_value::unqualified_function_set_id:
	case ast::constant_value::qualified_function_set_id:
		bz_unreachable;
	case ast::constant_value::type:
		bz_unreachable;
	default:
		bz_unreachable;
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	lex::src_tokens src_tokens,
	ast::constant_expression const &const_expr,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	if (
		const_expr.kind == ast::expression_type_kind::type_name
		|| const_expr.kind == ast::expression_type_kind::none
	)
	{
		return {};
	}

	auto const needs_destructor = result_address == nullptr
		&& const_expr.kind == ast::expression_type_kind::rvalue
		&& ast::needs_destructor(const_expr.type);
	if (needs_destructor)
	{
		auto const result_type = get_llvm_type(const_expr.type, context);
		result_address = context.create_alloca(result_type);
		push_destructor_call(src_tokens, result_address, const_expr.type, context);
	}
	val_ptr result;

	// consteval variable
	if (const_expr.kind == ast::expression_type_kind::lvalue)
	{
		result = const_expr.expr.visit([&](auto const &expr) {
			return emit_bitcode<abi>(expr, context, nullptr);
		});
	}
	else
	{
		result.kind = val_ptr::value;
	}

	result.consteval_val = get_value<abi>(const_expr.value, const_expr.type, &const_expr, context);

	if (result_address == nullptr)
	{
		return result;
	}
	else
	{
		auto const result_val = result.get_value(context.builder);
		context.builder.CreateStore(result_val, result_address);
		return { val_ptr::reference, result_address };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	lex::src_tokens src_tokens,
	ast::dynamic_expression const &dyn_expr,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const needs_destructor = result_address == nullptr
		&& dyn_expr.kind == ast::expression_type_kind::rvalue
		&& ast::needs_destructor(dyn_expr.type);
	if (needs_destructor)
	{
		auto const result_type = get_llvm_type(dyn_expr.type, context);
		result_address = context.create_alloca(result_type);
		push_destructor_call(src_tokens, result_address, dyn_expr.type, context);
	}
	return dyn_expr.expr.visit([&](auto const &expr) {
		return emit_bitcode<abi>(expr, context, result_address);
	});
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expression const &expr,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	switch (expr.kind())
	{
	case ast::expression::index_of<ast::constant_expression>:
		return emit_bitcode<abi>(expr.src_tokens, expr.get<ast::constant_expression>(), context, result_address);
	case ast::expression::index_of<ast::dynamic_expression>:
		return emit_bitcode<abi>(expr.src_tokens, expr.get<ast::dynamic_expression>(), context, result_address);
	case ast::expression::index_of<ast::error_expression>:
		emit_error(expr.src_tokens, "failed to resolve expression", context);
		return {};

	default:
		emit_error(expr.src_tokens, "failed to resolve expression", context);
		// we can safely return {} here, because errors should have been propagated enough while parsing for this to not matter
		return {};
	}
}

// ================================================================
// -------------------------- statement ---------------------------
// ================================================================

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::statement const &stmt,
	ctx::comptime_executor_context &context
);
/*
static void emit_bitcode(
	ast::stmt_if const &if_stmt,
	ctx::comptime_executor_context &context
)
{
	auto const condition = emit_bitcode(if_stmt.condition, context).get_value(context.builder);
	// assert that the condition is an i1 (bool)
	bz_assert(condition->getType()->isIntegerTy() && condition->getType()->getIntegerBitWidth() == 1);
	// the original block
	auto const entry_bb = context.builder.GetInsertBlock();

	// emit code for the then block
	auto const then_bb = context.add_basic_block("then");
	context.builder.SetInsertPoint(then_bb);
	emit_bitcode(if_stmt.then_block, context);
	auto const then_bb_end = context.builder.GetInsertBlock();

	// emit code for the else block if there's any
	auto const else_bb = if_stmt.else_block.has_value() ? context.add_basic_block("else") : nullptr;
	if (else_bb)
	{
		context.builder.SetInsertPoint(else_bb);
		emit_bitcode(*if_stmt.else_block, context);
	}
	auto const else_bb_end = else_bb ? context.builder.GetInsertBlock() : nullptr;

	// if both branches have a return at the end, then don't create the end block
	if (else_bb_end && context.has_terminator(then_bb_end) && context.has_terminator(else_bb_end))
	{
		context.builder.SetInsertPoint(entry_bb);
		// else_bb must be valid here
		context.builder.CreateCondBr(condition, then_bb, else_bb);
		return;
	}

	auto const end_bb = context.add_basic_block("endif");
	// create branches for the entry block
	context.builder.SetInsertPoint(entry_bb);
	context.builder.CreateCondBr(condition, then_bb, else_bb ? else_bb : end_bb);

	// create branches for the then and else blocks, if there's no return at the end
	if (!context.has_terminator(then_bb_end))
	{
		context.builder.SetInsertPoint(then_bb_end);
		context.builder.CreateBr(end_bb);
	}
	if (else_bb_end && !context.has_terminator(else_bb_end))
	{
		context.builder.SetInsertPoint(else_bb_end);
		context.builder.CreateBr(end_bb);
	}

	context.builder.SetInsertPoint(end_bb);
}
*/
template<abi::platform_abi abi>
static void emit_bitcode(
	ast::stmt_while const &while_stmt,
	ctx::comptime_executor_context &context
)
{
	auto const condition_check = context.add_basic_block("while_condition_check");
	context.builder.CreateBr(condition_check);
	context.builder.SetInsertPoint(condition_check);
	context.push_expression_scope();
	auto const condition = emit_bitcode<abi>(while_stmt.condition, context, nullptr).get_value(context.builder);
	context.pop_expression_scope();
	auto const condition_check_end = context.builder.GetInsertBlock();

	auto const while_bb = context.add_basic_block("while");
	context.builder.SetInsertPoint(while_bb);
	context.push_expression_scope();
	emit_bitcode<abi>(while_stmt.while_block, context);
	context.pop_expression_scope();
	if (!context.has_terminator())
	{
		context.builder.CreateBr(condition_check);
	}

	auto const end_bb = context.add_basic_block("endwhile");
	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition, while_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
}

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::stmt_for const &for_stmt,
	ctx::comptime_executor_context &context
)
{
	context.push_expression_scope();
	if (for_stmt.init.not_null())
	{
		emit_bitcode<abi>(for_stmt.init, context);
	}
	auto const condition_check = context.add_basic_block("for_condition_check");
	context.builder.CreateBr(condition_check);
	context.builder.SetInsertPoint(condition_check);
	context.push_expression_scope();
	auto const condition = for_stmt.condition.not_null()
		? emit_bitcode<abi>(for_stmt.condition, context, nullptr).get_value(context.builder)
		: llvm::ConstantInt::getTrue(context.get_llvm_context());
	context.pop_expression_scope();
	auto const condition_check_end = context.builder.GetInsertBlock();

	auto const for_bb = context.add_basic_block("for");
	context.builder.SetInsertPoint(for_bb);
	context.push_expression_scope();
	emit_bitcode<abi>(for_stmt.for_block, context);
	context.pop_expression_scope();
	if (!context.has_terminator())
	{
		if (for_stmt.iteration.not_null())
		{
			context.push_expression_scope();
			emit_bitcode<abi>(for_stmt.iteration, context, nullptr);
			context.pop_expression_scope();
		}
		context.builder.CreateBr(condition_check);
	}

	auto const end_bb = context.add_basic_block("endfor");
	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition, for_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
	context.pop_expression_scope();
}

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::stmt_foreach const &foreach_stmt,
	ctx::comptime_executor_context &context
)
{
	context.push_expression_scope();
	emit_bitcode<abi>(foreach_stmt.range_var_decl, context);
	emit_bitcode<abi>(foreach_stmt.iter_var_decl, context);
	emit_bitcode<abi>(foreach_stmt.end_var_decl, context);

	auto const condition_check = context.add_basic_block("foreach_condition_check");
	context.builder.CreateBr(condition_check);
	context.builder.SetInsertPoint(condition_check);
	auto const condition = emit_bitcode<abi>(foreach_stmt.condition, context, nullptr).get_value(context.builder);
	auto const condition_check_end = context.builder.GetInsertBlock();

	auto const foreach_bb = context.add_basic_block("foreach");
	context.builder.SetInsertPoint(foreach_bb);
	context.push_expression_scope();
	emit_bitcode<abi>(foreach_stmt.iter_deref_var_decl, context);
	context.push_expression_scope();
	emit_bitcode<abi>(foreach_stmt.for_block, context);
	context.pop_expression_scope();
	if (!context.has_terminator())
	{
		emit_bitcode<abi>(foreach_stmt.iteration, context, nullptr);
		context.builder.CreateBr(condition_check);
	}
	context.pop_expression_scope();

	auto const end_bb = context.add_basic_block("endforeach");
	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition, foreach_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
	context.pop_expression_scope();
}

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::stmt_return const &ret_stmt,
	ctx::comptime_executor_context &context
)
{
	if (context.current_function.first == nullptr)
	{
		// we are in a comptime compound expression here
		emit_error(
			ret_stmt.expr.src_tokens,
			"return statement is not allowed in compile time evaluation of compound expression",
			context
		);
		return;
	}

	if (ret_stmt.expr.is_null())
	{
		context.emit_destructor_calls();
		if (context.current_function.first->is_main())
		{
			context.builder.CreateRet(llvm::ConstantInt::get(context.get_int32_t(), 0));
		}
		else
		{
			context.builder.CreateRetVoid();
		}
	}
	else if (ret_stmt.expr.is_error())
	{
		emit_error(ret_stmt.expr.src_tokens, "failed to evaluate expression", context);
	}
	else
	{
		if (context.current_function.first->return_type.is<ast::ts_lvalue_reference>())
		{
			auto const ret_val = emit_bitcode<abi>(ret_stmt.expr, context, context.output_pointer);
			context.emit_destructor_calls();
			bz_assert(ret_val.kind == val_ptr::reference);
			context.builder.CreateRet(ret_val.val);
		}
		else if (context.output_pointer != nullptr)
		{
			auto const ret_val = emit_bitcode<abi>(ret_stmt.expr, context, context.output_pointer);
			context.emit_destructor_calls();
			bz_assert(ret_val.val == context.output_pointer);
			bz_assert(ret_val.kind == val_ptr::reference);
			context.builder.CreateRetVoid();
		}
		else
		{
			auto const result_type = get_llvm_type(context.current_function.first->return_type, context);
			auto const ret_kind = abi::get_pass_kind<abi>(result_type, context.get_data_layout(), context.get_llvm_context());
			switch (ret_kind)
			{
			case abi::pass_kind::reference:
				bz_unreachable;
			case abi::pass_kind::value:
			{
				auto const ret_val = emit_bitcode<abi>(ret_stmt.expr, context, context.output_pointer);
				context.emit_destructor_calls();
				context.builder.CreateRet(ret_val.get_value(context.builder));
				break;
			}
			case abi::pass_kind::one_register:
			case abi::pass_kind::two_registers:
			{
				auto const ret_type = context.current_function.second->getReturnType();
				auto const alloca = context.create_alloca(result_type);
				auto const result_ptr = context.builder.CreatePointerCast(alloca, llvm::PointerType::get(ret_type, 0));
				emit_bitcode<abi>(ret_stmt.expr, context, alloca);
				auto const result = context.builder.CreateLoad(result_ptr);
				context.emit_destructor_calls();
				context.builder.CreateRet(result);
				break;
			}
			}
		}
	}
}

template<abi::platform_abi abi>
static void emit_bitcode(
	[[maybe_unused]] ast::stmt_no_op const & no_op_stmt,
	[[maybe_unused]] ctx::comptime_executor_context &context
)
{
	// we do nothing
	return;
}

/*
static void emit_bitcode(
	ast::stmt_compound const &comp_stmt,
	ctx::comptime_executor_context &context
)
{
	for (auto &stmt : comp_stmt.statements)
	{
		emit_bitcode(stmt, context);
	}
}
*/

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::stmt_expression const &expr_stmt,
	ctx::comptime_executor_context &context
)
{
	context.push_expression_scope();
	emit_bitcode<abi>(expr_stmt.expr, context, nullptr);
	context.pop_expression_scope();
}

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::decl_variable const &var_decl,
	ctx::comptime_executor_context &context
)
{
	if (var_decl.var_type.is<ast::ts_lvalue_reference>())
	{
		bz_assert(var_decl.init_expr.not_null());
		auto const init_val = emit_bitcode<abi>(var_decl.init_expr, context, nullptr);
		bz_assert(init_val.kind == val_ptr::reference);
		context.add_variable(&var_decl, init_val.val);
	}
	else
	{
		auto const type = get_llvm_type(var_decl.var_type, context);
		auto const alloca = context.create_alloca(type);
		push_destructor_call(var_decl.src_tokens, alloca, var_decl.var_type, context);
		if (var_decl.init_expr.not_null())
		{
			context.push_expression_scope();
			emit_bitcode<abi>(var_decl.init_expr, context, alloca);
			context.pop_expression_scope();
		}
		else
		{
			auto const init = get_constant_zero(var_decl.var_type, type, context);
			context.builder.CreateStore(init, alloca);
		}
		context.add_variable(&var_decl, alloca);
	}
}


template<abi::platform_abi abi>
static void emit_bitcode(
	ast::statement const &stmt,
	ctx::comptime_executor_context &context
)
{
	if (context.has_terminator())
	{
		return;
	}

	switch (stmt.kind())
	{
	case ast::statement::index<ast::stmt_while>:
		emit_bitcode<abi>(stmt.get<ast::stmt_while>(), context);
		break;
	case ast::statement::index<ast::stmt_for>:
		emit_bitcode<abi>(stmt.get<ast::stmt_for>(), context);
		break;
	case ast::statement::index<ast::stmt_foreach>:
		emit_bitcode<abi>(stmt.get<ast::stmt_foreach>(), context);
		break;
	case ast::statement::index<ast::stmt_return>:
		emit_bitcode<abi>(stmt.get<ast::stmt_return>(), context);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		emit_bitcode<abi>(stmt.get<ast::stmt_no_op>(), context);
		break;
	case ast::statement::index<ast::stmt_expression>:
		emit_bitcode<abi>(stmt.get<ast::stmt_expression>(), context);
		break;
	case ast::statement::index<ast::stmt_static_assert>:
		// nothing
		break;

	case ast::statement::index<ast::decl_variable>:
		emit_bitcode<abi>(stmt.get<ast::decl_variable>(), context);
		break;

	case ast::statement::index<ast::decl_function>:
	case ast::statement::index<ast::decl_operator>:
	case ast::statement::index<ast::decl_struct>:
	case ast::statement::index<ast::decl_import>:
	case ast::statement::index<ast::decl_type_alias>:
		break;
	default:
		bz_unreachable;
		break;
	}
}

template<abi::platform_abi abi>
static llvm::Function *create_function_from_symbol_impl(
	ast::function_body &func_body,
	ctx::comptime_executor_context &context
)
{
	auto const result_t = get_llvm_type(func_body.return_type, context);
	auto const return_kind = abi::get_pass_kind<abi>(result_t, context.get_data_layout(), context.get_llvm_context());

	bz::vector<bool> pass_arg_by_ref = {};
	bz::vector<llvm::Type *> args = {};
	pass_arg_by_ref.reserve(func_body.params.size());
	args.reserve(func_body.params.size() + (return_kind == abi::pass_kind::reference ? 1 : 0));

	if (return_kind == abi::pass_kind::reference)
	{
		args.push_back(llvm::PointerType::get(result_t, 0));
	}
	if (func_body.is_main())
	{
		auto const str_slice = context.get_slice_t(context.get_str_t());
		auto const pass_kind = abi::get_pass_kind<abi>(str_slice, context.get_data_layout(), context.get_llvm_context());

		switch (pass_kind)
		{
		case abi::pass_kind::reference:
			pass_arg_by_ref.push_back(true);
			args.push_back(llvm::PointerType::get(str_slice, 0));
			break;
		case abi::pass_kind::value:
			pass_arg_by_ref.push_back(false);
			args.push_back(str_slice);
			break;
		case abi::pass_kind::one_register:
			pass_arg_by_ref.push_back(false);
			args.push_back(abi::get_one_register_type<abi>(str_slice, context.get_data_layout(), context.get_llvm_context()));
			break;
		case abi::pass_kind::two_registers:
		{
			auto const [first_type, second_type] = abi::get_two_register_types<abi>(str_slice, context.get_data_layout(), context.get_llvm_context());
			pass_arg_by_ref.push_back(false);
			args.push_back(first_type);
			pass_arg_by_ref.push_back(false);
			args.push_back(second_type);
			break;
		}
		}
	}
	else
	{
		for (auto &p : func_body.params)
		{
			if (p.var_type.is_typename())
			{
				// skip typename args
				continue;
			}
			auto const t = get_llvm_type(p.var_type, context);
			auto const pass_kind = abi::get_pass_kind<abi>(t, context.get_data_layout(), context.get_llvm_context());

			switch (pass_kind)
			{
			case abi::pass_kind::reference:
				pass_arg_by_ref.push_back(true);
				args.push_back(llvm::PointerType::get(t, 0));
				break;
			case abi::pass_kind::value:
				pass_arg_by_ref.push_back(false);
				args.push_back(t);
				break;
			case abi::pass_kind::one_register:
				pass_arg_by_ref.push_back(false);
				args.push_back(abi::get_one_register_type<abi>(t, context.get_data_layout(), context.get_llvm_context()));
				break;
			case abi::pass_kind::two_registers:
			{
				auto const [first_type, second_type] = abi::get_two_register_types<abi>(t, context.get_data_layout(), context.get_llvm_context());
				pass_arg_by_ref.push_back(false);
				args.push_back(first_type);
				pass_arg_by_ref.push_back(false);
				args.push_back(second_type);
				break;
			}
			}
		}
	}
	auto const func_t = [&]() {
		llvm::Type *real_result_t;
		if (func_body.is_main())
		{
			real_result_t = context.get_int32_t();
		}
		else
		{
			switch (return_kind)
			{
			case abi::pass_kind::reference:
				real_result_t = llvm::Type::getVoidTy(context.get_llvm_context());
				break;
			case abi::pass_kind::value:
				real_result_t = result_t;
				break;
			case abi::pass_kind::one_register:
				real_result_t = abi::get_one_register_type<abi>(result_t, context.get_data_layout(), context.get_llvm_context());
				break;
			case abi::pass_kind::two_registers:
			{
				auto const [first_type, second_type] = abi::get_two_register_types<abi>(result_t, context.get_data_layout(), context.get_llvm_context());
				real_result_t = llvm::StructType::get(first_type, second_type);
				break;
			}
			}
		}
		return llvm::FunctionType::get(real_result_t, llvm::ArrayRef(args.data(), args.size()), false);
	}();

	// bz_assert(func_body.symbol_name != "");
	auto const name_string = [&]() -> bz::u8string {
		if (func_body.symbol_name != "")
		{
			return func_body.symbol_name;
		}
		else if (func_body.function_name_or_operator_kind.is<ast::identifier>())
		{
			return func_body.function_name_or_operator_kind.get<ast::identifier>().as_string();
		}
		else
		{
			return bz::format("operator.{}", func_body.function_name_or_operator_kind.get<uint32_t>());
		}
	}();
	auto const name = llvm::StringRef(name_string.data_as_char_ptr(), name_string.size());

	auto const linkage = func_body.is_external_linkage()
		? llvm::Function::ExternalLinkage
		: llvm::Function::InternalLinkage;

	auto const fn = llvm::Function::Create(
		func_t, linkage,
		name, context.get_module()
	);

	switch (func_body.cc)
	{
	case abi::calling_convention::bozon:
		fn->setCallingConv(llvm::CallingConv::C);
		break;
	case abi::calling_convention::c:
		fn->setCallingConv(llvm::CallingConv::C);
		break;
	case abi::calling_convention::fast:
		fn->setCallingConv(llvm::CallingConv::Fast);
		break;
	case abi::calling_convention::std:
		fn->setCallingConv(llvm::CallingConv::X86_StdCall);
		break;
	}

	auto is_by_ref_it = pass_arg_by_ref.begin();
	auto is_by_ref_end = pass_arg_by_ref.end();
	auto arg_it = fn->arg_begin();

	if (return_kind == abi::pass_kind::reference)
	{
		arg_it->addAttr(llvm::Attribute::StructRet);
		arg_it->addAttr(llvm::Attribute::NoAlias);
		arg_it->addAttr(llvm::Attribute::NoCapture);
		arg_it->addAttr(llvm::Attribute::NonNull);
		++arg_it;
	}

	for (; is_by_ref_it != is_by_ref_end; ++is_by_ref_it, ++arg_it)
	{
		auto &arg = *arg_it;
		auto const is_by_ref = *is_by_ref_it;
		if (is_by_ref)
		{
			arg.addAttr(llvm::Attribute::ByVal);
			arg.addAttr(llvm::Attribute::NoAlias);
			arg.addAttr(llvm::Attribute::NoCapture);
			arg.addAttr(llvm::Attribute::NonNull);
		}
	}
	return fn;
}

static llvm::Function *create_function_from_symbol(
	ast::function_body &func_body,
	ctx::comptime_executor_context &context
)
{
	auto const abi = context.get_platform_abi();
	switch (abi)
	{
	case abi::platform_abi::generic:
		return create_function_from_symbol_impl<abi::platform_abi::generic>(
			func_body, context
		);
	case abi::platform_abi::microsoft_x64:
		return create_function_from_symbol_impl<abi::platform_abi::microsoft_x64>(
			func_body, context
		);
	case abi::platform_abi::systemv_amd64:
		return create_function_from_symbol_impl<abi::platform_abi::systemv_amd64>(
			func_body, context
		);
	}
	bz_unreachable;
}

llvm::Function *add_function_to_module(
	ast::function_body *func_body,
	ctx::comptime_executor_context &context
)
{
	auto const fn = create_function_from_symbol(*func_body, context);
	context.funcs_.insert({ func_body, fn });
	return fn;
}

template<abi::platform_abi abi>
static void emit_function_bitcode_impl(
	ast::function_body &func_body,
	ctx::comptime_executor_context &context
)
{
	auto const fn = context.get_function(&func_body);
	bz_assert(fn != nullptr);
	if (fn->size() != 0)
	{
		return;
	}

	context.current_function = { &func_body, fn };

	auto const alloca_bb = context.add_basic_block("alloca");
	auto const error_bb = context.add_basic_block("error");
	context.alloca_bb = alloca_bb;
	context.error_bb = error_bb;

	context.builder.SetInsertPoint(error_bb);
	auto const fn_return_type = fn->getReturnType();
	if (fn_return_type->isVoidTy())
	{
		context.builder.CreateRetVoid();
	}
	else
	{
		auto const return_val = llvm::UndefValue::get(fn_return_type);
		context.builder.CreateRet(return_val);
	}

	auto const entry_bb = context.add_basic_block("entry");
	context.builder.SetInsertPoint(entry_bb);

	bz_assert(func_body.body.is<bz::vector<ast::statement>>());
	bz::vector<llvm::Value *> params = {};
	params.reserve(func_body.params.size());

	// initialization of function parameters
	{
		auto p_it = func_body.params.begin();
		auto const p_end = func_body.params.end();
		auto fn_it = fn->arg_begin();
		auto const fn_end = fn->arg_end();

		if (fn_it != fn_end && fn_it->hasAttribute(llvm::Attribute::StructRet))
		{
			context.output_pointer = fn_it;
			++fn_it;
		}

		while (p_it != p_end)
		{
			auto &p = *p_it;
			if (p.var_type.is_typename())
			{
				++p_it;
				continue;
			}
			if (!p.var_type.is<ast::ts_lvalue_reference>() && !fn_it->hasAttribute(llvm::Attribute::ByVal))
			{
				auto const t = get_llvm_type(p.var_type, context);
				auto const pass_kind = abi::get_pass_kind<abi>(t, context.get_data_layout(), context.get_llvm_context());
				switch (pass_kind)
				{
				case abi::pass_kind::reference:
					context.add_variable(&p, fn_it);
					break;
				case abi::pass_kind::value:
				{
					auto const alloca = context.create_alloca(t);
					context.builder.CreateStore(fn_it, alloca);
					context.add_variable(&p, alloca);
					break;
				}
				case abi::pass_kind::one_register:
				{
					auto const alloca = context.create_alloca(t);
					auto const alloca_cast = context.builder.CreatePointerCast(alloca, llvm::PointerType::get(fn_it->getType(), 0));
					context.builder.CreateStore(fn_it, alloca_cast);
					context.add_variable(&p, alloca);
					break;
				}
				case abi::pass_kind::two_registers:
				{
					auto const alloca = context.create_alloca(t);
					auto const first_val = fn_it;
					auto const first_type = fn_it->getType();
					++fn_it;
					auto const second_val = fn_it;
					auto const second_type = fn_it->getType();
					auto const alloca_cast = context.builder.CreatePointerCast(
						alloca,
						llvm::PointerType::get(llvm::StructType::get(first_type, second_type), 0)
					);
					auto const first_address  = context.builder.CreateStructGEP(alloca_cast, 0);
					auto const second_address = context.builder.CreateStructGEP(alloca_cast, 1);
					context.builder.CreateStore(first_val, first_address);
					context.builder.CreateStore(second_val, second_address);
					context.add_variable(&p, alloca);
					break;
				}
				}
			}
			else
			{
				bz_assert(fn_it->getType()->isPointerTy());
				context.add_variable(&p, fn_it);
			}
			++p_it;
			++fn_it;
		}
	}

	context.push_expression_scope();
	// code emission for statements
	for (auto &stmt : func_body.get_statements())
	{
		emit_bitcode<abi>(stmt, context);
	}
	context.pop_expression_scope();

	if (!context.has_terminator())
	{
		bz_assert(func_body.return_type.is<ast::ts_void>());
		if (context.current_function.first->is_main())
		{
			context.builder.CreateRet(llvm::ConstantInt::get(context.get_int32_t(), 0));
		}
		else
		{
			context.builder.CreateRetVoid();
		}
	}

	context.builder.SetInsertPoint(alloca_bb);
	context.builder.CreateBr(entry_bb);

	// true means it failed
	/*
	if (llvm::verifyFunction(*fn, &llvm::dbgs()) == true)
	{
		bz::log(
			"{}verifyFunction failed on '{}' !!!{}\n",
			colors::bright_red,
			func_body.get_signature(),
			colors::clear
		);
	}
	*/
	context.current_function = {};
	context.alloca_bb = nullptr;
	context.error_bb = nullptr;
	context.output_pointer = nullptr;
}

void emit_function_bitcode(
	ast::function_body &func_body,
	ctx::comptime_executor_context &context
)
{
	auto const abi = context.get_platform_abi();
	switch (abi)
	{
	case abi::platform_abi::generic:
		emit_function_bitcode_impl<abi::platform_abi::generic>(
			func_body, context
		);
		return;
	case abi::platform_abi::microsoft_x64:
		emit_function_bitcode_impl<abi::platform_abi::microsoft_x64>(
			func_body, context
		);
		return;
	case abi::platform_abi::systemv_amd64:
		emit_function_bitcode_impl<abi::platform_abi::systemv_amd64>(
			func_body, context
		);
		return;
	}
	bz_unreachable;
}

template<abi::platform_abi abi>
static void emit_global_variable_impl(ast::decl_variable const &var_decl, ctx::comptime_executor_context &context)
{
	auto const name = var_decl.id.format_for_symbol();
	auto const name_ref = llvm::StringRef(name.data_as_char_ptr(), name.size());
	auto const type = get_llvm_type(var_decl.var_type, context);
	auto const val = context.get_module().getOrInsertGlobal(name_ref, type);
	bz_assert(llvm::dyn_cast<llvm::GlobalVariable>(val) != nullptr);
	auto const global_var = static_cast<llvm::GlobalVariable *>(val);
	bz_assert(var_decl.init_expr.is<ast::constant_expression>());
	auto const &const_expr = var_decl.init_expr.get<ast::constant_expression>();
	auto const init_val = get_value<abi>(const_expr.value, const_expr.type, &const_expr, context);
	global_var->setInitializer(init_val);
	context.add_variable(&var_decl, global_var);
}

void emit_global_variable(ast::decl_variable const &var_decl, ctx::comptime_executor_context &context)
{
	if (context.vars_.find(&var_decl) != context.vars_.end())
	{
		return;
	}
	auto const abi = context.get_platform_abi();
	switch (abi)
	{
	case abi::platform_abi::generic:
		emit_global_variable_impl<abi::platform_abi::generic>(
			var_decl, context
		);
		return;
	case abi::platform_abi::microsoft_x64:
		emit_global_variable_impl<abi::platform_abi::microsoft_x64>(
			var_decl, context
		);
		return;
	case abi::platform_abi::systemv_amd64:
		emit_global_variable_impl<abi::platform_abi::systemv_amd64>(
			var_decl, context
		);
		return;
	}
	bz_unreachable;
}

void resolve_global_type(ast::type_info *info, llvm::Type *type, ctx::comptime_executor_context &context)
{
	bz_assert(type->isStructTy());
	auto const struct_type = static_cast<llvm::StructType *>(type);
	switch (info->kind)
	{
	case ast::type_info::forward_declaration:
		// there's nothing to do
		return;
	case ast::type_info::aggregate:
	{
		auto const types = info->member_variables
			.transform([&](auto const &member) { return get_llvm_type(member.type, context); })
			.collect();
		struct_type->setBody(llvm::ArrayRef<llvm::Type *>(types.data(), types.size()));
		break;
	}
	default:
		bz_unreachable;
	}
}

void add_builtin_functions(ctx::comptime_executor_context &context)
{
	for (
		uint32_t kind = ast::function_body::_builtin_first;
		kind < ast::function_body::_builtin_last;
		++kind
	)
	{
		auto const body = context.get_builtin_function(kind);
		if (body->symbol_name != "")
		{
			add_function_to_module(body, context);
		}
	}
}

bool emit_necessary_functions(ctx::comptime_executor_context &context)
{
	auto const abi = context.get_platform_abi();
	switch (abi)
	{
	case abi::platform_abi::generic:
		for (size_t i = 0; i < context.functions_to_compile.size(); ++i)
		{
			auto const body = context.functions_to_compile[i];
			if (context.contains_function(body))
			{
				continue;
			}
			if (!context.resolve_function(body))
			{
				return false;
			}
			emit_function_bitcode_impl<abi::platform_abi::generic>(*body, context);
		}
		return true;
	case abi::platform_abi::microsoft_x64:
		for (size_t i = 0; i < context.functions_to_compile.size(); ++i)
		{
			auto const body = context.functions_to_compile[i];
			if (context.contains_function(body))
			{
				continue;
			}
			if (!context.resolve_function(body))
			{
				return false;
			}
			emit_function_bitcode_impl<abi::platform_abi::microsoft_x64>(*body, context);
		}
		return true;
	case abi::platform_abi::systemv_amd64:
		for (size_t i = 0; i < context.functions_to_compile.size(); ++i)
		{
			auto const body = context.functions_to_compile[i];
			if (context.contains_function(body))
			{
				continue;
			}
			if (!context.resolve_function(body))
			{
				return false;
			}
			emit_function_bitcode_impl<abi::platform_abi::systemv_amd64>(*body, context);
		}
		return true;
	}
	bz_unreachable;
}

template<abi::platform_abi abi>
static void add_global_result_getters(
	bz::vector<llvm::Function *> &getters,
	llvm::Constant *global_value_ptr,
	llvm::Type *type,
	bz::vector<std::uint32_t> gep_indicies,
	ctx::comptime_executor_context &context
)
{
	switch (type->getTypeID())
	{
	case llvm::Type::StructTyID:
	{
		auto const struct_type = static_cast<llvm::StructType *>(type);
		gep_indicies.push_back(0);
		for (auto const elem_type : struct_type->elements())
		{
			add_global_result_getters<abi>(getters, global_value_ptr, elem_type, gep_indicies, context);
			gep_indicies.back() += 1;
		}
		gep_indicies.pop_back();
		break;
	}
	case llvm::Type::ArrayTyID:
	{
		auto const array_type = static_cast<llvm::ArrayType *>(type);
		gep_indicies.push_back(0);
		auto const elem_type = array_type->getElementType();
		for ([[maybe_unused]] auto const _ : bz::iota(0, array_type->getNumElements()))
		{
			add_global_result_getters<abi>(getters, global_value_ptr, elem_type, gep_indicies, context);
			gep_indicies.back() += 1;
		}
		gep_indicies.pop_back();
		break;
	}
	default:
	{
		auto const func_type = llvm::FunctionType::get(type, false);
		auto const func = llvm::Function::Create(func_type, llvm::Function::InternalLinkage, "__global_result_getter", &context.get_module());
		getters.push_back(func);
		auto const bb = llvm::BasicBlock::Create(
			context.get_llvm_context(),
			"entry",
			func
		);
		context.builder.SetInsertPoint(bb);
		auto const indicies = gep_indicies
			.transform([&](auto const i) -> llvm::Value * { return llvm::ConstantInt::get(context.get_uint32_t(), i); })
			.collect();
		auto const ptr = context.builder.CreateGEP(global_value_ptr, llvm::ArrayRef(indicies.data(), indicies.size()));
		auto const result_val = context.builder.CreateLoad(ptr);
		context.builder.CreateRet(result_val);
		break;
	}
	}
}

template<abi::platform_abi abi>
static std::pair<llvm::Function *, bz::vector<llvm::Function *>> create_function_for_comptime_execution_impl(
	ast::function_body *body,
	bz::array_view<ast::constant_value const> params,
	ctx::comptime_executor_context &context
)
{
	auto const called_fn = context.get_function(body);
	bz_assert(called_fn != nullptr);

	auto const result_type = get_llvm_type(body->return_type, context);
	auto const void_type = llvm::Type::getVoidTy(context.get_llvm_context());
	auto const return_result_as_global = result_type->isAggregateType();

	auto const result_func_type = llvm::FunctionType::get(return_result_as_global ? void_type : result_type, false);
	std::pair<llvm::Function *, bz::vector<llvm::Function *>> result;
	result.first = llvm::Function::Create(
		result_func_type,
		llvm::Function::InternalLinkage,
		"__anon_comptime_func_call",
		&context.get_module()
	);

	auto const bb = llvm::BasicBlock::Create(context.get_llvm_context(), "entry", result.first);
	context.alloca_bb = bb;
	context.builder.SetInsertPoint(bb);

	auto const result_kind = abi::get_pass_kind<abi>(result_type, context.get_data_layout(), context.get_llvm_context());

	bz::vector<llvm::Value *> args;
	bz::vector<bool> args_is_pass_by_ref;
	args.reserve(params.size() + (result_kind == abi::pass_kind::reference ? 1 : 0));
	args_is_pass_by_ref.reserve(params.size() + (result_kind == abi::pass_kind::reference ? 1 : 0));

	if (result_kind == abi::pass_kind::reference)
	{
		auto const output_ptr = context.create_alloca(result_type);
		args.push_back(output_ptr);
		args_is_pass_by_ref.push_back(false);
	}

	for (auto const [value, i] : params.enumerate())
	{
		if (ast::is_generic_parameter(body->params[i]))
		{
			continue;
		}
		auto const param_t = body->params[i].var_type.as_typespec_view();
		auto const param_type = get_llvm_type(param_t, context);
		auto const param_val = get_value<abi>(value, param_t, nullptr, context);

		auto const pass_kind = abi::get_pass_kind<abi>(param_type, context.get_data_layout(), context.get_llvm_context());
		switch (pass_kind)
		{
		case abi::pass_kind::reference:
		{
			auto const alloca = context.create_alloca(param_type);
			context.builder.CreateStore(param_val, alloca);
			args.push_back(alloca);
			args_is_pass_by_ref.push_back(true);
			break;
		}
		case abi::pass_kind::value:
			args.push_back(param_val);
			args_is_pass_by_ref.push_back(false);
			break;
		case abi::pass_kind::one_register:
		{
			auto const register_type = abi::get_one_register_type<abi>(param_type, context.get_data_layout(), context.get_llvm_context());
			auto const alloca = context.create_alloca(param_type);
			context.builder.CreateStore(param_val, alloca);
			auto const ptr = context.builder.CreatePointerCast(alloca, llvm::PointerType::get(register_type, 0));
			auto const register_value = context.builder.CreateLoad(ptr);
			args.push_back(register_value);
			args_is_pass_by_ref.push_back(false);
			break;
		}
		case abi::pass_kind::two_registers:
		{
			auto const [first_register_type, second_register_type] = abi::get_two_register_types<abi>(
				param_type, context.get_data_layout(), context.get_llvm_context()
			);
			auto const register_struct_type = llvm::StructType::get(first_register_type, second_register_type);
			auto const alloca = context.create_alloca(param_type);
			context.builder.CreateStore(param_val, alloca);
			auto const ptr = context.builder.CreatePointerCast(alloca, llvm::PointerType::get(register_struct_type, 0));
			auto const first_ptr = context.builder.CreateStructGEP(ptr, 0);
			auto const first_val = context.builder.CreateLoad(first_ptr);
			auto const second_ptr = context.builder.CreateStructGEP(ptr, 1);
			auto const second_val = context.builder.CreateLoad(second_ptr);
			args.push_back(first_val);
			args.push_back(second_val);
			args_is_pass_by_ref.push_back(false);
			args_is_pass_by_ref.push_back(false);
			break;
		}
		}
	}

	auto const call = context.builder.CreateCall(called_fn, llvm::ArrayRef(args.data(), args.size()));
	call->setCallingConv(called_fn->getCallingConv());
	auto is_pass_by_ref_it = args_is_pass_by_ref.begin();
	auto const is_pass_by_ref_end = args_is_pass_by_ref.end();
	unsigned i = 0;
	bz_assert(called_fn->arg_size() == call->arg_size());
	if (result_kind == abi::pass_kind::reference)
	{
		call->addParamAttr(0, llvm::Attribute::StructRet);
		bz_assert(is_pass_by_ref_it != is_pass_by_ref_end);
		++is_pass_by_ref_it, ++i;
	}
	for (; is_pass_by_ref_it != is_pass_by_ref_end; ++is_pass_by_ref_it, ++i)
	{
		auto const is_pass_by_ref = *is_pass_by_ref_it;
		if (is_pass_by_ref)
		{
			call->addParamAttr(i, llvm::Attribute::ByVal);
			call->addParamAttr(i, llvm::Attribute::NoAlias);
			call->addParamAttr(i, llvm::Attribute::NoCapture);
			call->addParamAttr(i, llvm::Attribute::NonNull);
		}
	}

	if (return_result_as_global && !result_type->isVoidTy())
	{
		auto const global_result = context.current_module->getOrInsertGlobal("__anon_func_call_result", result_type);
		{
			bz_assert(llvm::dyn_cast<llvm::GlobalVariable>(global_result) != nullptr);
			static_cast<llvm::GlobalVariable *>(global_result)->setInitializer(llvm::UndefValue::get(result_type));
		}

		switch (result_kind)
		{
		case abi::pass_kind::reference:
			context.builder.CreateStore(context.builder.CreateLoad(args.front()), global_result);
			break;
		case abi::pass_kind::value:
			if (body->return_type.is<ast::ts_lvalue_reference>())
			{
				bz_unreachable;
			}
			else
			{
				context.builder.CreateStore(call, global_result);
			}
			break;
		case abi::pass_kind::one_register:
		case abi::pass_kind::two_registers:
		{
			auto const call_result_type = call->getType();
			if (result_type == call_result_type)
			{
				context.builder.CreateStore(call, global_result);
			}
			else
			{
				auto const result_ptr_cast = context.builder.CreatePointerCast(
					global_result,
					llvm::PointerType::get(call_result_type, 0)
				);
				context.builder.CreateStore(call, result_ptr_cast);
			}
			break;
		}
		}
		context.builder.CreateRetVoid();
		bz::vector<uint32_t> gep_indicies = { 0 };
		add_global_result_getters<abi>(result.second, global_result, result_type, gep_indicies, context);
	}
	else
	{
		switch (result_kind)
		{
		case abi::pass_kind::reference:
			context.builder.CreateRet(context.builder.CreateLoad(args.front()));
			break;
		case abi::pass_kind::value:
			if (call->getType()->isVoidTy())
			{
				context.builder.CreateRetVoid();
			}
			else if (body->return_type.is<ast::ts_lvalue_reference>())
			{
				bz_unreachable;
			}
			else
			{
				context.builder.CreateRet(call);
			}
			break;
		case abi::pass_kind::one_register:
		case abi::pass_kind::two_registers:
		{
			auto const call_result_type = call->getType();
			if (result_type == call_result_type)
			{
				context.builder.CreateRet(call);
			}
			else
			{
				auto const result_ptr = context.create_alloca(result_type);
				auto const result_ptr_cast = context.builder.CreatePointerCast(
					result_ptr,
					llvm::PointerType::get(call_result_type, 0)
				);
				context.builder.CreateStore(call, result_ptr_cast);
				context.builder.CreateRet(context.builder.CreateLoad(result_ptr));
			}
			break;
		}
		}
	}

	return result;
}

std::pair<llvm::Function *, bz::vector<llvm::Function *>> create_function_for_comptime_execution(
	ast::function_body *body,
	bz::array_view<ast::constant_value const> params,
	ctx::comptime_executor_context &context
)
{
	auto const abi = context.get_platform_abi();
	switch (abi)
	{
	case abi::platform_abi::generic:
		return create_function_for_comptime_execution_impl<abi::platform_abi::generic>(body, params, context);
	case abi::platform_abi::microsoft_x64:
		return create_function_for_comptime_execution_impl<abi::platform_abi::microsoft_x64>(body, params, context);
	case abi::platform_abi::systemv_amd64:
		return create_function_for_comptime_execution_impl<abi::platform_abi::systemv_amd64>(body, params, context);
	}
	bz_unreachable;
}

template<abi::platform_abi abi>
static std::pair<llvm::Function *, bz::vector<llvm::Function *>> create_function_for_comptime_execution_impl(
	ast::expr_compound &expr,
	ctx::comptime_executor_context &context
)
{
	bz_assert(expr.final_expr.not_null());
	auto const result_type = get_llvm_type(expr.final_expr.get_expr_type_and_kind().first, context);
	auto const void_type = llvm::Type::getVoidTy(context.get_llvm_context());
	auto const return_result_as_global = result_type->isAggregateType() || result_type->isStructTy() || result_type->isArrayTy();

	auto const func_t = llvm::FunctionType::get(return_result_as_global ? void_type : result_type, false);
	std::pair<llvm::Function *, bz::vector<llvm::Function *>> result;
	result.first = llvm::Function::Create(
		func_t, llvm::Function::InternalLinkage,
		"__anon_comptime_compound_expr", &context.get_module()
	);
	context.current_function = { nullptr, result.first };
	auto const alloca_bb = context.add_basic_block("alloca");
	context.alloca_bb = alloca_bb;

	auto const error_bb = context.add_basic_block("error");
	context.error_bb = error_bb;
	context.builder.SetInsertPoint(error_bb);
	if (result.first->getReturnType()->isVoidTy())
	{
		context.builder.CreateRetVoid();
	}
	else
	{
		context.builder.CreateRet(llvm::UndefValue::get(result.first->getReturnType()));
	}

	auto const entry_bb = context.add_basic_block("entry");
	context.builder.SetInsertPoint(entry_bb);

	for (auto &stmt : expr.statements)
	{
		emit_bitcode<abi>(stmt, context);
	}

	if (!context.has_terminator())
	{
		if (return_result_as_global)
		{
			bz_unreachable;
		}
		else
		{
			auto const result_val = emit_bitcode<abi>(expr.final_expr, context, nullptr).get_value(context.builder);
			context.builder.CreateRet(result_val);
		}
	}

	context.builder.SetInsertPoint(alloca_bb);
	context.builder.CreateBr(entry_bb);

	context.current_function = {};
	context.alloca_bb = nullptr;
	context.error_bb = nullptr;
	context.output_pointer = nullptr;

	return result;
}

std::pair<llvm::Function *, bz::vector<llvm::Function *>> create_function_for_comptime_execution(
	ast::expr_compound &expr,
	ctx::comptime_executor_context &context
)
{
	auto const abi = context.get_platform_abi();
	switch (abi)
	{
	case abi::platform_abi::generic:
		return create_function_for_comptime_execution_impl<abi::platform_abi::generic>(expr, context);
	case abi::platform_abi::microsoft_x64:
		return create_function_for_comptime_execution_impl<abi::platform_abi::microsoft_x64>(expr, context);
	case abi::platform_abi::systemv_amd64:
		return create_function_for_comptime_execution_impl<abi::platform_abi::systemv_amd64>(expr, context);
	}
	bz_unreachable;
}

} // namespace bc::comptime
