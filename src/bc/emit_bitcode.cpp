#include "emit_bitcode.h"
#include "ctx/built_in_operators.h"

#include <llvm/IR/Verifier.h>

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
};

static llvm::Value *get_value(
	val_ptr val,
	ctx::bitcode_context &context
)
{
	if (val.kind == val_ptr::reference)
	{
		auto const loaded_val = context.builder.CreateLoad(val.val, "load_tmp");
		return loaded_val;
	}
	else
	{
		return val.val;
	}
}

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
	assert(lhs.expr_type.expr_type.is<ast::ts_base_type>());
	assert(rhs.expr_type.expr_type.is<ast::ts_base_type>());

	auto const lhs_val = get_value(emit_bitcode(lhs, context), context);
	auto const rhs_val = get_value(emit_bitcode(rhs, context), context);

	auto const lhs_kind = lhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
	auto const rhs_kind = rhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;

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
		else if (ctx::is_floating_point_kind(lhs_kind))
		{
			auto const rhs_cast = context.builder.CreateFPCast(rhs_val, lhs_val->getType(), "cast_tmp");
			return { lhs_val, rhs_cast };
		}
		else
		{
			assert(false);
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
			assert(false);
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
	assert(id.decl.index() == id.decl.index_of<ast::decl_variable const *>);
	auto const var_decl = id.decl.get<ast::decl_variable const *>();
	return { val_ptr::reference, context.get_variable_val(var_decl) };
}

static val_ptr emit_bitcode(
	ast::expr_literal const &literal,
	ctx::bitcode_context &context
)
{
	switch (literal.value.index())
	{
	case ast::expr_literal::integer_number:
		return {
			val_ptr::value,
			llvm::ConstantInt::get(
				llvm::Type::getInt32Ty(context.llvm_context),
				literal.value.get<ast::expr_literal::integer_number>()
			)
		};
	case ast::expr_literal::floating_point_number:
		return {
			val_ptr::value,
			llvm::ConstantFP::get(
				llvm::Type::getDoubleTy(context.llvm_context),
				literal.value.get<ast::expr_literal::floating_point_number>()
			)
		};
	case ast::expr_literal::string:
		assert(false);
		return {};
	case ast::expr_literal::character:
		return {
			val_ptr::value,
			llvm::ConstantInt::get(
				llvm::Type::getInt32Ty(context.llvm_context),
				literal.value.get<ast::expr_literal::character>()
			)
		};
	case ast::expr_literal::bool_true:
		return {
			val_ptr::value,
			llvm::ConstantInt::get(
				llvm::Type::getInt1Ty(context.llvm_context),
				1ull
			)
		};
	case ast::expr_literal::bool_false:
		return {
			val_ptr::value,
			llvm::ConstantInt::get(
				llvm::Type::getInt1Ty(context.llvm_context),
				0ull
			)
		};
	case ast::expr_literal::null:
	default:
		assert(false);
		return {};
	}
}

static val_ptr emit_bitcode(
	[[maybe_unused]] ast::expr_tuple const &tuple,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
	assert(false);
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
		assert(val.kind == val_ptr::reference);
		return { val_ptr::value, val.val };
	}
	case lex::token::kw_sizeof:          // 'sizeof'
		assert(false);
		return {};

	// ==== overloadable ====
	case lex::token::plus:               // '+'
	{
		assert(unary_op.op_body == nullptr);
		auto const val = get_value(emit_bitcode(unary_op.expr, context), context);
		return { val_ptr::value, val };
	}
	case lex::token::minus:              // '-'
	{
		assert(unary_op.op_body == nullptr);
		auto const val = get_value(emit_bitcode(unary_op.expr, context), context);
		auto const res = context.builder.CreateNeg(val, "unary_minus_tmp");
		return { val_ptr::value, res };
	}
	case lex::token::dereference:        // '*'
	{
		assert(unary_op.op_body == nullptr);
		auto const val = get_value(emit_bitcode(unary_op.expr, context), context);
		return { val_ptr::reference, val };
	}
	case lex::token::bit_not:            // '~'
	case lex::token::bool_not:           // '!'
	{
		assert(unary_op.op_body == nullptr);
		auto const val = get_value(emit_bitcode(unary_op.expr, context), context);
		auto const res = context.builder.CreateNot(val, "unary_bit_not_tmp");
		return { val_ptr::value, res };
	}

	case lex::token::plus_plus:          // '++'
	case lex::token::minus_minus:        // '--'
	default:
		assert(false);
		return {};
	}
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
	{
		assert(binary_op.op_body == nullptr);
		auto const lhs_val = emit_bitcode(binary_op.lhs, context);
		assert(lhs_val.kind == val_ptr::reference);
		auto rhs_val = get_value(emit_bitcode(binary_op.rhs, context), context);
		auto &rhs_t = ast::remove_const(binary_op.rhs.expr_type.expr_type);
		if (rhs_t.is<ast::ts_base_type>())
		{
			auto const rhs_kind = rhs_t.get<ast::ts_base_type_ptr>()->info->kind;
			if (
				rhs_kind >= ast::type_info::type_kind::int8_
				&& rhs_kind <= ast::type_info::type_kind::int64_
			)
			{
				rhs_val = context.builder.CreateIntCast(
					rhs_val,
					lhs_val.val->getType()->getPointerElementType(),
					true,
					"cast_tmp"
				);
			}
			else if (
				rhs_kind >= ast::type_info::type_kind::uint8_
				&& rhs_kind <= ast::type_info::type_kind::uint64_
			)
			{
				rhs_val = context.builder.CreateIntCast(
					rhs_val,
					lhs_val.val->getType()->getPointerElementType(),
					false,
					"cast_tmp"
				);
			}
			else if (
				rhs_kind == ast::type_info::type_kind::float32_
				|| rhs_kind == ast::type_info::type_kind::float64_
			)
			{
				rhs_val = context.builder.CreateFPCast(
					rhs_val,
					lhs_val.val->getType()->getPointerElementType(),
					"cast_tmp"
				);
			}
		}
		context.builder.CreateStore(rhs_val, lhs_val.val);
		return lhs_val;
	}
	case lex::token::plus:               // '+'
	{
		assert(binary_op.op_body == nullptr);
		if (
			binary_op.lhs.expr_type.expr_type.is<ast::ts_base_type>()
			&& binary_op.rhs.expr_type.expr_type.is<ast::ts_base_type>()
		)
		{
			auto const lhs_kind = binary_op.lhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			auto const rhs_kind = binary_op.rhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			if (ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind))
			{
				auto const [lhs_val, rhs_val] = get_common_type_vals(binary_op.lhs, binary_op.rhs, context);
				auto const res = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
				return { val_ptr::value, res };
			}
			else
			{
				assert(false);
				return {};
			}
		}
		else
		{
			assert(false);
			return {};
		}
	}
	case lex::token::minus:              // '-'
	{
		assert(binary_op.op_body == nullptr);
		if (
			binary_op.lhs.expr_type.expr_type.is<ast::ts_base_type>()
			&& binary_op.rhs.expr_type.expr_type.is<ast::ts_base_type>()
		)
		{
			auto const lhs_kind = binary_op.lhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			auto const rhs_kind = binary_op.rhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			if (ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind))
			{
				auto const [lhs_val, rhs_val] = get_common_type_vals(binary_op.lhs, binary_op.rhs, context);
				auto const res = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
				return { val_ptr::value, res };
			}
			else
			{
				assert(false);
				return {};
			}
		}
		else
		{
			assert(false);
			return {};
		}
	}
	case lex::token::multiply:           // '*'
	{
		assert(binary_op.op_body == nullptr);
		if (
			binary_op.lhs.expr_type.expr_type.is<ast::ts_base_type>()
			&& binary_op.rhs.expr_type.expr_type.is<ast::ts_base_type>()
		)
		{
			auto const lhs_kind = binary_op.lhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			auto const rhs_kind = binary_op.rhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			assert(ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind));
			auto const [lhs_val, rhs_val] = get_common_type_vals(binary_op.lhs, binary_op.rhs, context);
			auto const res = context.builder.CreateMul(lhs_val, rhs_val, "mul_tmp");
			return { val_ptr::value, res };
		}
		else
		{
			assert(false);
			return {};
		}
	}
	case lex::token::divide:             // '/'
	{
		assert(binary_op.op_body == nullptr);
		if (
			binary_op.lhs.expr_type.expr_type.is<ast::ts_base_type>()
			&& binary_op.rhs.expr_type.expr_type.is<ast::ts_base_type>()
		)
		{
			auto const lhs_kind = binary_op.lhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			auto const rhs_kind = binary_op.rhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			assert(ctx::is_arithmetic_kind(lhs_kind) && ctx::is_arithmetic_kind(rhs_kind));
			auto const [lhs_val, rhs_val] = get_common_type_vals(binary_op.lhs, binary_op.rhs, context);
			if (ctx::is_signed_integer_kind(lhs_kind))
			{
				auto const res = context.builder.CreateSDiv(lhs_val, rhs_val, "div_tmp");
				return { val_ptr::value, res };
			}
			else if (ctx::is_unsigned_integer_kind(lhs_kind))
			{
				auto const res = context.builder.CreateUDiv(lhs_val, rhs_val, "div_tmp");
				return { val_ptr::value, res };
			}
			else // if (ctx::is_floating_point_kind(lhs_kind))
			{
				auto const res = context.builder.CreateFDiv(lhs_val, rhs_val, "div_tmp");
				return { val_ptr::value, res };
			}
		}
		else
		{
			assert(false);
			return {};
		}
	}
	case lex::token::modulo:             // '%'
	{
		assert(binary_op.op_body == nullptr);
		if (
			binary_op.lhs.expr_type.expr_type.is<ast::ts_base_type>()
			&& binary_op.rhs.expr_type.expr_type.is<ast::ts_base_type>()
		)
		{
			auto const lhs_kind = binary_op.lhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			auto const rhs_kind = binary_op.rhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			assert(ctx::is_integer_kind(lhs_kind) && ctx::is_integer_kind(rhs_kind));
			auto const [lhs_val, rhs_val] = get_common_type_vals(binary_op.lhs, binary_op.rhs, context);
			if (ctx::is_signed_integer_kind(lhs_kind))
			{
				auto const res = context.builder.CreateSRem(lhs_val, rhs_val, "mod_tmp");
				return { val_ptr::value, res };
			}
			else // if(ctx::is_unsigned_integer_kind(lhs_kind))
			{
				auto const res = context.builder.CreateURem(lhs_val, rhs_val, "mod_tmp");
				return { val_ptr::value, res };
			}
		}
		else
		{
			assert(false);
			return {};
		}
	}
	case lex::token::bit_and:            // '&'
	{
		assert(binary_op.op_body == nullptr);
		if (
			binary_op.lhs.expr_type.expr_type.is<ast::ts_base_type>()
			&& binary_op.rhs.expr_type.expr_type.is<ast::ts_base_type>()
		)
		{
			auto const lhs_kind = binary_op.lhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			auto const rhs_kind = binary_op.rhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
			auto const [lhs_val, rhs_val] = get_common_type_vals(binary_op.lhs, binary_op.rhs, context);
			auto const res = context.builder.CreateAnd(lhs_val, rhs_val, "and_tmp");
			return { val_ptr::value, res };
		}
		else
		{
			assert(false);
			return {};
		}
	}
	case lex::token::bit_xor:            // '^'
	{
		assert(binary_op.op_body == nullptr);
		if (
			binary_op.lhs.expr_type.expr_type.is<ast::ts_base_type>()
			&& binary_op.rhs.expr_type.expr_type.is<ast::ts_base_type>()
		)
		{
			auto const lhs_kind = binary_op.lhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			auto const rhs_kind = binary_op.rhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
			auto const [lhs_val, rhs_val] = get_common_type_vals(binary_op.lhs, binary_op.rhs, context);
			auto const res = context.builder.CreateXor(lhs_val, rhs_val, "xor_tmp");
			return { val_ptr::value, res };
		}
		else
		{
			assert(false);
			return {};
		}
	}
	case lex::token::bit_or:             // '|'
	{
		assert(binary_op.op_body == nullptr);
		if (
			binary_op.lhs.expr_type.expr_type.is<ast::ts_base_type>()
			&& binary_op.rhs.expr_type.expr_type.is<ast::ts_base_type>()
		)
		{
			auto const lhs_kind = binary_op.lhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			auto const rhs_kind = binary_op.rhs.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind;
			assert(ctx::is_unsigned_integer_kind(lhs_kind) && ctx::is_unsigned_integer_kind(rhs_kind));
			auto const [lhs_val, rhs_val] = get_common_type_vals(binary_op.lhs, binary_op.rhs, context);
			auto const res = context.builder.CreateOr(lhs_val, rhs_val, "or_tmp");
			return { val_ptr::value, res };
		}
		else
		{
			assert(false);
			return {};
		}
	}

	case lex::token::plus_eq:            // '+='
	case lex::token::minus_eq:           // '-='
	case lex::token::multiply_eq:        // '*='
	case lex::token::divide_eq:          // '/='
	case lex::token::modulo_eq:          // '%='
	case lex::token::dot_dot:            // '..'
	case lex::token::dot_dot_eq:         // '..='
	case lex::token::equals:             // '=='
	case lex::token::not_equals:         // '!='
	case lex::token::less_than:          // '<'
	case lex::token::less_than_eq:       // '<='
	case lex::token::greater_than:       // '>'
	case lex::token::greater_than_eq:    // '>='
	case lex::token::bit_and_eq:         // '&='
	case lex::token::bit_xor_eq:         // '^='
	case lex::token::bit_or_eq:          // '|='
	case lex::token::bit_left_shift:     // '<<'
	case lex::token::bit_left_shift_eq:  // '<<='
	case lex::token::bit_right_shift:    // '>>'
	case lex::token::bit_right_shift_eq: // '>>='
	case lex::token::bool_and:           // '&&'
	case lex::token::bool_xor:           // '^^'
	case lex::token::bool_or:            // '||'
//	case lex::token::arrow:              // '->' ???
	case lex::token::square_open:        // '[]'

	default:
		assert(false);
		return {};
	}
}

static val_ptr emit_bitcode(
	ast::expr_function_call const &expr,
	ctx::bitcode_context &context
)
{
	assert(false);
	return {};
}

static val_ptr emit_bitcode(
	ast::expression const &expr,
	ctx::bitcode_context &context
)
{
	switch (expr.kind())
	{
	case ast::expression::index<ast::expr_identifier>:
		return emit_bitcode(*expr.get<ast::expr_identifier_ptr>(), context);
	case ast::expression::index<ast::expr_literal>:
		return emit_bitcode(*expr.get<ast::expr_literal_ptr>(), context);
	case ast::expression::index<ast::expr_tuple>:
		return emit_bitcode(*expr.get<ast::expr_tuple_ptr>(), context);
	case ast::expression::index<ast::expr_unary_op>:
		return emit_bitcode(*expr.get<ast::expr_unary_op_ptr>(), context);
	case ast::expression::index<ast::expr_binary_op>:
		return emit_bitcode(*expr.get<ast::expr_binary_op_ptr>(), context);
	case ast::expression::index<ast::expr_function_call>:
		return emit_bitcode(*expr.get<ast::expr_function_call_ptr>(), context);

	default:
		assert(false);
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
	auto const condition = get_value(emit_bitcode(if_stmt.condition, context), context);
	assert(condition->getType()->isIntegerTy() && condition->getType()->getIntegerBitWidth() == 1);
	auto const before_then_block_end = context.builder.GetInsertBlock();

	auto const then_block = context.add_basic_block("then_block");
	context.builder.SetInsertPoint(then_block);
	emit_bitcode(if_stmt.then_block, context);
	auto const then_block_end = context.builder.GetInsertBlock();

	auto const else_block = if_stmt.else_block.has_value() ? context.add_basic_block("else_block") : nullptr;
	if (else_block)
	{
		context.builder.SetInsertPoint(else_block);
		emit_bitcode(*if_stmt.else_block, context);
	}
	auto const else_block_end = else_block ? context.builder.GetInsertBlock() : nullptr;

	auto const after_if = context.add_basic_block("after_if");
	context.builder.SetInsertPoint(before_then_block_end);
	context.builder.CreateCondBr(condition, then_block, else_block ? else_block : after_if);
	context.builder.SetInsertPoint(then_block_end);
	context.builder.CreateBr(after_if);
	if (else_block_end)
	{
		context.builder.SetInsertPoint(else_block_end);
		context.builder.CreateBr(after_if);
	}

	context.builder.SetInsertPoint(after_if);
}

static void emit_bitcode(
	ast::stmt_while const &while_stmt,
	ctx::bitcode_context &context
)
{
	auto const condition_check = context.add_basic_block("while_condition_check");
	context.builder.CreateBr(condition_check);
	context.builder.SetInsertPoint(condition_check);
	auto const condition = get_value(emit_bitcode(while_stmt.condition, context), context);
	auto const condition_check_end = context.builder.GetInsertBlock();

	auto const while_block = context.add_basic_block("while_block");
	context.builder.SetInsertPoint(while_block);
	emit_bitcode(while_stmt.while_block, context);
	context.builder.CreateBr(condition_check);

	auto const after_while = context.add_basic_block("after_while");
	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition, while_block, after_while);
	context.builder.SetInsertPoint(after_while);
}

static void emit_bitcode(
	[[maybe_unused]] ast::stmt_for const &for_stmt,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
	assert(false);
}

static void emit_bitcode(
	ast::stmt_return const &ret_stmt,
	ctx::bitcode_context &context
)
{
	if (ret_stmt.expr.kind() == ast::expression::null)
	{
		context.builder.CreateRetVoid();
	}
	else
	{
		auto const ret_val = get_value(
			emit_bitcode(ret_stmt.expr, context),
			context
		);
		context.builder.CreateRet(ret_val);
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
	auto const val_ptr = std::find_if(
		context.vars.begin(), context.vars.end(),
		[ptr = &var_decl](auto const pair) {
			return ptr == pair.first;
		}
	);
	if (val_ptr == context.vars.end())
	{
		assert(var_decl.var_type.is<ast::ts_reference>());
		assert(var_decl.init_expr.has_value());
		auto const init_val = emit_bitcode(*var_decl.init_expr, context);
		assert(init_val.kind == val_ptr::reference);
		context.vars.push_back({ &var_decl, init_val.val });
	}
	else if (var_decl.init_expr.has_value())
	{
		auto const init_val = get_value(
			emit_bitcode(*var_decl.init_expr, context),
			context
		);
		context.builder.CreateStore(init_val, val_ptr->second);
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
		emit_alloca(if_stmt.then_block, context);
		if (if_stmt.else_block.has_value())
		{
			emit_alloca(*if_stmt.else_block, context);
		}
		break;
	}
	case ast::statement::index<ast::stmt_while>:
		emit_alloca(stmt.get<ast::stmt_while_ptr>()->while_block, context);
		break;
	case ast::statement::index<ast::stmt_for>:
		break;
	case ast::statement::index<ast::stmt_return>:
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
		break;
	case ast::statement::index<ast::decl_variable>:
	{
		auto &var_decl = *stmt.get<ast::decl_variable_ptr>();
		if (var_decl.var_type.is<ast::ts_reference>())
		{
			return;
		}
		auto const var_t = get_llvm_type(var_decl.var_type, context);
		auto const name = llvm::StringRef(var_decl.identifier->value.data(), var_decl.identifier->value.length());
		auto const alloca = context.builder.CreateAlloca(var_t, nullptr, name);
		context.vars.push_back({ &var_decl, alloca });
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

	case ast::statement::index<ast::decl_variable>:
		emit_bitcode(*stmt.get<ast::decl_variable_ptr>(), context);
		break;

	case ast::statement::index<ast::decl_function>:
	case ast::statement::index<ast::decl_operator>:
	case ast::statement::index<ast::decl_struct>:
		break;
	default:
		assert(false);
		break;
	}
}

static llvm::Type *get_llvm_base_type(ast::ts_base_type const &base_t, ctx::bitcode_context &context)
{
	switch (base_t.info->kind)
	{
	case ast::type_info::type_kind::int8_:
	case ast::type_info::type_kind::uint8_:
		return llvm::Type::getInt8Ty(context.llvm_context);
	case ast::type_info::type_kind::int16_:
	case ast::type_info::type_kind::uint16_:
		return llvm::Type::getInt16Ty(context.llvm_context);
	case ast::type_info::type_kind::int32_:
	case ast::type_info::type_kind::uint32_:
		return llvm::Type::getInt32Ty(context.llvm_context);
	case ast::type_info::type_kind::int64_:
	case ast::type_info::type_kind::uint64_:
		return llvm::Type::getInt64Ty(context.llvm_context);
	case ast::type_info::type_kind::float32_:
		return llvm::Type::getFloatTy(context.llvm_context);
	case ast::type_info::type_kind::float64_:
		return llvm::Type::getDoubleTy(context.llvm_context);
	case ast::type_info::type_kind::char_:
		return llvm::Type::getInt32Ty(context.llvm_context);
	case ast::type_info::type_kind::str_:
		assert(false);
		return nullptr;
	case ast::type_info::type_kind::bool_:
		return llvm::Type::getInt1Ty(context.llvm_context);
	case ast::type_info::type_kind::null_t_:
	case ast::type_info::type_kind::aggregate:
	default:
		assert(false);
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
		return llvm::Type::getVoidTy(context.llvm_context);
	case ast::typespec::index<ast::ts_constant>:
		return get_llvm_type(ts.get<ast::ts_constant_ptr>()->base, context);
	case ast::typespec::index<ast::ts_pointer>:
	{
		auto base = get_llvm_type(ts.get<ast::ts_pointer_ptr>()->base, context);
		return llvm::PointerType::get(base, 0);
	}
	case ast::typespec::index<ast::ts_reference>:
	{
		auto base = get_llvm_type(ts.get<ast::ts_reference_ptr>()->base, context);
		return llvm::PointerType::get(base, 0);
	}
	case ast::typespec::index<ast::ts_function>:
	{
		auto &fn = *ts.get<ast::ts_function_ptr>();
		auto result_t = get_llvm_type(fn.return_type, context);
		bz::vector<llvm::Type *> args = {};
		for (auto &a : fn.argument_types)
		{
			args.push_back(get_llvm_type(a, context));
		}
		return llvm::FunctionType::get(result_t, llvm::ArrayRef(args.data(), args.size()), false);
	}
	case ast::typespec::index<ast::ts_tuple>:
	default:
		assert(false);
		return nullptr;
	}
}

llvm::Function *get_function_decl_bitcode(ast::decl_function &func, ctx::bitcode_context &context)
{
	auto const result_t = get_llvm_type(func.body.return_type, context);
	bz::vector<llvm::Type *> args = {};
	for (auto &p : func.body.params)
	{
		args.push_back(get_llvm_type(p.var_type, context));
	}
	auto const func_t = llvm::FunctionType::get(result_t, llvm::ArrayRef(args.data(), args.size()), false);
	auto const name = llvm::StringRef(func.identifier->value.data(), func.identifier->value.length());
	return llvm::Function::Create(func_t, llvm::Function::ExternalLinkage, name, context.module);
}

void emit_function_bitcode(ast::decl_function &func, ctx::bitcode_context &context)
{
	auto const fn = get_function_decl_bitcode(func, context);
	context.current_function = fn;
	auto const bb = context.add_basic_block("entry");
	context.builder.SetInsertPoint(bb);
	for (auto &stmt : func.body.body)
	{
		emit_alloca(stmt, context);
	}
	for (auto &stmt : func.body.body)
	{
		emit_bitcode(stmt, context);
	}
	if (llvm::verifyFunction(*fn) == true)
	{
		bz::print("verifyFunction failed!!!\n");
	}
	context.current_function = nullptr;
}

} // namespace bc
