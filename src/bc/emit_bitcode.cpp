#include "emit_bitcode.h"
#include "llvm/IR/Verifier.h"

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

static llvm::Type *get_llvm_type(ast::typespec const &ts, ctx::bitcode_context &context);

// ================================================================
// -------------------------- expression --------------------------
// ================================================================

static val_ptr emit_bitcode(
	ast::expression const &expr,
	ctx::bitcode_context &context
);

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
	ast::expr_tuple const &tuple,
	ctx::bitcode_context &context
)
{
	assert(false);
	return {};
}

static val_ptr emit_bitcode(
	ast::expr_unary_op const &expr,
	ctx::bitcode_context &context
)
{
	assert(false);
	return {};
}

static val_ptr emit_bitcode(
	ast::expr_binary_op const &expr,
	ctx::bitcode_context &context
)
{
	assert(false);
	return {};
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
	ast::stmt_for const &for_stmt,
	ctx::bitcode_context &context
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
	if (var_decl.init_expr.has_value())
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
	auto const result_t = get_llvm_type(func.return_type, context);
	bz::vector<llvm::Type *> args = {};
	for (auto &p : func.params)
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
	for (auto &stmt : func.body)
	{
		emit_alloca(stmt, context);
	}
	for (auto &stmt : func.body)
	{
		emit_bitcode(stmt, context);
	}
	llvm::verifyFunction(*fn);
	context.current_function = nullptr;
}

} // namespace bc
