#include <llvm/IR/Verifier.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/Attributes.h>

#include "emit_bitcode.h"
#include "ctx/built_in_operators.h"
#include "colors.h"
#include "abi/calling_conventions.h"

namespace bc
{

struct val_ptr
{
	enum : uintptr_t
	{
		reference,
		value,
	};
	uintptr_t kind = 0;
	llvm::Value *val = nullptr;
	llvm::Value *consteval_val = nullptr;

	bool has_value(void) const noexcept
	{
		return this->val != nullptr || this->consteval_val != nullptr;
	}

	llvm::Value *get_value(ctx::bitcode_context &context) const
	{
		if (this->consteval_val)
		{
			return this->consteval_val;
		}

		if (this->kind == reference)
		{
			auto const loaded_val = context.builder.CreateLoad(this->val, "load_tmp");
			return loaded_val;
		}
		else
		{
			return this->val;
		}
	}
};


template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expression const &expr,
	ctx::bitcode_context &context
);

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::statement const &stmt,
	ctx::bitcode_context &context
);


template<abi::platform_abi abi>
static void emit_alloca(
	ast::expression const &expr,
	ctx::bitcode_context &context
);

template<abi::platform_abi abi>
static void emit_alloca(
	ast::statement const &stmt,
	ctx::bitcode_context &context
);

static llvm::Type *get_llvm_type(ast::typespec_view ts, ctx::bitcode_context &context, bool is_top_level = true);


#include "common_declarations.impl"
#include "microsoft_x64.impl"


template<abi::platform_abi abi>
static std::pair<llvm::Value *, llvm::Value *> get_common_type_vals(
	ast::expression const &lhs,
	ast::expression const &rhs,
	ctx::bitcode_context &context
)
{
	auto lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);
	bz_assert(lhs_t.is<ast::ts_base_type>());
	bz_assert(rhs_t.is<ast::ts_base_type>());

	auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
	auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);

	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;

	if (lhs_kind == rhs_kind)
	{
		return { lhs_val, rhs_val };
	}
	else if (lhs_kind > rhs_kind)
	{
		if (ctx::is_signed_integer_kind(lhs_kind))
		{
			auto const rhs_cast = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), true, "cast_tmp");
			return { lhs_val, rhs_cast };
		}
		else if (ctx::is_unsigned_integer_kind(lhs_kind))
		{
			auto const rhs_cast = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false, "cast_tmp");
			return { lhs_val, rhs_cast };
		}
		else
		{
			bz_unreachable;
			return {};
		}
	}
	else // lhs_kind < rhs_kind
	{
		if (ctx::is_signed_integer_kind(lhs_kind))
		{
			auto const lhs_cast = context.builder.CreateIntCast(lhs_val, rhs_val->getType(), true, "cast_tmp");
			return { lhs_cast, rhs_val };
		}
		else if (ctx::is_unsigned_integer_kind(lhs_kind))
		{
			auto const lhs_cast = context.builder.CreateIntCast(lhs_val, rhs_val->getType(), false, "cast_tmp");
			return { lhs_cast, rhs_val };
		}
		else if (ctx::is_floating_point_kind(lhs_kind))
		{
			auto const lhs_cast = context.builder.CreateFPCast(lhs_val, rhs_val->getType(), "cast_tmp");
			return { lhs_cast, rhs_val };
		}
		else
		{
			bz_unreachable;
			return {};
		}
	}
}

static llvm::Value *get_constant_zero(
	ast::typespec_view type,
	llvm::Type *llvm_type,
	ctx::bitcode_context &context
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

	case ast::typespec_node_t::index_of<ast::ts_tuple>:
		bz_unreachable;
		return nullptr;
	case ast::typespec_node_t::index_of<ast::ts_unresolved>:
	case ast::typespec_node_t::index_of<ast::ts_void>:
	case ast::typespec_node_t::index_of<ast::ts_lvalue_reference>:
	case ast::typespec_node_t::index_of<ast::ts_auto>:
	default:
		bz_unreachable;
		return nullptr;
	}
}

// ================================================================
// -------------------------- expression --------------------------
// ================================================================

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_identifier const &id,
	ctx::bitcode_context &context
)
{
	auto const val_ptr = context.get_variable(id.decl);
	bz_assert(val_ptr != nullptr);
	return { val_ptr::reference, val_ptr };
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_literal const &,
	ctx::bitcode_context &
)
{
	// this should never be called, as a literal will always be an rvalue constant expression
	bz_unreachable;
	return {};
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_tuple const &,
	ctx::bitcode_context &
)
{
	bz_unreachable;
	return {};
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_unary_op const &unary_op,
	ctx::bitcode_context &context
)
{
	switch (unary_op.op->kind)
	{
	// ==== non-overloadable ====
	case lex::token::address_of:         // '&'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context);
		bz_assert(val.kind == val_ptr::reference);
		return { val_ptr::value, val.val };
	}
	case lex::token::kw_sizeof:          // 'sizeof'
		bz_unreachable;
		return {};

	// ==== overloadable ====
	case lex::token::plus:               // '+'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context).get_value(context);
		return { val_ptr::value, val };
	}
	case lex::token::minus:              // '-'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context).get_value(context);
		auto const res = context.builder.CreateNeg(val, "unary_minus_tmp");
		return { val_ptr::value, res };
	}
	case lex::token::dereference:        // '*'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context).get_value(context);
		return { val_ptr::reference, val };
	}
	case lex::token::bit_not:            // '~'
	case lex::token::bool_not:           // '!'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context).get_value(context);
		auto const res = context.builder.CreateNot(val, "unary_bit_not_tmp");
		return { val_ptr::value, res };
	}

	case lex::token::plus_plus:          // '++'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context);
		bz_assert(val.kind == val_ptr::reference);
		auto const original_value = val.get_value(context);
		if (original_value->getType()->isPointerTy())
		{
			auto const incremented_value = context.builder.CreateConstGEP1_64(original_value, 1);
			context.builder.CreateStore(incremented_value, val.val);
		}
		else
		{
			bz_assert(original_value->getType()->isIntegerTy());
			auto const incremented_value = context.builder.CreateAdd(
				original_value,
				llvm::ConstantInt::get(original_value->getType(), 1)
			);
			context.builder.CreateStore(incremented_value, val.val);
		}
		return val;
	}
	case lex::token::minus_minus:        // '--'
	{
		auto const val = emit_bitcode<abi>(unary_op.expr, context);
		bz_assert(val.kind == val_ptr::reference);
		auto const original_value = val.get_value(context);
		if (original_value->getType()->isPointerTy())
		{
			auto const incremented_value = context.builder.CreateConstGEP1_64(original_value, uint64_t(-1));
			context.builder.CreateStore(incremented_value, val.val);
		}
		else
		{
			bz_assert(original_value->getType()->isIntegerTy());
			auto const incremented_value = context.builder.CreateAdd(
				original_value,
				llvm::ConstantInt::get(original_value->getType(), uint64_t(-1))
			);
			context.builder.CreateStore(incremented_value, val.val);
		}
		return val;
	}
	default:
		bz_unreachable;
		return {};
	}
}


template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_assign(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto const rhs_t = ast::remove_const_or_consteval(binary_op.rhs.get_expr_type_and_kind().first);

	// we calculate the right hand side first
	auto rhs_val = emit_bitcode<abi>(binary_op.rhs, context).get_value(context);
	auto const lhs_val = emit_bitcode<abi>(binary_op.lhs, context);
	bz_assert(lhs_val.kind == val_ptr::reference);
	if (rhs_t.is<ast::ts_base_type>())
	{
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ctx::is_signed_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				lhs_val.val->getType()->getPointerElementType(),
				true
			);
		}
		else if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				lhs_val.val->getType()->getPointerElementType(),
				false
			);
		}
	}
	context.builder.CreateStore(rhs_val, lhs_val.val);
	return lhs_val;
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_plus(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
			auto const [lhs_val, rhs_val] = get_common_type_vals<abi>(binary_op.lhs, binary_op.rhs, context);
			if (ctx::is_floating_point_kind(lhs_kind))
			{
				return { val_ptr::value, context.builder.CreateFAdd(lhs_val, rhs_val, "add_tmp") };
			}
			else
			{
				return { val_ptr::value, context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp") };
			}
		}
		else if (lhs_kind == ast::type_info::char_)
		{
			auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
			auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_uint32_t(),
				ctx::is_signed_integer_kind(rhs_kind)
			);
			return { val_ptr::value, context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp") };
		}
		else // if (rhs_kind == ast::type_info::char_)
		{
			bz_assert(rhs_kind == ast::type_info::char_);
			auto lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
			lhs_val = context.builder.CreateIntCast(
				lhs_val,
				context.get_uint32_t(),
				ctx::is_signed_integer_kind(lhs_kind)
			);
			auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
			return { val_ptr::value, context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp") };
		}
	}
	else if (lhs_t.is<ast::ts_pointer>())
	{
		bz_assert(rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
		auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_uint64_t(), false);
		}
		return { val_ptr::value, context.builder.CreateGEP(lhs_val, rhs_val, "ptr_add_tmp") };
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_pointer>());
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
		auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(lhs_kind))
		{
			lhs_val = context.builder.CreateIntCast(lhs_val, context.get_uint64_t(), false);
		}
		return { val_ptr::value, context.builder.CreateGEP(rhs_val, lhs_val, "ptr_add_tmp") };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_plus_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
			auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
			auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context);
			llvm::Value *res;
			if (ctx::is_integer_kind(lhs_kind))
			{
				rhs_val = context.builder.CreateIntCast(
					rhs_val,
					lhs_val->getType(),
					ctx::is_signed_integer_kind(rhs_kind)
				);
				res = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			}
			else
			{
				bz_assert(ctx::is_floating_point_kind(lhs_kind));
				bz_assert(lhs_kind == rhs_kind);
				res = context.builder.CreateFAdd(lhs_val, rhs_val, "add_tmp");
			}
			context.builder.CreateStore(res, lhs_val_ref.val);
			return lhs_val_ref;
		}
		else
		{
			bz_assert(lhs_kind == ast::type_info::char_);
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
			auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_uint32_t(),
				ctx::is_signed_integer_kind(rhs_kind)
			);
			auto const res = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			context.builder.CreateStore(res, lhs_val_ref.val);
			return lhs_val_ref;
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		// we calculate the right hand side first
		auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_uint64_t(), false);
		}
		auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
		bz_assert(lhs_val_ref.kind == val_ptr::reference);
		auto const lhs_val = lhs_val_ref.get_value(context);
		auto const res = context.builder.CreateGEP(lhs_val, rhs_val, "ptr_add_tmp");
		context.builder.CreateStore(res, lhs_val_ref.val);
		return lhs_val_ref;
	}
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_minus(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
			auto const [lhs_val, rhs_val] = get_common_type_vals<abi>(binary_op.lhs, binary_op.rhs, context);
			if (ctx::is_floating_point_kind(lhs_kind))
			{
				return { val_ptr::value, context.builder.CreateFSub(lhs_val, rhs_val, "sub_tmp") };
			}
			else
			{
				return { val_ptr::value, context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp") };
			}
		}
		else if (
			lhs_kind == ast::type_info::char_
			&& rhs_kind == ast::type_info::char_
		)
		{
			auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
			auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
			return { val_ptr::value, context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp") };
		}
		else
		{
			bz_assert(
				lhs_kind == ast::type_info::char_
				&& ctx::is_integer_kind(rhs_kind)
			);
			auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
			auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_int32_t(),
				ctx::is_signed_integer_kind(rhs_kind)
			);
			return { val_ptr::value, context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp") };
		}
	}
	else if (rhs_t.is<ast::ts_base_type>())
	{
		bz_assert(lhs_t.is<ast::ts_pointer>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
		auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_uint64_t(), false);
		}
		// negate rhs_val
		rhs_val = context.builder.CreateNeg(rhs_val);
		return { val_ptr::value, context.builder.CreateGEP(lhs_val, rhs_val, "ptr_sub_tmp") };
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_pointer>());
		auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
		auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
		return { val_ptr::value, context.builder.CreatePtrDiff(lhs_val, rhs_val, "ptr_diff_tmp") };
		return {};
	}
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_minus_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
			auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
			auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context);
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
			return lhs_val_ref;
		}
		else
		{
			bz_assert(lhs_kind == ast::type_info::char_);
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
			auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_uint32_t(),
				ctx::is_signed_integer_kind(rhs_kind)
			);
			auto const res = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			context.builder.CreateStore(res, lhs_val_ref.val);
			return lhs_val_ref;
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		// we calculate the right hand side first
		auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_uint64_t(), false);
		}
		// negate rhs_val
		rhs_val = context.builder.CreateNeg(rhs_val);
		auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
		bz_assert(lhs_val_ref.kind == val_ptr::reference);
		auto const lhs_val = lhs_val_ref.get_value(context);
		auto const res = context.builder.CreateGEP(lhs_val, rhs_val, "ptr_sub_tmp");
		context.builder.CreateStore(res, lhs_val_ref.val);
		return lhs_val_ref;
	}
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_multiply(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const [lhs_val, rhs_val] = get_common_type_vals<abi>(lhs, rhs, context);
	if (ctx::is_floating_point_kind(lhs_kind))
	{
		return { val_ptr::value, context.builder.CreateFMul(lhs_val, rhs_val, "mul_tmp") };
	}
	else
	{
		return { val_ptr::value, context.builder.CreateMul(lhs_val, rhs_val, "mul_tmp") };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_multiply_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	llvm::Value *res;
	if (ctx::is_integer_kind(lhs_kind))
	{
		rhs_val = context.builder.CreateIntCast(
			rhs_val,
			lhs_val->getType(),
			ctx::is_signed_integer_kind(lhs_kind)
		);
		res = context.builder.CreateMul(lhs_val, rhs_val, "mul_tmp");
	}
	else
	{
		bz_assert(ctx::is_floating_point_kind(lhs_kind));
		bz_assert(lhs_kind == rhs_kind);
		res = context.builder.CreateFMul(lhs_val, rhs_val, "mul_tmp");
	}
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_divide(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const [lhs_val, rhs_val] = get_common_type_vals<abi>(lhs, rhs, context);
	if (ctx::is_signed_integer_kind(lhs_kind))
	{
		return { val_ptr::value, context.builder.CreateSDiv(lhs_val, rhs_val, "div_tmp") };
	}
	else if (ctx::is_unsigned_integer_kind(lhs_kind))
	{
		return { val_ptr::value, context.builder.CreateUDiv(lhs_val, rhs_val, "div_tmp") };
	}
	else
	{
		bz_assert(ctx::is_floating_point_kind(lhs_kind));
		return { val_ptr::value, context.builder.CreateFDiv(lhs_val, rhs_val, "div_tmp") };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_divide_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	llvm::Value *res;
	if (ctx::is_signed_integer_kind(lhs_kind))
	{
		rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), true);
		res = context.builder.CreateSDiv(lhs_val, rhs_val, "div_tmp");
	}
	else if (ctx::is_unsigned_integer_kind(lhs_kind))
	{
		rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false);
		res = context.builder.CreateUDiv(lhs_val, rhs_val, "div_tmp");
	}
	else
	{
		bz_assert(ctx::is_floating_point_kind(lhs_kind));
		bz_assert(lhs_kind == rhs_kind);
		res = context.builder.CreateFDiv(lhs_val, rhs_val, "div_tmp");
	}
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_modulo(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const [lhs_val, rhs_val] = get_common_type_vals<abi>(lhs, rhs, context);
	if (ctx::is_signed_integer_kind(lhs_kind))
	{
		return { val_ptr::value, context.builder.CreateSRem(lhs_val, rhs_val, "mod_tmp") };
	}
	else
	{
		bz_assert(ctx::is_unsigned_integer_kind(lhs_kind));
		return { val_ptr::value, context.builder.CreateURem(lhs_val, rhs_val, "mod_tmp") };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_modulo_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	llvm::Value *res;
	if (ctx::is_signed_integer_kind(lhs_kind))
	{
		rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), true);
		res = context.builder.CreateSRem(lhs_val, rhs_val, "mod_tmp");
	}
	else
	{
		bz_assert(ctx::is_unsigned_integer_kind(lhs_kind));
		rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false);
		res = context.builder.CreateURem(lhs_val, rhs_val, "mod_tmp");
	}
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_cmp(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto const op = binary_op.op->kind;
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
		auto const [lhs_val, rhs_val] = get_common_type_vals<abi>(lhs, rhs, context);
		if (ctx::is_floating_point_kind(lhs_kind))
		{
			auto const pred = get_cmp_predicate(2);
			return { val_ptr::value, context.builder.CreateFCmp(pred, lhs_val, rhs_val) };
		}
		else if (lhs_kind == ast::type_info::str_)
		{
			bz_unreachable;
			return {};
		}
		else
		{
			auto const pred = ctx::is_signed_integer_kind(lhs_kind)
				? get_cmp_predicate(0)
				: get_cmp_predicate(1);
			return { val_ptr::value, context.builder.CreateICmp(pred, lhs_val, rhs_val) };
		}
	}
	else // if pointer
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_pointer>());
		auto const lhs_ptr_val = emit_bitcode<abi>(lhs, context).get_value(context);
		auto const rhs_ptr_val = emit_bitcode<abi>(rhs, context).get_value(context);
		auto const lhs_val = context.builder.CreatePtrToInt(lhs_ptr_val, context.get_uint64_t());
		auto const rhs_val = context.builder.CreatePtrToInt(rhs_ptr_val, context.get_uint64_t());
		auto const p = get_cmp_predicate(1); // unsigned compare
		return { val_ptr::value, context.builder.CreateICmp(p, lhs_val, rhs_val, "cmp_tmp") };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_bit_and(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
	auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	return { val_ptr::value, context.builder.CreateAnd(lhs_val, rhs_val, "bit_and_tmp") };
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_bit_and_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	auto const res = context.builder.CreateAnd(lhs_val, rhs_val, "bit_and_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_bit_xor(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
	auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	return { val_ptr::value, context.builder.CreateXor(lhs_val, rhs_val, "bit_xor_tmp") };
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_bit_xor_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	auto const res = context.builder.CreateXor(lhs_val, rhs_val, "bit_xor_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_bit_or(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
	auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	return { val_ptr::value, context.builder.CreateOr(lhs_val, rhs_val, "bit_or_tmp") };
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_bit_or_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	auto const res = context.builder.CreateOr(lhs_val, rhs_val, "bit_or_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_left_shift(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
	auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false);
	return { val_ptr::value, context.builder.CreateShl(lhs_val, rhs_val, "lshift_tmp") };
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_left_shift_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false);
	auto const res = context.builder.CreateShl(lhs_val, rhs_val, "lshift_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_right_shift(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
	auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false);
	return { val_ptr::value, context.builder.CreateLShr(lhs_val, rhs_val, "rshift_tmp") };
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_right_shift_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false);
	auto const res = context.builder.CreateLShr(lhs_val, rhs_val, "rshift_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_bool_and(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
	auto const lhs_bb_end = context.builder.GetInsertBlock();

	// generate computation of rhs
	auto const rhs_bb = context.add_basic_block("bool_and_rhs");
	context.builder.SetInsertPoint(rhs_bb);
	auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
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

	return { val_ptr::value, phi };
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_bool_xor(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
	auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
	return { val_ptr::value, context.builder.CreateXor(lhs_val, rhs_val, "bool_xor_tmp") };
}

template<abi::platform_abi abi>
static val_ptr emit_built_in_binary_bool_or(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
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
	auto const lhs_val = emit_bitcode<abi>(lhs, context).get_value(context);
	auto const lhs_bb_end = context.builder.GetInsertBlock();

	// generate computation of rhs
	auto const rhs_bb = context.add_basic_block("bool_or_rhs");
	context.builder.SetInsertPoint(rhs_bb);
	auto const rhs_val = emit_bitcode<abi>(rhs, context).get_value(context);
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

	return { val_ptr::value, phi };
}


template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	switch (binary_op.op->kind)
	{
	// ==== non-overloadable ====
	case lex::token::comma:              // ','
	{
		emit_bitcode<abi>(binary_op.lhs, context);
		return emit_bitcode<abi>(binary_op.rhs, context);
	}

	// ==== overloadable ====
	case lex::token::assign:             // '='
		return emit_built_in_binary_assign<abi>(binary_op, context);
	case lex::token::plus:               // '+'
		return emit_built_in_binary_plus<abi>(binary_op, context);
	case lex::token::plus_eq:            // '+='
		return emit_built_in_binary_plus_eq<abi>(binary_op, context);
	case lex::token::minus:              // '-'
		return emit_built_in_binary_minus<abi>(binary_op, context);
	case lex::token::minus_eq:           // '-='
		return emit_built_in_binary_minus_eq<abi>(binary_op, context);
	case lex::token::multiply:           // '*'
		return emit_built_in_binary_multiply<abi>(binary_op, context);
	case lex::token::multiply_eq:        // '*='
		return emit_built_in_binary_multiply_eq<abi>(binary_op, context);
	case lex::token::divide:             // '/'
		return emit_built_in_binary_divide<abi>(binary_op, context);
	case lex::token::divide_eq:          // '/='
		return emit_built_in_binary_divide_eq<abi>(binary_op, context);
	case lex::token::modulo:             // '%'
		return emit_built_in_binary_modulo<abi>(binary_op, context);
	case lex::token::modulo_eq:          // '%='
		return emit_built_in_binary_modulo_eq<abi>(binary_op, context);
	case lex::token::equals:             // '=='
	case lex::token::not_equals:         // '!='
	case lex::token::less_than:          // '<'
	case lex::token::less_than_eq:       // '<='
	case lex::token::greater_than:       // '>'
	case lex::token::greater_than_eq:    // '>='
		return emit_built_in_binary_cmp<abi>(binary_op, context);
	case lex::token::bit_and:            // '&'
		return emit_built_in_binary_bit_and<abi>(binary_op, context);
	case lex::token::bit_and_eq:         // '&='
		return emit_built_in_binary_bit_and_eq<abi>(binary_op, context);
	case lex::token::bit_xor:            // '^'
		return emit_built_in_binary_bit_xor<abi>(binary_op, context);
	case lex::token::bit_xor_eq:         // '^='
		return emit_built_in_binary_bit_xor_eq<abi>(binary_op, context);
	case lex::token::bit_or:             // '|'
		return emit_built_in_binary_bit_or<abi>(binary_op, context);
	case lex::token::bit_or_eq:          // '|='
		return emit_built_in_binary_bit_or_eq<abi>(binary_op, context);
	case lex::token::bit_left_shift:     // '<<'
		return emit_built_in_binary_left_shift<abi>(binary_op, context);
	case lex::token::bit_left_shift_eq:  // '<<='
		return emit_built_in_binary_left_shift_eq<abi>(binary_op, context);
	case lex::token::bit_right_shift:    // '>>'
		return emit_built_in_binary_right_shift<abi>(binary_op, context);
	case lex::token::bit_right_shift_eq: // '>>='
		return emit_built_in_binary_right_shift_eq<abi>(binary_op, context);
	case lex::token::bool_and:           // '&&'
		return emit_built_in_binary_bool_and<abi>(binary_op, context);
	case lex::token::bool_xor:           // '^^'
		return emit_built_in_binary_bool_xor<abi>(binary_op, context);
	case lex::token::bool_or:            // '||'
		return emit_built_in_binary_bool_or<abi>(binary_op, context);

	// these have no built-in operations
	case lex::token::dot_dot:            // '..'
	case lex::token::dot_dot_eq:         // '..='
	default:
		bz_unreachable;
		return {};
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_function_call const &func_call,
	ctx::bitcode_context &context
)
{
	bz_assert(func_call.func_body != nullptr);
	bz::vector<llvm::Value *> params = {};
	params.reserve(func_call.params.size());
	for (size_t i = 0; i < func_call.params.size(); ++i)
	{
		auto &p = func_call.params[i];
		auto &p_t = func_call.func_body->params[i].var_type;
		if (p_t.is<ast::ts_lvalue_reference>())
		{
			auto const val = emit_bitcode<abi>(p, context);
			bz_assert(val.kind == val_ptr::reference);
			params.push_back(val.val);
		}
		else
		{
			auto const val = emit_bitcode<abi>(p, context);
			params.push_back(val.get_value(context));
		}
	}
	auto const fn = context.get_function(func_call.func_body);
	bz_assert(fn != nullptr);
	auto const res = context.builder.CreateCall(fn, llvm::ArrayRef(params.data(), params.size()));

	if (res->getType()->isVoidTy())
	{
		return {};
	}

	if (func_call.func_body->return_type.is<ast::ts_lvalue_reference>())
	{
		return { val_ptr::reference, res };
	}
	else
	{
		return { val_ptr::value, res };
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_subscript const &subscript,
	ctx::bitcode_context &context
)
{
	auto const array = emit_bitcode<abi>(subscript.base, context);
	bz::vector<llvm::Value *> indicies = {};
	for (auto &index : subscript.indicies)
	{
		indicies.push_back(emit_bitcode<abi>(index, context).get_value(context));
	}

	if (array.kind == val_ptr::reference)
	{
		indicies.push_front(llvm::ConstantInt::get(context.get_uint64_t(), 0));
		return {
			val_ptr::reference,
			context.builder.CreateGEP(array.val, llvm::ArrayRef(indicies.data(), indicies.size()))
		};
	}
	else
	{
		bz_assert(array.kind == val_ptr::value);
		return {
			val_ptr::value,
			context.builder.CreateGEP(array.val, llvm::ArrayRef(indicies.data(), indicies.size()))
		};
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_cast const &cast,
	ctx::bitcode_context &context
)
{
	auto const expr_t = ast::remove_const_or_consteval(cast.expr.get_expr_type_and_kind().first);
	auto &dest_t = cast.type;

	if (expr_t.is<ast::ts_base_type>() && dest_t.is<ast::ts_base_type>())
	{
		auto const llvm_dest_t = get_llvm_type(dest_t, context);
		auto const expr = emit_bitcode<abi>(cast.expr, context).get_value(context);
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
			return { val_ptr::value, res };
		}
		else if (ctx::is_floating_point_kind(expr_kind) && ctx::is_floating_point_kind(dest_kind))
		{
			return { val_ptr::value, context.builder.CreateFPCast(expr, llvm_dest_t, "cast_tmp") };
		}
		else if (ctx::is_floating_point_kind(expr_kind))
		{
			bz_assert(ctx::is_integer_kind(dest_kind));
			if (ctx::is_signed_integer_kind(dest_kind))
			{
				return { val_ptr::value, context.builder.CreateFPToSI(expr, llvm_dest_t, "cast_tmp") };
			}
			else
			{
				return { val_ptr::value, context.builder.CreateFPToUI(expr, llvm_dest_t, "cast_tmp") };
			}
		}
		else if (ctx::is_integer_kind(expr_kind) && ctx::is_floating_point_kind(dest_kind))
		{
			if (ctx::is_signed_integer_kind(dest_kind))
			{
				return { val_ptr::value, context.builder.CreateSIToFP(expr, llvm_dest_t, "cast_tmp") };
			}
			else
			{
				return { val_ptr::value, context.builder.CreateUIToFP(expr, llvm_dest_t, "cast_tmp") };
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
			return { val_ptr::value, expr };
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
	ast::expr_compound const &compound_expr,
	ctx::bitcode_context &context
)
{
	for (auto &stmt : compound_expr.statements)
	{
		emit_bitcode<abi>(stmt, context);
	}
	if (compound_expr.final_expr.is_null())
	{
		return {};
	}
	else
	{
		return emit_bitcode<abi>(compound_expr.final_expr, context);
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expr_if const &if_expr,
	ctx::bitcode_context &context
)
{
	auto const condition = emit_bitcode<abi>(if_expr.condition, context).get_value(context);
	// assert that the condition is an i1 (bool)
	bz_assert(condition->getType()->isIntegerTy() && condition->getType()->getIntegerBitWidth() == 1);
	// the original block
	auto const entry_bb = context.builder.GetInsertBlock();

	// emit code for the then block
	auto const then_bb = context.add_basic_block("then");
	context.builder.SetInsertPoint(then_bb);
	auto const then_val = emit_bitcode<abi>(if_expr.then_block, context);
	auto const then_bb_end = context.builder.GetInsertBlock();

	// emit code for the else block if there's any
	auto const else_bb = if_expr.else_block.is_null() ? nullptr : context.add_basic_block("else");
	val_ptr else_val = {};
	if (else_bb)
	{
		context.builder.SetInsertPoint(else_bb);
		else_val = emit_bitcode<abi>(if_expr.else_block, context);
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

	if (then_val.kind == val_ptr::reference && else_val.kind == val_ptr::reference)
	{
		auto const result = context.builder.CreatePHI(then_val.val->getType(), 2);
		bz_assert(then_val.val != nullptr);
		bz_assert(else_val.val != nullptr);
		result->addIncoming(then_val.val, then_bb_end);
		result->addIncoming(else_val.val, else_bb_end);
		return {
			val_ptr::reference,
			result
		};
	}
	else
	{
		auto const then_val_value = then_val.get_value(context);
		auto const else_val_value = else_val.get_value(context);
		auto const result = context.builder.CreatePHI(then_val_value->getType(), 2);
		result->addIncoming(then_val_value, then_bb_end);
		result->addIncoming(else_val_value, else_bb_end);
		return {
			val_ptr::value,
			result
		};
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::constant_expression const &const_expr,
	ctx::bitcode_context &context
)
{
	if (
		const_expr.kind == ast::expression_type_kind::type_name
		|| const_expr.kind == ast::expression_type_kind::none
	)
	{
		return {};
	}
	// consteval variable

	val_ptr result;

	if (const_expr.kind == ast::expression_type_kind::lvalue)
	{
		bz_assert(const_expr.expr.is<ast::expr_identifier>());
		result = emit_bitcode<abi>(*const_expr.expr.get<ast::expr_identifier_ptr>(), context);
	}
	else
	{
		result.kind = val_ptr::value;
	}

	auto const type = get_llvm_type(const_expr.type, context);

	switch (const_expr.value.kind())
	{
	case ast::constant_value::sint:
		result.consteval_val = llvm::ConstantInt::get(
			type,
			bit_cast<uint64_t>(const_expr.value.get<ast::constant_value::sint>()),
			true
		);
		break;
	case ast::constant_value::uint:
		result.consteval_val = llvm::ConstantInt::get(
			type,
			const_expr.value.get<ast::constant_value::uint>(),
			false
		);
		break;
	case ast::constant_value::float32:
		result.consteval_val = llvm::ConstantFP::get(
			type,
			const_expr.value.get<ast::constant_value::float32>()
		);
		break;
	case ast::constant_value::float64:
		result.consteval_val = llvm::ConstantFP::get(
			type,
			const_expr.value.get<ast::constant_value::float64>()
		);
		break;
	case ast::constant_value::u8char:
		result.consteval_val = llvm::ConstantInt::get(
			type,
			const_expr.value.get<ast::constant_value::u8char>()
		);
		break;
	case ast::constant_value::string:
	{
		auto const str = const_expr.value.get<ast::constant_value::string>().as_string_view();
		auto const str_t = llvm::dyn_cast<llvm::StructType>(type);
		bz_assert(str_t != nullptr);

		// if the string is empty, we make a zero initialized string, so
		// structs with a default value of "" get to be zero initialized
		if (str == "")
		{
			result.consteval_val = llvm::ConstantStruct::getNullValue(str_t);
			break;
		}

		auto const string_ref = llvm::StringRef(str.data(), str.size());
		auto const string_constant = context.builder.CreateGlobalString(string_ref);

		auto const begin_ptr = context.builder.CreateConstGEP2_64(string_constant, 0, 0);
		auto const const_begin_ptr = llvm::dyn_cast<llvm::Constant>(begin_ptr);
		bz_assert(const_begin_ptr != nullptr);

		auto const end_ptr = context.builder.CreateConstGEP2_64(string_constant, 0, str.length());
		auto const const_end_ptr = llvm::dyn_cast<llvm::Constant>(end_ptr);
		bz_assert(const_end_ptr != nullptr);
		llvm::Constant *elems[] = { const_begin_ptr, const_end_ptr };

		result.consteval_val = llvm::ConstantStruct::get(str_t, elems);
		break;
	}
	case ast::constant_value::boolean:
		result.consteval_val = llvm::ConstantInt::get(
			type,
			static_cast<uint64_t>(const_expr.value.get<ast::constant_value::boolean>())
		);
		break;
	case ast::constant_value::null:
	{
		if (ast::remove_const_or_consteval(const_expr.type).is<ast::ts_pointer>())
		{
			auto const ptr_t = llvm::dyn_cast<llvm::PointerType>(type);
			bz_assert(ptr_t != nullptr);
			result.consteval_val = llvm::ConstantPointerNull::get(ptr_t);
		}
		else
		{
			auto const struct_t = llvm::dyn_cast<llvm::StructType>(type);
			bz_assert(struct_t != nullptr);
			result.consteval_val = llvm::ConstantStruct::get(struct_t);
		}
		break;
	}
	case ast::constant_value::function:
	{
		auto const decl = const_expr.value.get<ast::constant_value::function>();
		result.consteval_val = context.get_function(decl);
		break;
	}
	case ast::constant_value::function_set_id:
		bz_unreachable;
		return {};
	case ast::constant_value::aggregate:
	default:
		bz_unreachable;
		return {};
	}
	return result;
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::dynamic_expression const &dyn_expr,
	ctx::bitcode_context &context
)
{
	return dyn_expr.expr.visit([&](auto const &expr) {
		return emit_bitcode<abi>(expr, context);
	});
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	ast::expression const &expr,
	ctx::bitcode_context &context
)
{
	switch (expr.kind())
	{
	case ast::expression::index_of<ast::constant_expression>:
		return emit_bitcode<abi>(expr.get<ast::constant_expression>(), context);
	case ast::expression::index_of<ast::dynamic_expression>:
		return emit_bitcode<abi>(expr.get<ast::dynamic_expression>(), context);

	default:
		bz_unreachable;
		return {};
	}
}

// ================================================================
// -------------------------- statement ---------------------------
// ================================================================

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::statement const &stmt,
	ctx::bitcode_context &context
);
/*
static void emit_bitcode(
	ast::stmt_if const &if_stmt,
	ctx::bitcode_context &context
)
{
	auto const condition = emit_bitcode(if_stmt.condition, context).get_value(context);
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
	ctx::bitcode_context &context
)
{
	auto const condition_check = context.add_basic_block("while_condition_check");
	context.builder.CreateBr(condition_check);
	context.builder.SetInsertPoint(condition_check);
	auto const condition = emit_bitcode<abi>(while_stmt.condition, context).get_value(context);
	auto const condition_check_end = context.builder.GetInsertBlock();

	auto const while_bb = context.add_basic_block("while");
	context.builder.SetInsertPoint(while_bb);
	emit_bitcode<abi>(while_stmt.while_block, context);
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
	ctx::bitcode_context &context
)
{
	if (for_stmt.init.not_null())
	{
		emit_bitcode<abi>(for_stmt.init, context);
	}
	auto const condition_check = context.add_basic_block("for_condition_check");
	context.builder.CreateBr(condition_check);
	context.builder.SetInsertPoint(condition_check);
	auto const condition = for_stmt.condition.not_null()
		? emit_bitcode<abi>(for_stmt.condition, context).get_value(context)
		: llvm::ConstantInt::getTrue(context.get_llvm_context());
	auto const condition_check_end = context.builder.GetInsertBlock();

	auto const for_bb = context.add_basic_block("for");
	context.builder.SetInsertPoint(for_bb);
	emit_bitcode<abi>(for_stmt.for_block, context);
	if (!context.has_terminator())
	{
		if (for_stmt.iteration.not_null())
		{
			emit_bitcode<abi>(for_stmt.iteration, context);
		}
		context.builder.CreateBr(condition_check);
	}

	auto const end_bb = context.add_basic_block("endfor");
	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition, for_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
}

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::stmt_return const &ret_stmt,
	ctx::bitcode_context &context
)
{
	if (ret_stmt.expr.is_null())
	{
		context.builder.CreateRetVoid();
	}
	else
	{
		auto const ret_val = emit_bitcode<abi>(ret_stmt.expr, context);
		if (context.current_function.first->return_type.is<ast::ts_lvalue_reference>())
		{
			bz_assert(ret_val.kind == val_ptr::reference);
			context.builder.CreateRet(ret_val.val);
		}
		else
		{
			context.builder.CreateRet(ret_val.get_value(context));
		}
	}
}

template<abi::platform_abi abi>
static void emit_bitcode(
	[[maybe_unused]] ast::stmt_no_op const & no_op_stmt,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
	// we do nothing
	return;
}

/*
static void emit_bitcode(
	ast::stmt_compound const &comp_stmt,
	ctx::bitcode_context &context
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
	ctx::bitcode_context &context
)
{
	emit_bitcode<abi>(expr_stmt.expr, context);
}

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::decl_variable const &var_decl,
	ctx::bitcode_context &context
)
{
	auto const val_ptr = context.get_variable(&var_decl);
	if (val_ptr == nullptr)
	{
		bz_assert(var_decl.var_type.is<ast::ts_lvalue_reference>());
		bz_assert(var_decl.init_expr.not_null());
		auto const init_val = emit_bitcode<abi>(var_decl.init_expr, context);
		bz_assert(init_val.kind == val_ptr::reference);
		context.add_variable(&var_decl, init_val.val);
	}
	else if (var_decl.init_expr.not_null())
	{
		auto const init_val = emit_bitcode<abi>(var_decl.init_expr, context).get_value(context);
		context.builder.CreateStore(init_val, val_ptr);
	}
	else
	{
		auto const ptr_type = val_ptr->getType();
		auto const type = llvm::dyn_cast<llvm::PointerType>(ptr_type);
		bz_assert(type != nullptr);
		auto const init_val = get_constant_zero(var_decl.var_type, type->getElementType(), context);
		context.builder.CreateStore(init_val, val_ptr);
	}
}


template<abi::platform_abi abi>
static void emit_alloca(
	ast::expr_identifier const &,
	ctx::bitcode_context &
)
{
	// nothing
}

template<abi::platform_abi abi>
static void emit_alloca(
	ast::expr_literal const &,
	ctx::bitcode_context &
)
{
	// nothing
}

template<abi::platform_abi abi>
static void emit_alloca(
	ast::expr_tuple const &,
	ctx::bitcode_context &
)
{
	bz_unreachable;
}

template<abi::platform_abi abi>
static void emit_alloca(
	ast::expr_unary_op const &unary_op,
	ctx::bitcode_context &context
)
{
	emit_alloca<abi>(unary_op.expr, context);
}

template<abi::platform_abi abi>
static void emit_alloca(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	emit_alloca<abi>(binary_op.lhs, context);
	emit_alloca<abi>(binary_op.rhs, context);
}

template<abi::platform_abi abi>
static void emit_alloca(
	ast::expr_subscript const &subscript_expr,
	ctx::bitcode_context &context
)
{
	emit_alloca<abi>(subscript_expr.base, context);
	for (auto &index : subscript_expr.indicies)
	{
		emit_alloca<abi>(index, context);
	}
}

template<abi::platform_abi abi>
static void emit_alloca(
	ast::expr_function_call const &fn_call,
	ctx::bitcode_context &context
)
{
	for (auto &param : fn_call.params)
	{
		emit_alloca<abi>(param, context);
	}
}

template<abi::platform_abi abi>
static void emit_alloca(
	ast::expr_cast const &cast_expr,
	ctx::bitcode_context &context
)
{
	emit_alloca<abi>(cast_expr.expr, context);
}

template<abi::platform_abi abi>
static void emit_alloca(
	ast::expr_compound const &expr_compound,
	ctx::bitcode_context &context
)
{
	for (auto &statement : expr_compound.statements)
	{
		emit_alloca<abi>(statement, context);
	}
	if (expr_compound.final_expr.not_null())
	{
		emit_alloca<abi>(expr_compound.final_expr, context);
	}
}

template<abi::platform_abi abi>
static void emit_alloca(
	ast::expr_if const &if_expr,
	ctx::bitcode_context &context
)
{
	emit_alloca<abi>(if_expr.condition, context);
	emit_alloca<abi>(if_expr.then_block, context);
	emit_alloca<abi>(if_expr.else_block, context);
}


template<abi::platform_abi abi>
static void emit_alloca(
	ast::expression const &expr,
	ctx::bitcode_context &context
)
{
	if (expr.kind() == ast::expression::index_of<ast::dynamic_expression>)
	{
		expr.get<ast::dynamic_expression>().expr.visit([&](auto const &expr) {
			emit_alloca<abi>(expr, context);
		});
	}
}

template<abi::platform_abi abi>
static void emit_alloca(
	ast::statement const &stmt,
	ctx::bitcode_context &context
)
{
	switch (stmt.kind())
	{
	case ast::statement::index<ast::stmt_while>:
	{
		auto &while_stmt = *stmt.get<ast::stmt_while_ptr>();
		emit_alloca<abi>(while_stmt.condition, context);
		emit_alloca<abi>(while_stmt.while_block, context);
		break;
	}
	case ast::statement::index<ast::stmt_for>:
	{
		auto &for_stmt = *stmt.get<ast::stmt_for_ptr>();
		emit_alloca<abi>(for_stmt.init, context);
		emit_alloca<abi>(for_stmt.condition, context);
		emit_alloca<abi>(for_stmt.iteration, context);
		emit_alloca<abi>(for_stmt.for_block, context);
		break;
	}
	case ast::statement::index<ast::stmt_return>:
		emit_alloca<abi>(stmt.get<ast::stmt_return_ptr>()->expr, context);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		break;
	case ast::statement::index<ast::stmt_expression>:
		emit_alloca<abi>(stmt.get<ast::stmt_expression_ptr>()->expr, context);
		break;
	case ast::statement::index<ast::decl_variable>:
	{
		auto &var_decl = *stmt.get<ast::decl_variable_ptr>();
		if (var_decl.var_type.is<ast::ts_lvalue_reference>())
		{
			return;
		}
		auto const var_t = get_llvm_type(var_decl.var_type, context);
		auto const name = llvm::StringRef(
			var_decl.identifier->value.data(),
			var_decl.identifier->value.size()
		);
		auto const alloca = context.builder.CreateAlloca(var_t, nullptr, name);
		context.add_variable(&var_decl, alloca);
		emit_alloca<abi>(var_decl.init_expr, context);
		break;
	}
	default:
		break;
	}
}

template<abi::platform_abi abi>
static void emit_bitcode(
	ast::statement const &stmt,
	ctx::bitcode_context &context
)
{
	if (context.has_terminator())
	{
		return;
	}

	switch (stmt.kind())
	{
	case ast::statement::index<ast::stmt_while>:
		emit_bitcode<abi>(*stmt.get<ast::stmt_while_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_for>:
		emit_bitcode<abi>(*stmt.get<ast::stmt_for_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_return>:
		emit_bitcode<abi>(*stmt.get<ast::stmt_return_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		emit_bitcode<abi>(*stmt.get<ast::stmt_no_op_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_expression>:
		emit_bitcode<abi>(*stmt.get<ast::stmt_expression_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_static_assert>:
		// nothing
		break;

	case ast::statement::index<ast::decl_variable>:
		emit_bitcode<abi>(*stmt.get<ast::decl_variable_ptr>(), context);
		break;

	case ast::statement::index<ast::decl_function>:
	case ast::statement::index<ast::decl_operator>:
	case ast::statement::index<ast::decl_struct>:
		break;
	default:
		bz_unreachable;
		break;
	}
}

static llvm::Type *get_llvm_base_type(ast::ts_base_type const &base_t, ctx::bitcode_context &context)
{
	switch (base_t.info->kind)
	{
	case ast::type_info::int8_:
	case ast::type_info::uint8_:
	case ast::type_info::int16_:
	case ast::type_info::uint16_:
	case ast::type_info::int32_:
	case ast::type_info::uint32_:
	case ast::type_info::int64_:
	case ast::type_info::uint64_:
	case ast::type_info::float32_:
	case ast::type_info::float64_:
	case ast::type_info::char_:
	case ast::type_info::str_:
	case ast::type_info::bool_:
	case ast::type_info::null_t_:
		return context.get_built_in_type(base_t.info->kind);

	case ast::type_info::aggregate:
	default:
		bz_unreachable;
		return nullptr;
	}
}

static llvm::Type *get_llvm_type(ast::typespec_view ts, ctx::bitcode_context &context, bool is_top_level)
{
	switch (ts.kind())
	{
	case ast::typespec_node_t::index_of<ast::ts_base_type>:
		return get_llvm_base_type(ts.get<ast::ts_base_type>(), context);
	case ast::typespec_node_t::index_of<ast::ts_void>:
		if (is_top_level)
		{
			return llvm::Type::getVoidTy(context.get_llvm_context());
		}
		else
		{
			// llvm doesn't allow void*, so we use i8* instead
			return llvm::Type::getInt8Ty(context.get_llvm_context());
		}
	case ast::typespec_node_t::index_of<ast::ts_const>:
		return get_llvm_type(ts.get<ast::ts_const>(), context, is_top_level);
	case ast::typespec_node_t::index_of<ast::ts_consteval>:
		return get_llvm_type(ts.get<ast::ts_consteval>(), context, is_top_level);
	case ast::typespec_node_t::index_of<ast::ts_pointer>:
	{
		auto const base = get_llvm_type(ts.get<ast::ts_pointer>(), context, false);
		return llvm::PointerType::get(base, 0);
	}
	case ast::typespec_node_t::index_of<ast::ts_lvalue_reference>:
	{
		auto const base = get_llvm_type(ts.get<ast::ts_lvalue_reference>(), context, false);
		return llvm::PointerType::get(base, 0);
	}
	case ast::typespec_node_t::index_of<ast::ts_function>:
	{
		auto &fn_t = ts.get<ast::ts_function>();
		auto const result_t = get_llvm_type(fn_t.return_type, context);
		bz::vector<llvm::Type *> args = {};
		for (auto &p : fn_t.param_types)
		{
			args.push_back(get_llvm_type(p, context));
		}
		return llvm::PointerType::get(
			llvm::FunctionType::get(result_t, llvm::ArrayRef(args.data(), args.size()), false),
			0
		);
	}
	case ast::typespec_node_t::index_of<ast::ts_array>:
	{
		auto &arr_t = ts.get<ast::ts_array>();
		auto elem_t = get_llvm_type(arr_t.elem_type, context);
		for (auto const size : bz::reversed(arr_t.sizes))
		{
			elem_t = llvm::ArrayType::get(elem_t, size);
		}

		return elem_t;
	}
	case ast::typespec_node_t::index_of<ast::ts_tuple>:
		bz_unreachable;
		return nullptr;
	case ast::typespec_node_t::index_of<ast::ts_auto>:
		bz_unreachable;
		return nullptr;
	case ast::typespec_node_t::index_of<ast::ts_unresolved>:
		bz_unreachable;
		return nullptr;
	default:
		bz_unreachable;
		return nullptr;
	}
}

template<abi::platform_abi abi>
static llvm::Function *create_function_from_symbol_impl(
	ast::function_body &func_body,
	ctx::bitcode_context &context
)
{
	auto const result_t = get_llvm_type(func_body.return_type, context);
	bz::vector<llvm::Type *> args = {};
	for (auto &p : func_body.params)
	{
		auto const t = get_llvm_type(p.var_type, context);
		auto const var_t = ast::remove_const_or_consteval(p.var_type);
		if (
			!(var_t.is<ast::ts_base_type>()
			&& var_t.get<ast::ts_base_type>().info->kind == ast::type_info::str_)
			&& t->isStructTy()
		)
		{
			args.push_back(llvm::PointerType::get(t, 0));
		}
		else
		{
			args.push_back(t);
		}
	}
	auto const func_t = llvm::FunctionType::get(result_t, llvm::ArrayRef(args.data(), args.size()), false);
	auto const name = llvm::StringRef(func_body.symbol_name.data_as_char_ptr(), func_body.symbol_name.size());
	auto const fn = llvm::Function::Create(
		func_t, llvm::Function::ExternalLinkage,
		name, context.get_module()
	);

	switch (func_body.cc)
	{
	case abi::calling_convention::bozon:
		fn->setCallingConv(llvm::CallingConv::Fast);
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

	for (auto &arg : fn->args())
	{
		auto const t = arg.getType();
		if (t->isPointerTy() && t->getPointerElementType()->isStructTy())
		{
			arg.addAttr(llvm::Attribute::ByVal);
		}
	}
	return fn;
}

static llvm::Function *create_function_from_symbol(
	ast::function_body &func_body,
	ctx::bitcode_context &context
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
	}
	bz_unreachable;
}

void add_function_to_module(
	ast::function_body *func_body,
	ctx::bitcode_context &context
)
{
	auto const fn = create_function_from_symbol(*func_body, context);
	context.funcs_.insert({ func_body, fn });
}

template<abi::platform_abi abi>
static void emit_function_bitcode_impl(
	ast::function_body &func_body,
	ctx::bitcode_context &context
)
{
	auto const fn = context.get_function(&func_body);
	bz_assert(fn != nullptr);
	bz_assert(fn->size() == 0);

	context.current_function = { &func_body, fn };
	auto const bb = context.add_basic_block("entry");
	context.builder.SetInsertPoint(bb);

	bz_assert(func_body.body.is<bz::vector<ast::statement>>());
	bz::vector<llvm::Value *> params = {};
	params.reserve(func_body.params.size());
	// alloca's for function paraementers
	{
		bz_assert(func_body.params.size() == fn->arg_size());
		auto p_it = func_body.params.begin();
		auto fn_it = fn->arg_begin();
		auto const fn_end = fn->arg_end();
		for (; fn_it != fn_end; ++fn_it, ++p_it)
		{
			auto &p = *p_it;
			if (p.var_type.is<ast::ts_lvalue_reference>())
			{
				context.add_variable(&p, fn_it);
			}
			else
			{
				auto const var_t = get_llvm_type(p.var_type, context);
				auto const name = llvm::StringRef(p.identifier->value.data(), p.identifier->value.size());
				auto const alloca = context.builder.CreateAlloca(var_t, nullptr, name);
				context.add_variable(&p, alloca);
			}
		}
	}
	// alloca's for statements
	for (auto &stmt : func_body.get_statements())
	{
		emit_alloca<abi>(stmt, context);
	}

	// initialization of function parameter alloca's
	{
		auto p_it = func_body.params.begin();
		auto fn_it = fn->arg_begin();
		auto const fn_end = fn->arg_end();
		for (; fn_it != fn_end; ++fn_it, ++p_it)
		{
			auto &p = *p_it;
			if (!p.var_type.is<ast::ts_lvalue_reference>())
			{
				auto const val = context.get_variable(&p);
				bz_assert(val != nullptr);
				auto const param_val = fn_it;
				context.builder.CreateStore(param_val, val);
			}
		}
	}

	// code emission for statements
	for (auto &stmt : func_body.get_statements())
	{
		emit_bitcode<abi>(stmt, context);
	}

	if (!context.has_terminator())
	{
//		std::string func_name = context.current_function->getName();
//		bz::print("{} {} {}\n", func_body.return_type, func_body.params.size(), func_name.c_str());
		bz_assert(func_body.return_type.is<ast::ts_void>());
		context.builder.CreateRetVoid();
	}

	// true means it failed
	if (llvm::verifyFunction(*fn) == true)
	{
		auto const fn_name = bz::u8string_view(
			fn->getName().data(),
			fn->getName().data() + fn->getName().size()
		);
		bz::print(
			"{}verifyFunction failed on {}!!!\n{}",
			colors::bright_red,
			ast::function_body::decode_symbol_name(fn_name),
			colors::clear
		);
	}
	context.current_function = {};
}

void emit_function_bitcode(
	ast::function_body &func_body,
	ctx::bitcode_context &context
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
	}
	bz_unreachable;
}

} // namespace bc
