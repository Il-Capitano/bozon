#include <llvm/IR/Verifier.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/Attributes.h>

#include "emit_bitcode.h"
#include "ctx/built_in_operators.h"
#include "colors.h"

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

	llvm::Value *get_value(ctx::bitcode_context &context) const
	{
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

static val_ptr emit_bitcode(
	ast::expression const &expr,
	ctx::bitcode_context &context
);

static llvm::Type *get_llvm_type(ast::typespec const &ts, ctx::bitcode_context &context);

static std::pair<llvm::Value *, llvm::Value *> get_common_type_vals(
	ast::expression const &lhs,
	ast::expression const &rhs,
	ctx::bitcode_context &context
)
{
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);
	bz_assert(lhs_t.is<ast::ts_base_type>());
	bz_assert(rhs_t.is<ast::ts_base_type>());

	auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
	auto const rhs_val = emit_bitcode(rhs, context).get_value(context);

	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;

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
			bz_assert(false);
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
			bz_assert(false);
			return {};
		}
	}
}

// ================================================================
// -------------------------- expression --------------------------
// ================================================================

static val_ptr emit_bitcode(
	ast::expr_identifier const &id,
	ctx::bitcode_context &context
)
{
	auto const val_ptr = context.get_variable(id.decl);
	bz_assert(val_ptr != nullptr);
	return { val_ptr::reference, val_ptr };
}

static val_ptr emit_bitcode(
	ast::expr_literal const &literal,
	ctx::bitcode_context &context
)
{
	bz_assert(false);
	return {};
}

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
		auto const val = emit_bitcode(unary_op.expr, context);
		bz_assert(val.kind == val_ptr::reference);
		return { val_ptr::value, val.val };
	}
	case lex::token::kw_sizeof:          // 'sizeof'
		bz_assert(false);
		return {};

	// ==== overloadable ====
	case lex::token::plus:               // '+'
	{
		auto const val = emit_bitcode(unary_op.expr, context).get_value(context);
		return { val_ptr::value, val };
	}
	case lex::token::minus:              // '-'
	{
		auto const val = emit_bitcode(unary_op.expr, context).get_value(context);
		auto const res = context.builder.CreateNeg(val, "unary_minus_tmp");
		return { val_ptr::value, res };
	}
	case lex::token::dereference:        // '*'
	{
		auto const val = emit_bitcode(unary_op.expr, context).get_value(context);
		return { val_ptr::reference, val };
	}
	case lex::token::bit_not:            // '~'
	case lex::token::bool_not:           // '!'
	{
		auto const val = emit_bitcode(unary_op.expr, context).get_value(context);
		auto const res = context.builder.CreateNot(val, "unary_bit_not_tmp");
		return { val_ptr::value, res };
	}

	case lex::token::plus_plus:          // '++'
	{
		auto const val = emit_bitcode(unary_op.expr, context);
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
		auto const val = emit_bitcode(unary_op.expr, context);
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
		bz_assert(false);
		return {};
	}
}


static val_ptr emit_built_in_binary_assign(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &rhs_t = ast::remove_const(binary_op.rhs.get_expr_type_and_kind().first);

	// we calculate the right hand side first
	auto rhs_val = emit_bitcode(binary_op.rhs, context).get_value(context);
	auto const lhs_val = emit_bitcode(binary_op.lhs, context);
	bz_assert(lhs_val.kind == val_ptr::reference);
	if (rhs_t.is<ast::ts_base_type>())
	{
		auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
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

static val_ptr emit_built_in_binary_plus(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		if (ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind))
		{
			auto const [lhs_val, rhs_val] = get_common_type_vals(binary_op.lhs, binary_op.rhs, context);
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
			auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
			auto rhs_val = emit_bitcode(rhs, context).get_value(context);
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
			auto lhs_val = emit_bitcode(lhs, context).get_value(context);
			lhs_val = context.builder.CreateIntCast(
				lhs_val,
				context.get_uint32_t(),
				ctx::is_signed_integer_kind(lhs_kind)
			);
			auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
			return { val_ptr::value, context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp") };
		}
	}
	else if (lhs_t.is<ast::ts_pointer>())
	{
		bz_assert(rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
		auto rhs_val = emit_bitcode(rhs, context).get_value(context);
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
		auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		auto lhs_val = emit_bitcode(lhs, context).get_value(context);
		auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(lhs_kind))
		{
			lhs_val = context.builder.CreateIntCast(lhs_val, context.get_uint64_t(), false);
		}
		return { val_ptr::value, context.builder.CreateGEP(rhs_val, lhs_val, "ptr_add_tmp") };
	}
}

static val_ptr emit_built_in_binary_plus_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		if (ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind))
		{
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode(rhs, context).get_value(context);
			auto const lhs_val_ref = emit_bitcode(lhs, context);
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
			auto rhs_val = emit_bitcode(rhs, context).get_value(context);
			auto const lhs_val_ref = emit_bitcode(lhs, context);
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
		auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		// we calculate the right hand side first
		auto rhs_val = emit_bitcode(rhs, context).get_value(context);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_uint64_t(), false);
		}
		auto const lhs_val_ref = emit_bitcode(lhs, context);
		bz_assert(lhs_val_ref.kind == val_ptr::reference);
		auto const lhs_val = lhs_val_ref.get_value(context);
		auto const res = context.builder.CreateGEP(lhs_val, rhs_val, "ptr_add_tmp");
		context.builder.CreateStore(res, lhs_val_ref.val);
		return lhs_val_ref;
	}
}

static val_ptr emit_built_in_binary_minus(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		if (ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind))
		{
			auto const [lhs_val, rhs_val] = get_common_type_vals(binary_op.lhs, binary_op.rhs, context);
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
			auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
			auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
			return { val_ptr::value, context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp") };
		}
		else
		{
			bz_assert(
				lhs_kind == ast::type_info::char_
				&& ctx::is_integer_kind(rhs_kind)
			);
			auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
			auto rhs_val = emit_bitcode(rhs, context).get_value(context);
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
		auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
		auto rhs_val = emit_bitcode(rhs, context).get_value(context);
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
		auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
		auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
		return { val_ptr::value, context.builder.CreatePtrDiff(lhs_val, rhs_val, "ptr_diff_tmp") };
		return {};
	}
}

static val_ptr emit_built_in_binary_minus_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		if (ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind))
		{
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode(rhs, context).get_value(context);
			auto const lhs_val_ref = emit_bitcode(lhs, context);
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
			auto rhs_val = emit_bitcode(rhs, context).get_value(context);
			auto const lhs_val_ref = emit_bitcode(lhs, context);
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
		auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		// we calculate the right hand side first
		auto rhs_val = emit_bitcode(rhs, context).get_value(context);
		// we need to cast unsigned integers to uint64, otherwise big values might count as a negative index
		if (ctx::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_uint64_t(), false);
		}
		// negate rhs_val
		rhs_val = context.builder.CreateNeg(rhs_val);
		auto const lhs_val_ref = emit_bitcode(lhs, context);
		bz_assert(lhs_val_ref.kind == val_ptr::reference);
		auto const lhs_val = lhs_val_ref.get_value(context);
		auto const res = context.builder.CreateGEP(lhs_val, rhs_val, "ptr_sub_tmp");
		context.builder.CreateStore(res, lhs_val_ref.val);
		return lhs_val_ref;
	}
}

static val_ptr emit_built_in_binary_multiply(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind));
	auto const [lhs_val, rhs_val] = get_common_type_vals(lhs, rhs, context);
	if (ctx::is_floating_point_kind(lhs_kind))
	{
		return { val_ptr::value, context.builder.CreateFMul(lhs_val, rhs_val, "mul_tmp") };
	}
	else
	{
		return { val_ptr::value, context.builder.CreateMul(lhs_val, rhs_val, "mul_tmp") };
	}
}

static val_ptr emit_built_in_binary_multiply_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind));
	// we calculate the right hand side first
	auto rhs_val = emit_bitcode(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode(lhs, context);
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

static val_ptr emit_built_in_binary_divide(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind));
	auto const [lhs_val, rhs_val] = get_common_type_vals(lhs, rhs, context);
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

static val_ptr emit_built_in_binary_divide_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind));
	// we calculate the right hand side first
	auto rhs_val = emit_bitcode(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode(lhs, context);
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

static val_ptr emit_built_in_binary_modulo(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(ctx::is_integer_kind(lhs_kind) && ctx::is_integer_kind(rhs_kind));
	auto const [lhs_val, rhs_val] = get_common_type_vals(lhs, rhs, context);
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

static val_ptr emit_built_in_binary_modulo_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(ctx::is_integer_kind(lhs_kind) && ctx::is_integer_kind(rhs_kind));
	// we calculate the right hand side first
	auto rhs_val = emit_bitcode(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode(lhs, context);
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
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);
	// 0: signed int
	// 1: unsigned int
	// 2: float
	auto const get_cmp_predicate = [op](int kind)
	{
		llvm::CmpInst::Predicate preds[3][6] = {
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
		int const pred = [op]() {
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
		auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
		bz_assert(lhs_kind != ast::type_info::str_);
		auto const [lhs_val, rhs_val] = get_common_type_vals(lhs, rhs, context);
		auto const p = ctx::is_signed_integer_kind(lhs_kind) ? get_cmp_predicate(0)
			: ctx::is_floating_point_kind(lhs_kind) ? get_cmp_predicate(2)
			: get_cmp_predicate(1); // unsigned, bool, char
		if (ctx::is_floating_point_kind(lhs_kind))
		{
			return { val_ptr::value, context.builder.CreateFCmp(p, lhs_val, rhs_val) };
		}
		else
		{
			return { val_ptr::value, context.builder.CreateICmp(p, lhs_val, rhs_val) };
		}
	}
	else // if pointer
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_pointer>());
		auto const lhs_ptr_val = emit_bitcode(lhs, context).get_value(context);
		auto const rhs_ptr_val = emit_bitcode(rhs, context).get_value(context);
		auto const lhs_val = context.builder.CreatePtrToInt(lhs_ptr_val, context.get_uint64_t());
		auto const rhs_val = context.builder.CreatePtrToInt(rhs_ptr_val, context.get_uint64_t());
		auto const p = get_cmp_predicate(1); // unsigned compare
		return { val_ptr::value, context.builder.CreateICmp(p, lhs_val, rhs_val, "cmp_tmp") };
	}
}

static val_ptr emit_built_in_binary_bit_and(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
	auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
	return { val_ptr::value, context.builder.CreateAnd(lhs_val, rhs_val, "bit_and_tmp") };
}

static val_ptr emit_built_in_binary_bit_and_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	auto const res = context.builder.CreateAnd(lhs_val, rhs_val, "bit_and_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_built_in_binary_bit_xor(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
	auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
	return { val_ptr::value, context.builder.CreateXor(lhs_val, rhs_val, "bit_xor_tmp") };
}

static val_ptr emit_built_in_binary_bit_xor_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	auto const res = context.builder.CreateXor(lhs_val, rhs_val, "bit_xor_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_built_in_binary_bit_or(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
	auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
	return { val_ptr::value, context.builder.CreateOr(lhs_val, rhs_val, "bit_or_tmp") };
}

static val_ptr emit_built_in_binary_bit_or_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(
		(ctx::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	auto const res = context.builder.CreateOr(lhs_val, rhs_val, "bit_or_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_built_in_binary_left_shift(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
	auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
	auto rhs_val = emit_bitcode(rhs, context).get_value(context);
	rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false);
	return { val_ptr::value, context.builder.CreateShl(lhs_val, rhs_val, "lshift_tmp") };
}

static val_ptr emit_built_in_binary_left_shift_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
	// we calculate the right hand side first
	auto rhs_val = emit_bitcode(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false);
	auto const res = context.builder.CreateShl(lhs_val, rhs_val, "lshift_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_built_in_binary_right_shift(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
	auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
	auto rhs_val = emit_bitcode(rhs, context).get_value(context);
	rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false);
	return { val_ptr::value, context.builder.CreateLShr(lhs_val, rhs_val, "rshift_tmp") };
}

static val_ptr emit_built_in_binary_right_shift_eq(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
	// we calculate the right hand side first
	auto rhs_val = emit_bitcode(rhs, context).get_value(context);
	auto const lhs_val_ref = emit_bitcode(lhs, context);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context);
	rhs_val = context.builder.CreateIntCast(rhs_val, lhs_val->getType(), false);
	auto const res = context.builder.CreateLShr(lhs_val, rhs_val, "rshift_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_built_in_binary_bool_and(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(lhs_kind == ast::type_info::bool_ && rhs_kind == ast::type_info::bool_);

	// generate computation of lhs
	auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
	auto const lhs_bb_end = context.builder.GetInsertBlock();

	// generate computation of rhs
	auto const rhs_bb = context.add_basic_block("bool_and_rhs");
	context.builder.SetInsertPoint(rhs_bb);
	auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
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

static val_ptr emit_built_in_binary_bool_xor(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(lhs_kind == ast::type_info::bool_ && rhs_kind == ast::type_info::bool_);
	auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
	auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
	return { val_ptr::value, context.builder.CreateXor(lhs_val, rhs_val, "bool_xor_tmp") };
}

static val_ptr emit_built_in_binary_bool_or(
	ast::expr_binary_op const &binary_op,
	ctx::bitcode_context &context
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;
	auto &lhs_t = ast::remove_const(lhs.get_expr_type_and_kind().first);
	auto &rhs_t = ast::remove_const(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
	bz_assert(lhs_kind == ast::type_info::bool_ && rhs_kind == ast::type_info::bool_);

	// generate computation of lhs
	auto const lhs_val = emit_bitcode(lhs, context).get_value(context);
	auto const lhs_bb_end = context.builder.GetInsertBlock();

	// generate computation of rhs
	auto const rhs_bb = context.add_basic_block("bool_or_rhs");
	context.builder.SetInsertPoint(rhs_bb);
	auto const rhs_val = emit_bitcode(rhs, context).get_value(context);
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
		emit_bitcode(binary_op.lhs, context);
		return emit_bitcode(binary_op.rhs, context);
	}

	// ==== overloadable ====
	case lex::token::assign:             // '='
		return emit_built_in_binary_assign(binary_op, context);
	case lex::token::plus:               // '+'
		return emit_built_in_binary_plus(binary_op, context);
	case lex::token::plus_eq:            // '+='
		return emit_built_in_binary_plus_eq(binary_op, context);
	case lex::token::minus:              // '-'
		return emit_built_in_binary_minus(binary_op, context);
	case lex::token::minus_eq:           // '-='
		return emit_built_in_binary_minus_eq(binary_op, context);
	case lex::token::multiply:           // '*'
		return emit_built_in_binary_multiply(binary_op, context);
	case lex::token::multiply_eq:        // '*='
		return emit_built_in_binary_multiply_eq(binary_op, context);
	case lex::token::divide:             // '/'
		return emit_built_in_binary_divide(binary_op, context);
	case lex::token::divide_eq:          // '/='
		return emit_built_in_binary_divide_eq(binary_op, context);
	case lex::token::modulo:             // '%'
		return emit_built_in_binary_modulo(binary_op, context);
	case lex::token::modulo_eq:          // '%='
		return emit_built_in_binary_modulo_eq(binary_op, context);
	case lex::token::equals:             // '=='
	case lex::token::not_equals:         // '!='
	case lex::token::less_than:          // '<'
	case lex::token::less_than_eq:       // '<='
	case lex::token::greater_than:       // '>'
	case lex::token::greater_than_eq:    // '>='
		return emit_built_in_binary_cmp(binary_op, context);
	case lex::token::bit_and:            // '&'
		return emit_built_in_binary_bit_and(binary_op, context);
	case lex::token::bit_and_eq:         // '&='
		return emit_built_in_binary_bit_and_eq(binary_op, context);
	case lex::token::bit_xor:            // '^'
		return emit_built_in_binary_bit_xor(binary_op, context);
	case lex::token::bit_xor_eq:         // '^='
		return emit_built_in_binary_bit_xor_eq(binary_op, context);
	case lex::token::bit_or:             // '|'
		return emit_built_in_binary_bit_or(binary_op, context);
	case lex::token::bit_or_eq:          // '|='
		return emit_built_in_binary_bit_or_eq(binary_op, context);
	case lex::token::bit_left_shift:     // '<<'
		return emit_built_in_binary_left_shift(binary_op, context);
	case lex::token::bit_left_shift_eq:  // '<<='
		return emit_built_in_binary_left_shift_eq(binary_op, context);
	case lex::token::bit_right_shift:    // '>>'
		return emit_built_in_binary_right_shift(binary_op, context);
	case lex::token::bit_right_shift_eq: // '>>='
		return emit_built_in_binary_right_shift_eq(binary_op, context);
	case lex::token::bool_and:           // '&&'
		return emit_built_in_binary_bool_and(binary_op, context);
	case lex::token::bool_xor:           // '^^'
		return emit_built_in_binary_bool_xor(binary_op, context);
	case lex::token::bool_or:            // '||'
		return emit_built_in_binary_bool_or(binary_op, context);

	case lex::token::square_open:        // '[]'

	// these have no built-in operations
	case lex::token::dot_dot:            // '..'
	case lex::token::dot_dot_eq:         // '..='
	default:
		bz_assert(false);
		return {};
	}
}

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
		if (p_t.is<ast::ts_reference>())
		{
			auto const val = emit_bitcode(p, context);
			bz_assert(val.kind == val_ptr::reference);
			params.push_back(val.val);
		}
		else
		{
			auto const val = emit_bitcode(p, context);
			params.push_back(val.get_value(context));
		}
	}
	auto const fn = context.get_function(func_call.func_body);
	bz_assert(fn != nullptr);
	auto const res = context.builder.CreateCall(fn, llvm::ArrayRef(params.data(), params.size()));
	if (func_call.func_body->return_type.is<ast::ts_reference>())
	{
		return { val_ptr::reference, res };
	}
	else
	{
		return { val_ptr::value, res };
	}
}

static val_ptr emit_bitcode(
	ast::expr_cast const &cast,
	ctx::bitcode_context &context
)
{
	auto &expr_t = ast::remove_const(cast.expr.get_expr_type_and_kind().first);
	auto &dest_t = cast.type;

	if (expr_t.is<ast::ts_base_type>() && dest_t.is<ast::ts_base_type>())
	{
		auto const llvm_dest_t = get_llvm_type(dest_t, context);
		auto const expr = emit_bitcode(cast.expr, context).get_value(context);
		auto const expr_kind = expr_t.get<ast::ts_base_type_ptr>()->info->kind;
		auto const dest_kind = dest_t.get<ast::ts_base_type_ptr>()->info->kind;

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
		bz_assert(false);
		return {};
	}
}

static val_ptr emit_bitcode(
	ast::constant_expression const &const_expr,
	ctx::bitcode_context &context
)
{
	if (const_expr.kind == ast::expression_type_kind::type_name)
	{
		return {};
	}
	auto const type = get_llvm_type(const_expr.type, context);
	switch (const_expr.value.kind())
	{
	case ast::constant_value::sint:
		return {
			val_ptr::value,
			llvm::ConstantInt::get(
				type,
				bit_cast<uint64_t>(const_expr.value.get<ast::constant_value::sint>()),
				true
			)
		};
	case ast::constant_value::uint:
		return {
			val_ptr::value,
			llvm::ConstantInt::get(
				type,
				const_expr.value.get<ast::constant_value::uint>(),
				false
			)
		};
	case ast::constant_value::float32:
		return {
			val_ptr::value,
			llvm::ConstantFP::get(
				type,
				const_expr.value.get<ast::constant_value::float32>()
			)
		};
	case ast::constant_value::float64:
		return {
			val_ptr::value,
			llvm::ConstantFP::get(
				type,
				const_expr.value.get<ast::constant_value::float64>()
			)
		};
	case ast::constant_value::u8char:
		return {
			val_ptr::value,
			llvm::ConstantInt::get(
				type,
				const_expr.value.get<ast::constant_value::u8char>()
			)
		};
	case ast::constant_value::string:
	{
		auto const &str = const_expr.value.get<ast::constant_value::string>();
		auto const string_ref = llvm::StringRef(str.data_as_char_ptr(), str.size());
		auto const string_constant = context.builder.CreateGlobalString(string_ref);

		auto const begin_ptr = context.builder.CreateConstGEP2_64(string_constant, 0, 0);
		auto const const_begin_ptr = llvm::dyn_cast<llvm::Constant>(begin_ptr);
		bz_assert(const_begin_ptr != nullptr);

		auto const end_ptr = context.builder.CreateConstGEP2_64(string_constant, 0, str.length());
		auto const const_end_ptr = llvm::dyn_cast<llvm::Constant>(end_ptr);
		bz_assert(const_end_ptr != nullptr);

		auto const str_t = llvm::dyn_cast<llvm::StructType>(type);
		bz_assert(str_t != nullptr);
		llvm::Constant *elems[] = { const_begin_ptr, const_end_ptr };

		return {
			val_ptr::value,
			llvm::ConstantStruct::get(str_t, elems)
		};
	}
	case ast::constant_value::boolean:
		return {
			val_ptr::value,
			llvm::ConstantInt::get(
				type,
				static_cast<uint64_t>(const_expr.value.get<ast::constant_value::boolean>())
			)
		};
	case ast::constant_value::null:
	{
		auto ptr_t = llvm::dyn_cast<llvm::PointerType>(type);
		bz_assert(ptr_t != nullptr);
		return {
			val_ptr::value,
			llvm::ConstantPointerNull::get(ptr_t)
		};
	}
	case ast::constant_value::function:
	{
		auto const decl = const_expr.value.get<ast::constant_value::function>();
		return {
			val_ptr::value,
			context.get_function(decl)
		};
	}
	case ast::constant_value::function_set_id:
		bz_assert(false);
		return {};
	case ast::constant_value::aggregate:
	default:
		bz_assert(false);
		return {};
	}
}

static val_ptr emit_bitcode(
	ast::dynamic_expression const &dyn_expr,
	ctx::bitcode_context &context
)
{
	switch (dyn_expr.expr.kind())
	{
	case ast::expr_t::index<ast::expr_identifier>:
		return emit_bitcode(*dyn_expr.expr.get<ast::expr_identifier_ptr>(), context);
	case ast::expr_t::index<ast::expr_literal>:
		return emit_bitcode(*dyn_expr.expr.get<ast::expr_literal_ptr>(), context);
	case ast::expr_t::index<ast::expr_unary_op>:
		return emit_bitcode(*dyn_expr.expr.get<ast::expr_unary_op_ptr>(), context);
	case ast::expr_t::index<ast::expr_binary_op>:
		return emit_bitcode(*dyn_expr.expr.get<ast::expr_binary_op_ptr>(), context);
	case ast::expr_t::index<ast::expr_function_call>:
		return emit_bitcode(*dyn_expr.expr.get<ast::expr_function_call_ptr>(), context);
	case ast::expr_t::index<ast::expr_cast>:
		return emit_bitcode(*dyn_expr.expr.get<ast::expr_cast_ptr>(), context);

	default:
		bz_assert(false);
		return {};
	}
}

static val_ptr emit_bitcode(
	ast::expression const &expr,
	ctx::bitcode_context &context
)
{
	switch (expr.kind())
	{
	case ast::expression::index_of<ast::constant_expression>:
		return emit_bitcode(expr.get<ast::constant_expression>(), context);
	case ast::expression::index_of<ast::dynamic_expression>:
		return emit_bitcode(expr.get<ast::dynamic_expression>(), context);

	case ast::expression::index_of<ast::tuple_expression>:
	default:
		bz_assert(false);
		return {};
	}
}

// ================================================================
// -------------------------- statement ---------------------------
// ================================================================

static void emit_bitcode(
	ast::statement const &stmt,
	ctx::bitcode_context &context
);

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

static void emit_bitcode(
	ast::stmt_while const &while_stmt,
	ctx::bitcode_context &context
)
{
	auto const condition_check = context.add_basic_block("while_condition_check");
	context.builder.CreateBr(condition_check);
	context.builder.SetInsertPoint(condition_check);
	auto const condition = emit_bitcode(while_stmt.condition, context).get_value(context);
	auto const condition_check_end = context.builder.GetInsertBlock();

	auto const while_bb = context.add_basic_block("while");
	context.builder.SetInsertPoint(while_bb);
	emit_bitcode(while_stmt.while_block, context);
	if (!context.has_terminator())
	{
		context.builder.CreateBr(condition_check);
	}

	auto const end_bb = context.add_basic_block("endwhile");
	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition, while_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
}

static void emit_bitcode(
	ast::stmt_for const &for_stmt,
	ctx::bitcode_context &context
)
{
	if (for_stmt.init.not_null())
	{
		emit_bitcode(for_stmt.init, context);
	}
	auto const condition_check = context.add_basic_block("for_condition_check");
	context.builder.CreateBr(condition_check);
	context.builder.SetInsertPoint(condition_check);
	auto const condition = for_stmt.condition.not_null()
		? emit_bitcode(for_stmt.condition, context).get_value(context)
		: llvm::ConstantInt::getTrue(context.get_llvm_context());
	auto const condition_check_end = context.builder.GetInsertBlock();

	auto const for_bb = context.add_basic_block("for");
	context.builder.SetInsertPoint(for_bb);
	emit_bitcode(for_stmt.for_block, context);
	if (!context.has_terminator())
	{
		if (for_stmt.iteration.not_null())
		{
			emit_bitcode(for_stmt.iteration, context);
		}
		context.builder.CreateBr(condition_check);
	}

	auto const end_bb = context.add_basic_block("endfor");
	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition, for_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
}

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
		auto const ret_val = emit_bitcode(ret_stmt.expr, context);
		if (context.current_function->return_type.is<ast::ts_reference>())
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

static void emit_bitcode(
	[[maybe_unused]] ast::stmt_no_op const & no_op_stmt,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
	// we do nothing
	return;
}

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

static void emit_bitcode(
	ast::stmt_expression const &expr_stmt,
	ctx::bitcode_context &context
)
{
	emit_bitcode(expr_stmt.expr, context);
}

static void emit_bitcode(
	ast::decl_variable const &var_decl,
	ctx::bitcode_context &context
)
{
	auto const val_ptr = context.get_variable(&var_decl);
	if (val_ptr == nullptr)
	{
		bz_assert(var_decl.var_type.is<ast::ts_reference>());
		bz_assert(var_decl.init_expr.not_null());
		auto const init_val = emit_bitcode(var_decl.init_expr, context);
		bz_assert(init_val.kind == val_ptr::reference);
		context.add_variable(&var_decl, init_val.val);
	}
	else if (var_decl.init_expr.not_null())
	{
		auto const init_val = emit_bitcode(var_decl.init_expr, context).get_value(context);
		context.builder.CreateStore(init_val, val_ptr);
	}
}

static void emit_alloca(
	ast::expression const &expr,
	ctx::bitcode_context &context
)
{
	switch (expr.kind())
	{
	default:
		break;
	}
}

static void emit_alloca(
	ast::statement const &stmt,
	ctx::bitcode_context &context
)
{
	switch (stmt.kind())
	{
	case ast::statement::index<ast::stmt_if>:
	{
		auto &if_stmt = *stmt.get<ast::stmt_if_ptr>();
		emit_alloca(if_stmt.condition, context);
		emit_alloca(if_stmt.then_block, context);
		if (if_stmt.else_block.has_value())
		{
			emit_alloca(*if_stmt.else_block, context);
		}
		break;
	}
	case ast::statement::index<ast::stmt_while>:
	{
		auto &while_stmt = *stmt.get<ast::stmt_while_ptr>();
		emit_alloca(while_stmt.condition, context);
		emit_alloca(while_stmt.while_block, context);
		break;
	}
	case ast::statement::index<ast::stmt_for>:
	{
		auto &for_stmt = *stmt.get<ast::stmt_for_ptr>();
		emit_alloca(for_stmt.init, context);
		emit_alloca(for_stmt.condition, context);
		emit_alloca(for_stmt.iteration, context);
		emit_alloca(for_stmt.for_block, context);
		break;
	}
	case ast::statement::index<ast::stmt_return>:
		emit_alloca(stmt.get<ast::stmt_return_ptr>()->expr, context);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		break;
	case ast::statement::index<ast::stmt_compound>:
	{
		auto &comp_stmt = *stmt.get<ast::stmt_compound_ptr>();
		for (auto &s : comp_stmt.statements)
		{
			emit_alloca(s, context);
		}
		break;
	}
	case ast::statement::index<ast::stmt_expression>:
		emit_alloca(stmt.get<ast::stmt_expression_ptr>()->expr, context);
		break;
	case ast::statement::index<ast::decl_variable>:
	{
		auto &var_decl = *stmt.get<ast::decl_variable_ptr>();
		if (var_decl.var_type.is<ast::ts_reference>())
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
		emit_alloca(var_decl.init_expr, context);
		break;
	}
	default:
		break;
	}
}

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
	case ast::statement::index<ast::stmt_if>:
		emit_bitcode(*stmt.get<ast::stmt_if_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_while>:
		emit_bitcode(*stmt.get<ast::stmt_while_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_for>:
		emit_bitcode(*stmt.get<ast::stmt_for_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_return>:
		emit_bitcode(*stmt.get<ast::stmt_return_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		emit_bitcode(*stmt.get<ast::stmt_no_op_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_compound>:
		emit_bitcode(*stmt.get<ast::stmt_compound_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_expression>:
		emit_bitcode(*stmt.get<ast::stmt_expression_ptr>(), context);
		break;
	case ast::statement::index<ast::stmt_static_assert>:
		// nothing
		break;

	case ast::statement::index<ast::decl_variable>:
		emit_bitcode(*stmt.get<ast::decl_variable_ptr>(), context);
		break;

	case ast::statement::index<ast::decl_function>:
	case ast::statement::index<ast::decl_operator>:
	case ast::statement::index<ast::decl_struct>:
		break;
	default:
		bz_assert(false);
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
		return context.get_built_in_type(base_t.info->kind);

	case ast::type_info::null_t_:
	case ast::type_info::aggregate:
	default:
		bz_assert(false);
		return nullptr;
	}
}

static llvm::Type *get_llvm_type(ast::typespec const &ts, ctx::bitcode_context &context)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_base_type>:
		return get_llvm_base_type(*ts.get<ast::ts_base_type_ptr>(), context);
	case ast::typespec::index<ast::ts_void>:
		return llvm::Type::getVoidTy(context.get_llvm_context());
	case ast::typespec::index<ast::ts_constant>:
		return get_llvm_type(ts.get<ast::ts_constant_ptr>()->base, context);
	case ast::typespec::index<ast::ts_pointer>:
	{
		auto const base = get_llvm_type(ts.get<ast::ts_pointer_ptr>()->base, context);
		return llvm::PointerType::get(base, 0);
	}
	case ast::typespec::index<ast::ts_reference>:
	{
		auto const base = get_llvm_type(ts.get<ast::ts_reference_ptr>()->base, context);
		return llvm::PointerType::get(base, 0);
	}
	case ast::typespec::index<ast::ts_function>:
	{
		auto &fn = *ts.get<ast::ts_function_ptr>();
		auto const result_t = get_llvm_type(fn.return_type, context);
		bz::vector<llvm::Type *> args = {};
		for (auto &a : fn.argument_types)
		{
			args.push_back(get_llvm_type(a, context));
		}
		return llvm::PointerType::get(
			llvm::FunctionType::get(result_t, llvm::ArrayRef(args.data(), args.size()), false),
			0
		);
	}
	case ast::typespec::index<ast::ts_tuple>:
	default:
		bz_assert(false);
		return nullptr;
	}
}

llvm::Function *create_function_from_symbol(
	ast::function_body &func_body,
	bz::u8string_view id,
	ctx::bitcode_context &context
)
{
	bz_assert(func_body.llvm_func == nullptr);
	auto const result_t = get_llvm_type(func_body.return_type, context);
	bz::vector<llvm::Type *> args = {};
	for (auto &p : func_body.params)
	{
		auto const t = get_llvm_type(p.var_type, context);
		auto &var_t = ast::remove_const(p.var_type);
		if (
			!(var_t.is<ast::ts_base_type>()
			&& var_t.get<ast::ts_base_type_ptr>()->info->kind == ast::type_info::str_)
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
	auto const name = llvm::StringRef(id.data(), id.size());
	auto const fn = llvm::Function::Create(func_t, llvm::Function::ExternalLinkage, name, context.get_module());
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

void emit_function_bitcode(
	ast::function_body &func_body,
	ctx::bitcode_context &context
)
{
	auto const fn = context.get_function(&func_body);
	bz_assert(fn != nullptr);
	bz_assert(fn->size() == 0);

	context.current_function = &func_body;
	auto const bb = context.add_basic_block("entry");
	context.builder.SetInsertPoint(bb);

	bz_assert(func_body.body.has_value());
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
			if (p.var_type.is<ast::ts_reference>())
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
	for (auto &stmt : *func_body.body)
	{
		emit_alloca(stmt, context);
	}

	// initialization of function parameter alloca's
	{
		auto p_it = func_body.params.begin();
		auto fn_it = fn->arg_begin();
		auto const fn_end = fn->arg_end();
		for (; fn_it != fn_end; ++fn_it, ++p_it)
		{
			auto &p = *p_it;
			if (!p.var_type.is<ast::ts_reference>())
			{
				auto const val = context.get_variable(&p);
				bz_assert(val != nullptr);
				auto const param_val = fn_it;
				context.builder.CreateStore(param_val, val);
			}
		}
	}

	// code emission for statements
	for (auto &stmt : *func_body.body)
	{
		emit_bitcode(stmt, context);
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
		bz::print(
			"{}verifyFunction failed on {}!!!\n{}",
			colors::bright_red,
			bz::u8string_view(fn->getName().data(), fn->getName().data() + fn->getName().size()),
			colors::clear
		);
	}
	context.current_function = {};
}

} // namespace bc
