#include "emit_bitcode.h"
#include "global_data.h"
#include "colors.h"
#include "ctx/global_context.h"

#include <llvm/IR/Verifier.h>

namespace codegen::llvm_latest
{

constexpr size_t array_loop_threshold = 16;

static llvm::Constant *get_value(
	ast::constant_value const &value,
	ast::typespec_view type,
	ast::constant_expression const *const_expr,
	bitcode_context &context
);

static val_ptr emit_bitcode(
	ast::expression const &expr,
	bitcode_context &context,
	llvm::Value *result_address
);

static void emit_bitcode(
	ast::statement const &stmt,
	bitcode_context &context
);

static int get_unique_id(void)
{
	static int i = 0;
	return i++;
}

struct src_tokens_llvm_value_t
{
	llvm::Constant *begin;
	llvm::Constant *pivot;
	llvm::Constant *end;
};

struct is_byval_and_type_pair
{
	bool is_byval;
	llvm::Type *type;
};

static val_ptr value_or_result_address(llvm::Value *value, llvm::Value *result_address, bitcode_context &context)
{
	if (result_address != nullptr)
	{
		auto const result_type = value->getType();
		context.builder.CreateStore(value, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
	else
	{
		return val_ptr::get_value(value);
	}
}

static void emit_memcpy(llvm::Value *dest, llvm::Value *source, size_t size, bitcode_context &context)
{
	context.builder.CreateMemCpy(dest, std::nullopt, source, std::nullopt, size);
}

static void emit_value_copy(val_ptr value, llvm::Value *dest_ptr, bitcode_context &context)
{
	if (value.kind == val_ptr::value || !value.get_type()->isAggregateType())
	{
		context.builder.CreateStore(value.get_value(context.builder), dest_ptr);
	}
	else
	{
		emit_memcpy(dest_ptr, value.val, context.get_size(value.get_type()), context);
	}
}

template<bool push_to_front = false>
static void add_call_parameter(
	ast::typespec_view param_type,
	llvm::Type *param_llvm_type,
	val_ptr param,
	ast::arena_vector<llvm::Value *> &params,
	ast::arena_vector<is_byval_and_type_pair> &params_is_byval,
	bitcode_context &context
)
{
	using params_push_type = llvm::Value *&(ast::arena_vector<llvm::Value *>::*)(llvm::Value * const &);
	using byval_push_type = is_byval_and_type_pair &(ast::arena_vector<is_byval_and_type_pair>::*)(is_byval_and_type_pair const &);
	constexpr auto params_push = push_to_front
		? static_cast<params_push_type>(&ast::arena_vector<llvm::Value *>::push_front)
		: static_cast<params_push_type>(&ast::arena_vector<llvm::Value *>::push_back);
	constexpr auto byval_push = push_to_front
		? static_cast<byval_push_type>(&ast::arena_vector<is_byval_and_type_pair>::push_front)
		: static_cast<byval_push_type>(&ast::arena_vector<is_byval_and_type_pair>::push_back);
	if (param_type.is<ast::ts_lvalue_reference>() || param_type.is<ast::ts_move_reference>())
	{
		bz_assert(param.kind == val_ptr::reference);
		(params.*params_push)(param.val);
		(params_is_byval.*byval_push)({ false, nullptr });
	}
	// special case for *void and *mut void
	else if (param_type.remove_mut_pointer().is<ast::ts_void>())
	{
		auto const void_ptr_val = param.get_value(context.builder);
		bz_assert(void_ptr_val->getType()->isPointerTy());
		(params.*params_push)(void_ptr_val);
		(params_is_byval.*byval_push)({ false, nullptr });
	}
	else
	{
		auto const pass_kind = context.get_pass_kind(param_type, param_llvm_type);

		switch (pass_kind)
		{
		case abi::pass_kind::reference:
			if (param.kind == val_ptr::reference)
			{
				// the argument expression must be an rvalue here, meaning that the reference
				// is unique, so we can pass it safely along
				//
				// on SystemV these parameters have an extra byval attribute, which doesn't need a copy
				// in the first place.   see: https://reviews.llvm.org/D79636
				(params.*params_push)(param.val);
			}
			else
			{
				auto const alloca = context.create_alloca(param_llvm_type);
				bz_assert(param.kind == val_ptr::value);
				context.builder.CreateStore(param.get_value(context.builder), alloca);
				(params.*params_push)(alloca);
			}
			(params_is_byval.*byval_push)({ true, param_llvm_type });
			break;
		case abi::pass_kind::value:
			(params.*params_push)(param.get_value(context.builder));
			(params_is_byval.*byval_push)({ false, nullptr });
			break;
		case abi::pass_kind::one_register:
			(params.*params_push)(context.create_bitcast(
				param,
				abi::get_one_register_type(context.get_platform_abi(), param_llvm_type, context.get_data_layout(), context.get_llvm_context())
			));
			(params_is_byval.*byval_push)({ false, nullptr });
			break;
		case abi::pass_kind::two_registers:
		{
			auto const [first_type, second_type] = abi::get_two_register_types(
				context.get_platform_abi(),
				param_llvm_type,
				context.get_data_layout(),
				context.get_llvm_context()
			);
			auto const cast_val = context.create_bitcast(param, llvm::StructType::get(first_type, second_type));
			auto const first_val = context.builder.CreateExtractValue(cast_val, 0);
			auto const second_val = context.builder.CreateExtractValue(cast_val, 1);
			if constexpr (push_to_front)
			{
				params.push_front(second_val);
				params_is_byval.push_front({ false, nullptr });
				params.push_front(first_val);
				params_is_byval.push_front({ false, nullptr });
			}
			else
			{
				params.push_back(first_val);
				params_is_byval.push_back({ false, nullptr });
				params.push_back(second_val);
				params_is_byval.push_back({ false, nullptr });
			}
			break;
		}
		case abi::pass_kind::non_trivial:
			bz_assert(param.kind == val_ptr::reference);
			(params.*params_push)(param.val);
			(params_is_byval.*byval_push)({ false, nullptr });
			break;
		}
	}
}

static void add_byval_attributes(llvm::CallInst *call, llvm::Type *byval_type, unsigned index, bitcode_context &context)
{
	auto const attributes = abi::get_pass_by_reference_attributes(context.get_platform_abi());
	for (auto const attribute : attributes)
	{
		switch (attribute)
		{
		case llvm::Attribute::ByVal:
		{
			call->addParamAttr(index, llvm::Attribute::getWithByValType(context.get_llvm_context(), byval_type));
			break;
		}
		// Captures represents capture(none)
		case llvm::Attribute::Captures:
		{
			call->addParamAttr(
				index,
				llvm::Attribute::getWithCaptureInfo(context.get_llvm_context(), llvm::CaptureInfo::none())
			);
			break;
		}
		default:
			call->addParamAttr(index, attribute);
			break;
		}
	}
}

static void add_byval_attributes(llvm::Argument &arg, llvm::Type *byval_type, bitcode_context &context)
{
	auto const attributes = abi::get_pass_by_reference_attributes(context.get_platform_abi());
	for (auto const attribute : attributes)
	{
		switch (attribute)
		{
		case llvm::Attribute::ByVal:
		{
			arg.addAttr(llvm::Attribute::getWithByValType(context.get_llvm_context(), byval_type));
			break;
		}
		// Captures represents capture(none)
		case llvm::Attribute::Captures:
		{
			arg.addAttr(llvm::Attribute::getWithCaptureInfo(context.get_llvm_context(), llvm::CaptureInfo::none()));
			break;
		}
		default:
			arg.addAttr(attribute);
			break;
		}
	}
}

static void emit_panic_call(
	lex::src_tokens const &src_tokens,
	bz::u8string_view message,
	bitcode_context &context
)
{
	auto const panic_handler_func_body = context.get_builtin_function(ast::function_body::builtin_panic_handler);
	if (panic_handler_func_body == nullptr)
	{
		context.builder.CreateIntrinsic(llvm::Intrinsic::trap, {});

		auto const current_ret_type = context.current_function.second->getReturnType();
		if (current_ret_type->isVoidTy())
		{
			context.builder.CreateRetVoid();
		}
		else
		{
			context.builder.CreateRet(llvm::UndefValue::get(current_ret_type));
		}
		return;
	}

	bz_assert(panic_handler_func_body->params.size() == 1);
	bz_assert(panic_handler_func_body->params[0].get_type().is<ast::ts_base_type>());
	bz_assert(panic_handler_func_body->params[0].get_type().get<ast::ts_base_type>().info->kind == ast::type_info::str_);
	auto const panic_handler_fn = context.get_function(panic_handler_func_body);
	bz_assert(panic_handler_fn != nullptr);

	bz_assert(get_llvm_type(panic_handler_func_body->return_type, context)->isVoidTy());
	bz_assert(context.get_pass_kind(
		panic_handler_func_body->return_type,
		llvm::Type::getVoidTy(context.get_llvm_context())
	) == abi::pass_kind::value);

	ast::arena_vector<llvm::Value *> params = {};
	params.reserve(2); // on linux str is passed in two registers
	ast::arena_vector<is_byval_and_type_pair> params_is_byval = {};
	params.reserve(2);

	auto const panic_string = bz::format(
		"panic called from {}: {}",
		context.global_ctx.get_location_string(src_tokens.pivot), message
	);
	auto const param_val = get_value(
		ast::constant_value(panic_string.as_string_view()),
		panic_handler_func_body->params[0].get_type(),
		nullptr,
		context
	);
	auto const param = val_ptr::get_value(param_val);
	auto const param_type = panic_handler_func_body->params[0].get_type().as_typespec_view();
	auto const param_llvm_type = context.get_str_t();
	add_call_parameter<false>(param_type, param_llvm_type, param, params, params_is_byval, context);

	auto const call = context.create_call(panic_handler_fn, llvm::ArrayRef(params.data(), params.size()));
	auto is_byval_it = params_is_byval.begin();
	auto const is_byval_end = params_is_byval.end();
	unsigned i = 0;
	bz_assert(panic_handler_fn->arg_size() == call->arg_size());
	for (; is_byval_it != is_byval_end; ++is_byval_it, ++i)
	{
		if (is_byval_it->is_byval)
		{
			add_byval_attributes(call, is_byval_it->type, i, context);
		}
	}

	// just to be sure...
	context.builder.CreateIntrinsic(llvm::Intrinsic::trap, {});

	auto const current_ret_type = context.current_function.second->getReturnType();
	if (current_ret_type->isVoidTy())
	{
		context.builder.CreateRetVoid();
	}
	else
	{
		context.builder.CreateRet(llvm::UndefValue::get(current_ret_type));
	}
}

static llvm::Value *optional_has_value(val_ptr optional_val, bitcode_context &context)
{
	if (optional_val.get_type()->isPointerTy())
	{
		return context.builder.CreateICmpNE(
			optional_val.get_value(context.builder),
			llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(optional_val.get_type()))
		);
	}
	else if (optional_val.kind == val_ptr::value)
	{
		return context.builder.CreateExtractValue(optional_val.get_value(context.builder), 1);
	}
	else
	{
		auto const has_value_ptr = context.create_struct_gep(optional_val.get_type(), optional_val.val, 1);
		return context.builder.CreateLoad(context.get_bool_t(), has_value_ptr);
	}
}

static val_ptr optional_get_value_ptr(val_ptr optional_val, bitcode_context &context)
{
	if (optional_val.get_type()->isPointerTy())
	{
		return optional_val;
	}
	else if (optional_val.kind == val_ptr::value)
	{
		return val_ptr::get_value(context.builder.CreateExtractValue(optional_val.get_value(context.builder), 0));
	}
	else
	{
		auto const value_ptr = context.create_struct_gep(optional_val.get_type(), optional_val.val, 0);
		bz_assert(optional_val.get_type()->isStructTy());
		return val_ptr::get_reference(value_ptr, optional_val.get_type()->getStructElementType(0));
	}
}

static void optional_set_has_value(val_ptr optional_val, bool has_value, bitcode_context &context)
{
	bz_assert(optional_val.kind == val_ptr::reference);
	if (optional_val.get_type()->isPointerTy())
	{
		if (has_value == false)
		{
			context.builder.CreateStore(llvm::ConstantPointerNull::get(context.get_opaque_pointer_t()), optional_val.val);
		}
	}
	else
	{
		auto const has_value_ptr = context.create_struct_gep(optional_val.get_type(), optional_val.val, 1);
		context.builder.CreateStore(context.builder.getInt1(has_value), has_value_ptr);
	}
}

static void optional_set_has_value(val_ptr optional_val, llvm::Value *has_value, bitcode_context &context)
{
	bz_assert(optional_val.kind == val_ptr::reference);
	bz_assert(!optional_val.get_type()->isPointerTy());
	bz_assert(has_value->getType()->isIntegerTy());
	auto const has_value_ptr = context.create_struct_gep(optional_val.get_type(), optional_val.val, 1);
	context.builder.CreateStore(has_value, has_value_ptr);
}

static void emit_null_optional_get_value_check(
	lex::src_tokens const &src_tokens,
	val_ptr optional_val,
	bitcode_context &context
)
{
	if (global_data::panic_on_null_get_value)
	{
		auto const has_value = optional_has_value(optional_val, context);
		auto const begin_bb = context.builder.GetInsertBlock();
		auto const error_bb = context.add_basic_block("get_value_null_check_error");
		context.builder.SetInsertPoint(error_bb);
		emit_panic_call(src_tokens, "'get_value' called on a null optional", context);
		bz_assert(context.has_terminator());

		auto const continue_bb = context.add_basic_block("get_value_null_check_continue");
		context.builder.SetInsertPoint(begin_bb);
		context.builder.CreateCondBr(has_value, continue_bb, error_bb);
		context.builder.SetInsertPoint(continue_bb);
	}
}

static void emit_null_pointer_arithmetic_check(
	lex::src_tokens const &src_tokens,
	llvm::Value *ptr,
	bitcode_context &context
)
{
	if (global_data::panic_on_null_pointer_arithmetic)
	{
		auto const has_value = optional_has_value(val_ptr::get_value(ptr), context);
		auto const begin_bb = context.builder.GetInsertBlock();
		auto const error_bb = context.add_basic_block("arithmetic_null_check_error");
		context.builder.SetInsertPoint(error_bb);
		emit_panic_call(src_tokens, "null value used in pointer arithmetic", context);
		bz_assert(context.has_terminator());

		auto const continue_bb = context.add_basic_block("arithmetic_null_check_continue");
		context.builder.SetInsertPoint(begin_bb);
		context.builder.CreateCondBr(has_value, continue_bb, error_bb);
		context.builder.SetInsertPoint(continue_bb);
	}
}

static void emit_null_pointer_arithmetic_check(
	lex::src_tokens const &src_tokens,
	llvm::Value *ptr,
	llvm::Value *offset,
	bitcode_context &context
)
{
	if (global_data::panic_on_null_pointer_arithmetic)
	{
		auto const has_value = optional_has_value(val_ptr::get_value(ptr), context);
		auto const is_offset_zero = context.builder.CreateICmpEQ(offset, llvm::ConstantInt::get(offset->getType(), 0));

		auto const is_valid = context.builder.CreateOr(has_value, is_offset_zero);
		auto const begin_bb = context.builder.GetInsertBlock();
		auto const error_bb = context.add_basic_block("arithmetic_null_check_error");
		context.builder.SetInsertPoint(error_bb);
		emit_panic_call(src_tokens, "null value used in pointer arithmetic", context);
		bz_assert(context.has_terminator());

		auto const continue_bb = context.add_basic_block("arithmetic_null_check_continue");
		context.builder.SetInsertPoint(begin_bb);
		context.builder.CreateCondBr(is_valid, continue_bb, error_bb);
		context.builder.SetInsertPoint(continue_bb);
	}
}

template<typename ExprScopeInfoT>
struct array_init_loop_info_t
{
	llvm::BasicBlock *condition_check_bb;
	llvm::BasicBlock *end_bb;
	llvm::PHINode *iter_val;
	ExprScopeInfoT prev_info;
};

static auto create_loop_start(size_t size, bitcode_context &context)
	-> array_init_loop_info_t<typename bz::meta::remove_cv_reference<decltype(context)>::expression_scope_info_t>
{
	// create a loop
	auto const start_bb = context.builder.GetInsertBlock();
	auto const condition_check_bb = context.add_basic_block("array_init_condition_check");
	auto const loop_bb = context.add_basic_block("array_init_loop");
	auto const end_bb = context.add_basic_block("array_init_end");

	context.builder.CreateBr(condition_check_bb);
	context.builder.SetInsertPoint(condition_check_bb);
	auto const iter_val = context.builder.CreatePHI(context.get_usize_t(), 2);
	auto const zero_value = llvm::ConstantInt::get(iter_val->getType(), 0);
	iter_val->addIncoming(zero_value, start_bb);
	auto const size_value = llvm::ConstantInt::get(iter_val->getType(), size);
	auto const is_at_end = context.builder.CreateICmpEQ(iter_val, size_value);
	context.builder.CreateCondBr(is_at_end, end_bb, loop_bb);
	context.builder.SetInsertPoint(loop_bb);

	return {
		.condition_check_bb = condition_check_bb,
		.end_bb = end_bb,
		.iter_val = iter_val,
		.prev_info = context.push_expression_scope(),
	};
}

template<typename T>
static void create_loop_end(array_init_loop_info_t<T> loop_info, bitcode_context &context)
{
	context.pop_expression_scope(loop_info.prev_info);

	auto const one_value = llvm::ConstantInt::get(loop_info.iter_val->getType(), 1);
	auto const next_iter_val = context.builder.CreateAdd(loop_info.iter_val, one_value);
	context.builder.CreateBr(loop_info.condition_check_bb);
	auto const loop_end_bb = context.builder.GetInsertBlock();

	loop_info.iter_val->addIncoming(next_iter_val, loop_end_bb);
	context.builder.SetInsertPoint(loop_info.end_bb);
}

template<typename ExprScopeInfoT>
struct array_destruct_loop_info_t
{
	llvm::BasicBlock *condition_check_bb;
	llvm::BasicBlock *end_bb;
	llvm::PHINode *condition_check_iter_val;
	llvm::Value *iter_val;
	ExprScopeInfoT prev_info;
};

static auto create_reversed_loop_start(size_t size, bitcode_context &context)
	-> array_destruct_loop_info_t<typename bz::meta::remove_cv_reference<decltype(context)>::expression_scope_info_t>
{
	// create a loop
	auto const start_bb = context.builder.GetInsertBlock();
	auto const condition_check_bb = context.add_basic_block("array_init_condition_check");
	auto const loop_bb = context.add_basic_block("array_init_loop");
	auto const end_bb = context.add_basic_block("array_init_end");

	context.builder.CreateBr(condition_check_bb);
	context.builder.SetInsertPoint(condition_check_bb);
	auto const iter_val = context.builder.CreatePHI(context.get_usize_t(), 2);
	auto const zero_value = llvm::ConstantInt::get(iter_val->getType(), 0);
	auto const size_value = llvm::ConstantInt::get(iter_val->getType(), size);
	iter_val->addIncoming(size_value, start_bb);
	auto const is_at_end = context.builder.CreateICmpEQ(iter_val, zero_value);
	context.builder.CreateCondBr(is_at_end, end_bb, loop_bb);
	context.builder.SetInsertPoint(loop_bb);
	auto const one_value = llvm::ConstantInt::get(iter_val->getType(), 1);
	auto const next_iter_val = context.builder.CreateSub(iter_val, one_value);

	return {
		.condition_check_bb = condition_check_bb,
		.end_bb = end_bb,
		.condition_check_iter_val = iter_val,
		.iter_val = next_iter_val,
		.prev_info = context.push_expression_scope(),
	};
}

template<typename T>
static void create_reversed_loop_end(array_destruct_loop_info_t<T> loop_info, bitcode_context &context)
{
	context.pop_expression_scope(loop_info.prev_info);

	context.builder.CreateBr(loop_info.condition_check_bb);
	auto const loop_end_bb = context.builder.GetInsertBlock();

	loop_info.condition_check_iter_val->addIncoming(loop_info.iter_val, loop_end_bb);
	context.builder.SetInsertPoint(loop_info.end_bb);
}

// ================================================================
// -------------------------- expression --------------------------
// ================================================================

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_variable_name const &var_name,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const [ptr, type] = context.get_variable(var_name.decl);
	bz_assert(ptr != nullptr);
	bz_assert(result_address == nullptr);
	return val_ptr::get_reference(ptr, type);
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_function_name const &,
	bitcode_context &,
	llvm::Value *result_address
)
{
	bz_assert(result_address == nullptr);
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_function_alias_name const &,
	bitcode_context &,
	llvm::Value *result_address
)
{
	bz_assert(result_address == nullptr);
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_function_overload_set const &,
	bitcode_context &,
	llvm::Value *result_address
)
{
	bz_assert(result_address == nullptr);
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_struct_name const &,
	bitcode_context &,
	llvm::Value *
)
{
	// this is always a constant expression
	bz_unreachable;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_enum_name const &,
	bitcode_context &,
	llvm::Value *
)
{
	// this is always a constant expression
	bz_unreachable;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_type_alias_name const &,
	bitcode_context &,
	llvm::Value *
)
{
	// this is always a constant expression
	bz_unreachable;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_integer_literal const &,
	bitcode_context &,
	llvm::Value *
)
{
	// this is always a constant expression
	bz_unreachable;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_null_literal const &,
	bitcode_context &,
	llvm::Value *
)
{
	// this is always a constant expression
	bz_unreachable;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_enum_literal const &,
	bitcode_context &,
	llvm::Value *
)
{
	// this is always a constant expression
	bz_unreachable;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_typed_literal const &,
	bitcode_context &,
	llvm::Value *
)
{
	// this is always a constant expression
	bz_unreachable;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_placeholder_literal const &,
	bitcode_context &,
	llvm::Value *
)
{
	// this is not a valid expression at this point
	bz_unreachable;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_typename_literal const &,
	bitcode_context &,
	llvm::Value *
)
{
	// this is always a constant expression
	bz_unreachable;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_tuple const &tuple_expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const result_type = [&]() -> llvm::Type * {
		if (result_address == nullptr)
		{
			return nullptr;
		}

		auto const types = tuple_expr.elems
			.transform([](auto const &expr) { return expr.get_expr_type(); })
			.transform([&context](auto const ts) { return get_llvm_type(ts, context); })
			.template collect<ast::arena_vector>();
		return context.get_tuple_t(types);
	}();

	for (unsigned i = 0; i < tuple_expr.elems.size(); ++i)
	{
		if (result_address != nullptr)
		{
			if (tuple_expr.elems[i].get_expr_type().is_reference())
			{
				auto const elem_result_address = context.create_struct_gep(result_type, result_address, i);
				auto const result = emit_bitcode(tuple_expr.elems[i], context, nullptr);
				bz_assert(result.kind == val_ptr::reference);
				context.builder.CreateStore(result.val, elem_result_address);
			}
			else
			{
				auto const elem_result_address = context.create_struct_gep(result_type, result_address, i);
				emit_bitcode(tuple_expr.elems[i], context, elem_result_address);
			}
		}
		else
		{
			emit_bitcode(tuple_expr.elems[i], context, nullptr);
		}
	}

	return result_address == nullptr ? val_ptr::get_none() : val_ptr::get_reference(result_address, result_type);
}

static val_ptr emit_builtin_unary_address_of(
	ast::expression const &expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const val = emit_bitcode(expr, context, nullptr);
	bz_assert(val.kind == val_ptr::reference);
	bz_assert(val.val->getType()->isPointerTy());
	return value_or_result_address(val.val, result_address, context);
}

static val_ptr emit_builtin_unary_plus(
	ast::expression const &expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	return emit_bitcode(expr, context, result_address);
}

static val_ptr emit_builtin_unary_minus(
	ast::expression const &expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const [val, type] = emit_bitcode(expr, context, nullptr).get_value_and_type(context.builder);
	bz_assert(type == val->getType());
	auto const res = type->isFloatingPointTy()
		? context.builder.CreateFNeg(val, "unary_minus_tmp")
		: context.builder.CreateNeg(val, "unary_minus_tmp");
	return value_or_result_address(res, result_address, context);
}

static val_ptr emit_builtin_unary_dereference(
	lex::src_tokens const &src_tokens,
	ast::expression const &expr,
	bitcode_context &context
)
{
	auto const val = emit_bitcode(expr, context, nullptr).get_value(context.builder);
	auto const type = expr.get_expr_type();
	bz_assert(type.is<ast::ts_pointer>() || type.is_optional_pointer());
	if (type.is_optional_pointer())
	{
		if (global_data::panic_on_null_dereference)
		{
			auto const has_value = optional_has_value(val_ptr::get_value(val), context);
			auto const begin_bb = context.builder.GetInsertBlock();
			auto const error_bb = context.add_basic_block("deref_null_check_error");
			context.builder.SetInsertPoint(error_bb);
			emit_panic_call(src_tokens, "null pointer dereferenced", context);
			bz_assert(context.has_terminator());

			auto const continue_bb = context.add_basic_block("deref_null_check_continue");
			context.builder.SetInsertPoint(begin_bb);
			context.builder.CreateCondBr(has_value, continue_bb, error_bb);
			context.builder.SetInsertPoint(continue_bb);
		}

		auto const result_type = get_llvm_type(type.get_optional_pointer(), context);
		return val_ptr::get_reference(val, result_type);
	}
	else
	{
		auto const result_type = get_llvm_type(type.template get<ast::ts_pointer>(), context);
		return val_ptr::get_reference(val, result_type);
	}
}

static val_ptr emit_builtin_unary_bit_not(
	ast::expression const &expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const val = emit_bitcode(expr, context, nullptr).get_value(context.builder);
	auto const res = context.builder.CreateNot(val, "unary_bit_not_tmp");
	return value_or_result_address(res, result_address, context);
}

static val_ptr emit_builtin_unary_bool_not(
	ast::expression const &expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const val = emit_bitcode(expr, context, nullptr).get_value(context.builder);
	auto const res = context.builder.CreateNot(val, "unary_bool_not_tmp");
	return value_or_result_address(res, result_address, context);
}

static val_ptr emit_builtin_unary_plus_plus(
	ast::expression const &expr,
	bitcode_context &context
)
{
	auto const val = emit_bitcode(expr, context, nullptr);
	bz_assert(val.kind == val_ptr::reference);
	auto const [original_value, type] = val.get_value_and_type(context.builder);
	bz_assert(type == original_value->getType());
	if (type->isPointerTy())
	{
		auto const expr_type = expr.get_expr_type().get_mut_reference();
		bz_assert(expr_type.is<ast::ts_pointer>() || expr_type.is_optional_pointer());
		auto const inner_type = expr_type.is<ast::ts_pointer>()
			? get_llvm_type(expr_type.get<ast::ts_pointer>(), context)
			: get_llvm_type(expr_type.get_optional_pointer(), context);

		if (expr_type.is_optional_pointer())
		{
			emit_null_pointer_arithmetic_check(expr.src_tokens, original_value, context);
		}

		auto const incremented_value = context.create_gep(inner_type, original_value, 1);
		context.builder.CreateStore(incremented_value, val.val);
		return val;
	}
	else
	{
		bz_assert(type->isIntegerTy());
		auto const incremented_value = context.builder.CreateAdd(
			original_value,
			llvm::ConstantInt::get(type, 1)
		);
		context.builder.CreateStore(incremented_value, val.val);
		return val;
	}
}

static val_ptr emit_builtin_unary_minus_minus(
	ast::expression const &expr,
	bitcode_context &context
)
{
	auto const val = emit_bitcode(expr, context, nullptr);
	bz_assert(val.kind == val_ptr::reference);
	auto const [original_value, type] = val.get_value_and_type(context.builder);
	if (type->isPointerTy())
	{
		auto const expr_type = expr.get_expr_type().get_mut_reference();
		bz_assert(expr_type.is<ast::ts_pointer>() || expr_type.is_optional_pointer());
		auto const inner_type = expr_type.is<ast::ts_pointer>()
			? get_llvm_type(expr_type.get<ast::ts_pointer>(), context)
			: get_llvm_type(expr_type.get_optional_pointer(), context);

		if (expr_type.is_optional_pointer())
		{
			emit_null_pointer_arithmetic_check(expr.src_tokens, original_value, context);
		}

		auto const incremented_value = context.create_gep(inner_type, original_value, uint64_t(-1));
		context.builder.CreateStore(incremented_value, val.val);
		return val;
	}
	else
	{
		bz_assert(type->isIntegerTy());
		auto const incremented_value = context.builder.CreateAdd(
			original_value,
			llvm::ConstantInt::get(type, uint64_t(-1))
		);
		context.builder.CreateStore(incremented_value, val.val);
		return val;
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_unary_op const &unary_op,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	switch (unary_op.op)
	{
	// ==== non-overloadable ====
	case lex::token::address_of:         // '&'
		return emit_builtin_unary_address_of(unary_op.expr, context, result_address);
	case lex::token::kw_sizeof:          // 'sizeof'
		// this is always a constant expression
		bz_unreachable;
	case lex::token::kw_move:
	case lex::token::kw_unsafe_move:
		bz_assert(result_address == nullptr);
		return emit_bitcode(unary_op.expr, context, result_address);

	// overloadables are handled as function calls
	default:
		bz_unreachable;
		return val_ptr::get_none();
	}
}


static val_ptr emit_builtin_binary_assign(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	bz_unreachable;
	bz_assert(
		rhs.get_expr_type().get_mut_reference().is<ast::ts_base_type>()
		&& rhs.get_expr_type().get_mut_reference().get<ast::ts_base_type>().info->kind == ast::type_info::null_t_
	);

	emit_bitcode(rhs, context, nullptr);
	auto const lhs_val = emit_bitcode(lhs, context, nullptr);
	bz_assert(lhs_val.kind == val_ptr::reference);

	context.builder.CreateStore(llvm::ConstantPointerNull::get(context.get_opaque_pointer_t()), lhs_val.val);
	return lhs_val;
}

static val_ptr emit_builtin_binary_plus(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = lhs.get_expr_type();
	auto const rhs_t = rhs.get_expr_type();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind))
		{
			auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
			auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
			auto const result_val = ast::is_floating_point_kind(lhs_kind)
				? context.builder.CreateFAdd(lhs_val, rhs_val, "add_tmp")
				: context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
		else if (lhs_kind == ast::type_info::char_)
		{
			auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
			auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_uint32_t(),
				ast::is_signed_integer_kind(rhs_kind)
			);
			auto const result_val = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
		else // if (rhs_kind == ast::type_info::char_)
		{
			bz_assert(rhs_kind == ast::type_info::char_);
			auto lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
			lhs_val = context.builder.CreateIntCast(
				lhs_val,
				context.get_uint32_t(),
				ast::is_signed_integer_kind(lhs_kind)
			);
			auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
			auto const result_val = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
	}
	else if (lhs_t.is<ast::ts_pointer>() || lhs_t.is_optional_pointer())
	{
		bz_assert(rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
		auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to usize, otherwise big values might count as a negative index
		if (ast::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_usize_t(), false);
		}
		auto const lhs_inner_type = lhs_t.is<ast::ts_pointer>()
			? get_llvm_type(lhs_t.get<ast::ts_pointer>(), context)
			: get_llvm_type(lhs_t.get_optional_pointer(), context);

		if (lhs_t.is_optional_pointer())
		{
			emit_null_pointer_arithmetic_check(lhs.src_tokens, lhs_val, rhs_val, context);
		}

		auto const result_val = context.create_gep(lhs_inner_type, lhs_val, rhs_val, "ptr_add_tmp");
		return value_or_result_address(result_val, result_address, context);
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_base_type>() && (rhs_t.is<ast::ts_pointer>() || rhs_t.is_optional_pointer()));
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to usize, otherwise big values might count as a negative index
		if (ast::is_unsigned_integer_kind(lhs_kind))
		{
			lhs_val = context.builder.CreateIntCast(lhs_val, context.get_usize_t(), false);
		}
		auto const rhs_inner_type = rhs_t.is<ast::ts_pointer>()
			? get_llvm_type(rhs_t.get<ast::ts_pointer>(), context)
			: get_llvm_type(rhs_t.get_optional_pointer(), context);

		if (rhs_t.is_optional_pointer())
		{
			emit_null_pointer_arithmetic_check(rhs.src_tokens, rhs_val, lhs_val, context);
		}

		auto const result_val = context.create_gep(rhs_inner_type, rhs_val, lhs_val, "ptr_add_tmp");
		return value_or_result_address(result_val, result_address, context);
	}
}

static val_ptr emit_builtin_binary_plus_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	auto const lhs_t = lhs.get_expr_type().get_mut_reference();
	auto const rhs_t = rhs.get_expr_type();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind))
		{
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
			auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context.builder);
			llvm::Value *res;
			if (ast::is_integer_kind(lhs_kind))
			{
				res = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			}
			else
			{
				bz_assert(ast::is_floating_point_kind(lhs_kind));
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
			auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
			auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context.builder);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_uint32_t(),
				ast::is_signed_integer_kind(rhs_kind)
			);
			auto const res = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			context.builder.CreateStore(res, lhs_val_ref.val);
			return lhs_val_ref;
		}
	}
	else
	{
		bz_assert((lhs_t.is<ast::ts_pointer>() || lhs_t.is_optional_pointer()) && rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		// we calculate the right hand side first
		auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to usize, otherwise big values might count as a negative index
		if (ast::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_usize_t(), false);
		}
		auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
		bz_assert(lhs_val_ref.kind == val_ptr::reference);
		auto const lhs_val = lhs_val_ref.get_value(context.builder);
		auto const lhs_inner_type = lhs_t.is<ast::ts_pointer>()
			? get_llvm_type(lhs_t.get<ast::ts_pointer>(), context)
			: get_llvm_type(lhs_t.get_optional_pointer(), context);

		if (lhs_t.is_optional_pointer())
		{
			emit_null_pointer_arithmetic_check(lhs.src_tokens, lhs_val, rhs_val, context);
		}

		auto const res = context.create_gep(lhs_inner_type, lhs_val, rhs_val, "ptr_add_tmp");
		context.builder.CreateStore(res, lhs_val_ref.val);
		return lhs_val_ref;
	}
}

static val_ptr emit_builtin_binary_minus(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = lhs.get_expr_type();
	auto const rhs_t = rhs.get_expr_type();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind))
		{
			auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
			auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
			auto const result_val = ast::is_floating_point_kind(lhs_kind)
				? context.builder.CreateFSub(lhs_val, rhs_val, "sub_tmp")
				: context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
		else if (
			lhs_kind == ast::type_info::char_
			&& rhs_kind == ast::type_info::char_
		)
		{
			auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
			auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
			auto const result_val = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
		else
		{
			bz_assert(
				lhs_kind == ast::type_info::char_
				&& ast::is_integer_kind(rhs_kind)
			);
			auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
			auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_int32_t(),
				ast::is_signed_integer_kind(rhs_kind)
			);
			auto const result_val = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
	}
	else if (rhs_t.is<ast::ts_base_type>())
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() || lhs_t.is_optional_pointer());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
		auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to usize, otherwise big values might count as a negative index
		if (ast::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_usize_t(), false);
		}
		// negate rhs_val
		rhs_val = context.builder.CreateNeg(rhs_val);
		auto const lhs_inner_type = lhs_t.is<ast::ts_pointer>()
			? get_llvm_type(lhs_t.get<ast::ts_pointer>(), context)
			: get_llvm_type(lhs_t.get_optional_pointer(), context);

		if (lhs_t.is_optional_pointer())
		{
			emit_null_pointer_arithmetic_check(lhs.src_tokens, lhs_val, rhs_val, context);
		}

		auto const result_val = context.create_gep(lhs_inner_type, lhs_val, rhs_val, "ptr_sub_tmp");
		return value_or_result_address(result_val, result_address, context);
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() || lhs_t.is_optional_pointer());
		bz_assert(rhs_t.is<ast::ts_pointer>() || rhs_t.is_optional_pointer());
		bz_assert(lhs_t.is_optional_pointer() == rhs_t.is_optional_pointer());
		auto const elem_type = lhs_t.is<ast::ts_pointer>()
			? get_llvm_type(lhs_t.get<ast::ts_pointer>(), context)
			: get_llvm_type(lhs_t.get_optional_pointer(), context);
		auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);

		if (lhs_t.is_optional_pointer())
		{
			if (global_data::panic_on_null_pointer_arithmetic)
			{
				auto const lhs_has_value = optional_has_value(val_ptr::get_value(lhs_val), context);
				auto const rhs_has_value = optional_has_value(val_ptr::get_value(rhs_val), context);
				auto const is_valid = context.builder.CreateICmpEQ(lhs_has_value, rhs_has_value);

				auto const begin_bb = context.builder.GetInsertBlock();
				auto const error_bb = context.add_basic_block("pointer_diff_null_check_error");

				auto const lhs_null_bb = context.add_basic_block("pointer_diff_null_check_error_lhs");
				context.builder.SetInsertPoint(lhs_null_bb);
				emit_panic_call(lhs.src_tokens, "null value used in pointer arithmetic", context);
				bz_assert(context.has_terminator());

				auto const rhs_null_bb = context.add_basic_block("pointer_diff_null_check_error_rhs");
				context.builder.SetInsertPoint(rhs_null_bb);
				emit_panic_call(rhs.src_tokens, "null value used in pointer arithmetic", context);
				bz_assert(context.has_terminator());

				context.builder.SetInsertPoint(error_bb);
				context.builder.CreateCondBr(lhs_has_value, rhs_null_bb, lhs_null_bb);

				auto const end_bb = context.add_basic_block("pointer_diff_null_check_end");
				context.builder.SetInsertPoint(begin_bb);
				context.builder.CreateCondBr(is_valid, end_bb, error_bb);
				context.builder.SetInsertPoint(end_bb);
			}
		}

		auto const result_val_i64 = context.builder.CreatePtrDiff(elem_type, lhs_val, rhs_val, "ptr_diff_tmp");
		auto const result_val = context.builder.CreateIntCast(result_val_i64, context.get_isize_t(), true);
		return value_or_result_address(result_val, result_address, context);
	}
}

static val_ptr emit_builtin_binary_minus_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	auto const lhs_t = lhs.get_expr_type().get_mut_reference();
	auto const rhs_t = rhs.get_expr_type();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind))
		{
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
			auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context.builder);
			llvm::Value *res;
			if (ast::is_integer_kind(lhs_kind))
			{
				rhs_val = context.builder.CreateIntCast(
					rhs_val,
					lhs_val->getType(),
					ast::is_signed_integer_kind(rhs_kind)
				);
				res = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			}
			else
			{
				bz_assert(ast::is_floating_point_kind(lhs_kind));
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
			auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
			auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
			bz_assert(lhs_val_ref.kind == val_ptr::reference);
			auto const lhs_val = lhs_val_ref.get_value(context.builder);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_uint32_t(),
				ast::is_signed_integer_kind(rhs_kind)
			);
			auto const res = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			context.builder.CreateStore(res, lhs_val_ref.val);
			return lhs_val_ref;
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() || lhs_t.is_optional_pointer());
		bz_assert(rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		// we calculate the right hand side first
		auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to usize, otherwise big values might count as a negative index
		if (ast::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_usize_t(), false);
		}
		// negate rhs_val
		rhs_val = context.builder.CreateNeg(rhs_val);
		auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
		bz_assert(lhs_val_ref.kind == val_ptr::reference);
		auto const lhs_val = lhs_val_ref.get_value(context.builder);
		auto const lhs_inner_type = lhs_t.is<ast::ts_pointer>()
			? get_llvm_type(lhs_t.get<ast::ts_pointer>(), context)
			: get_llvm_type(lhs_t.get_optional_pointer(), context);

		if (lhs_t.is_optional_pointer())
		{
			emit_null_pointer_arithmetic_check(lhs.src_tokens, lhs_val, rhs_val, context);
		}

		auto const res = context.create_gep(lhs_inner_type, lhs_val, rhs_val, "ptr_sub_tmp");
		context.builder.CreateStore(res, lhs_val_ref.val);
		return lhs_val_ref;
	}
}

static val_ptr emit_builtin_binary_multiply(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = lhs.get_expr_type();

	bz_assert(lhs_t == rhs.get_expr_type());
	bz_assert(lhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_arithmetic_kind(lhs_kind));
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = ast::is_floating_point_kind(lhs_kind)
		? context.builder.CreateFMul(lhs_val, rhs_val, "mul_tmp")
		: context.builder.CreateMul(lhs_val, rhs_val, "mul_tmp");
	return value_or_result_address(result_val, result_address, context);
}

static val_ptr emit_builtin_binary_multiply_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	auto const lhs_t = lhs.get_expr_type().get_mut_reference();

	bz_assert(lhs_t == rhs.get_expr_type());
	bz_assert(lhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_arithmetic_kind(lhs_kind));
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = ast::is_integer_kind(lhs_kind)
		? context.builder.CreateMul(lhs_val, rhs_val, "mul_tmp")
		: context.builder.CreateFMul(lhs_val, rhs_val, "mul_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_builtin_binary_divide(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = lhs.get_expr_type();

	bz_assert(lhs_t == rhs.get_expr_type());
	bz_assert(lhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_arithmetic_kind(lhs_kind));
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);

	if (global_data::panic_on_int_divide_by_zero && ast::is_integer_kind(lhs_kind))
	{
		auto const is_rhs_zero = context.builder.CreateICmpEQ(rhs_val, llvm::ConstantInt::get(rhs_val->getType(), 0));
		auto const begin_bb = context.builder.GetInsertBlock();

		auto const panic_bb = context.add_basic_block("divide_by_zero_check");
		context.builder.SetInsertPoint(panic_bb);
		emit_panic_call(src_tokens, "integer division by zero", context);

		auto const end_bb = context.add_basic_block("divide_by_zero_check_end");
		context.builder.SetInsertPoint(begin_bb);
		context.builder.CreateCondBr(is_rhs_zero, panic_bb, end_bb);
		context.builder.SetInsertPoint(end_bb);
	}

	auto const result_val = [&]() -> llvm::Value * {
		if (ast::is_signed_integer_kind(lhs_kind))
		{
			auto const result_type = lhs_val->getType();
			auto const min_value = [&]() {
				switch (lhs_kind)
				{
				case ast::type_info::int8_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int8_t>::min());
				case ast::type_info::int16_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int16_t>::min());
				case ast::type_info::int32_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int32_t>::min());
				case ast::type_info::int64_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int64_t>::min());
				default:
					bz_unreachable;
				}
			}();
			auto const lhs_is_overflow = context.builder.CreateICmpEQ(lhs_val, min_value);
			auto const rhs_is_overflow = context.builder.CreateICmpEQ(rhs_val, llvm::ConstantInt::getSigned(result_type, -1));
			auto const is_overflow = context.builder.CreateAnd(lhs_is_overflow, rhs_is_overflow);

			auto const begin_bb = context.builder.GetInsertBlock();
			auto const non_overflow_bb = context.add_basic_block("div_overflow_check");
			auto const end_bb = context.add_basic_block("div_overflow_check_end");

			context.builder.CreateCondBr(is_overflow, end_bb, non_overflow_bb);
			context.builder.SetInsertPoint(non_overflow_bb);
			auto const non_overflow_result = context.builder.CreateSDiv(lhs_val, rhs_val, "div_tmp");
			context.builder.CreateBr(end_bb);

			context.builder.SetInsertPoint(end_bb);
			auto const result = context.builder.CreatePHI(result_type, 2, "div_tmp");
			result->addIncoming(non_overflow_result, non_overflow_bb);
			result->addIncoming(min_value, begin_bb);

			return result;
		}
		else if (ast::is_unsigned_integer_kind(lhs_kind))
		{
			return context.builder.CreateUDiv(lhs_val, rhs_val, "div_tmp");
		}
		else
		{
			return context.builder.CreateFDiv(lhs_val, rhs_val, "div_tmp");
		}
	}();

	return value_or_result_address(result_val, result_address, context);
}

static val_ptr emit_builtin_binary_divide_eq(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	auto const lhs_t = lhs.get_expr_type().get_mut_reference();

	bz_assert(lhs_t == rhs.get_expr_type());
	bz_assert(lhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_arithmetic_kind(lhs_kind));
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);

	if (global_data::panic_on_int_divide_by_zero && ast::is_integer_kind(lhs_kind))
	{
		auto const is_rhs_zero = context.builder.CreateICmpEQ(rhs_val, llvm::ConstantInt::get(rhs_val->getType(), 0));
		auto const begin_bb = context.builder.GetInsertBlock();

		auto const panic_bb = context.add_basic_block("divide_by_zero_check");
		context.builder.SetInsertPoint(panic_bb);
		emit_panic_call(src_tokens, "integer division by zero", context);

		auto const end_bb = context.add_basic_block("divide_by_zero_check_end");
		context.builder.SetInsertPoint(begin_bb);
		context.builder.CreateCondBr(is_rhs_zero, panic_bb, end_bb);
		context.builder.SetInsertPoint(end_bb);
	}

	auto const res = [&]() -> llvm::Value * {
		if (ast::is_signed_integer_kind(lhs_kind))
		{
			auto const result_type = lhs_val->getType();
			auto const min_value = [&]() {
				switch (lhs_kind)
				{
				case ast::type_info::int8_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int8_t>::min());
				case ast::type_info::int16_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int16_t>::min());
				case ast::type_info::int32_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int32_t>::min());
				case ast::type_info::int64_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int64_t>::min());
				default:
					bz_unreachable;
				}
			}();
			auto const lhs_is_overflow = context.builder.CreateICmpEQ(lhs_val, min_value);
			auto const rhs_is_overflow = context.builder.CreateICmpEQ(rhs_val, llvm::ConstantInt::getSigned(result_type, -1));
			auto const is_overflow = context.builder.CreateAnd(lhs_is_overflow, rhs_is_overflow);

			auto const begin_bb = context.builder.GetInsertBlock();
			auto const non_overflow_bb = context.add_basic_block("div_overflow_check");
			auto const end_bb = context.add_basic_block("div_overflow_check_end");

			context.builder.CreateCondBr(is_overflow, end_bb, non_overflow_bb);
			context.builder.SetInsertPoint(non_overflow_bb);
			auto const non_overflow_result = context.builder.CreateSDiv(lhs_val, rhs_val, "div_tmp");
			context.builder.CreateBr(end_bb);

			context.builder.SetInsertPoint(end_bb);
			auto const result = context.builder.CreatePHI(result_type, 2, "div_tmp");
			result->addIncoming(non_overflow_result, non_overflow_bb);
			result->addIncoming(min_value, begin_bb);

			return result;
		}
		else if (ast::is_unsigned_integer_kind(lhs_kind))
		{
			return context.builder.CreateUDiv(lhs_val, rhs_val, "div_tmp");
		}
		else
		{
			return context.builder.CreateFDiv(lhs_val, rhs_val, "div_tmp");
		}
	}();

	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_builtin_binary_modulo(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = lhs.get_expr_type();

	bz_assert(lhs_t == rhs.get_expr_type());
	bz_assert(lhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_integer_kind(lhs_kind));
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);

	if (global_data::panic_on_int_divide_by_zero && ast::is_integer_kind(lhs_kind))
	{
		auto const is_rhs_zero = context.builder.CreateICmpEQ(rhs_val, llvm::ConstantInt::get(rhs_val->getType(), 0));
		auto const begin_bb = context.builder.GetInsertBlock();

		auto const panic_bb = context.add_basic_block("divide_by_zero_check");
		context.builder.SetInsertPoint(panic_bb);
		emit_panic_call(src_tokens, "integer division by zero", context);

		auto const end_bb = context.add_basic_block("divide_by_zero_check_end");
		context.builder.SetInsertPoint(begin_bb);
		context.builder.CreateCondBr(is_rhs_zero, panic_bb, end_bb);
		context.builder.SetInsertPoint(end_bb);
	}

	auto const result_val = [&]() -> llvm::Value * {
		if (ast::is_signed_integer_kind(lhs_kind))
		{
			auto const result_type = lhs_val->getType();
			auto const min_value = [&]() {
				switch (lhs_kind)
				{
				case ast::type_info::int8_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int8_t>::min());
				case ast::type_info::int16_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int16_t>::min());
				case ast::type_info::int32_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int32_t>::min());
				case ast::type_info::int64_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int64_t>::min());
				default:
					bz_unreachable;
				}
			}();
			auto const lhs_is_overflow = context.builder.CreateICmpEQ(lhs_val, min_value);
			auto const rhs_is_overflow = context.builder.CreateICmpEQ(rhs_val, llvm::ConstantInt::getSigned(result_type, -1));
			auto const is_overflow = context.builder.CreateAnd(lhs_is_overflow, rhs_is_overflow);

			auto const begin_bb = context.builder.GetInsertBlock();
			auto const non_overflow_bb = context.add_basic_block("mod_overflow_check");
			auto const end_bb = context.add_basic_block("mod_overflow_check_end");

			context.builder.CreateCondBr(is_overflow, end_bb, non_overflow_bb);
			context.builder.SetInsertPoint(non_overflow_bb);
			auto const non_overflow_result = context.builder.CreateSRem(lhs_val, rhs_val, "mod_tmp");
			context.builder.CreateBr(end_bb);

			context.builder.SetInsertPoint(end_bb);
			auto const result = context.builder.CreatePHI(result_type, 2, "mod_tmp");
			result->addIncoming(non_overflow_result, non_overflow_bb);
			result->addIncoming(llvm::ConstantInt::getSigned(result_type, 0), begin_bb);

			return result;
		}
		else
		{
			bz_assert(ast::is_unsigned_integer_kind(lhs_kind));
			return context.builder.CreateURem(lhs_val, rhs_val, "mod_tmp");
		}
	}();

	return value_or_result_address(result_val, result_address, context);
}

static val_ptr emit_builtin_binary_modulo_eq(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	auto const lhs_t = lhs.get_expr_type().get_mut_reference();

	bz_assert(lhs_t == rhs.get_expr_type());
	bz_assert(lhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_integer_kind(lhs_kind));
	// we calculate the right hand side first
	auto rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);

	if (global_data::panic_on_int_divide_by_zero && ast::is_integer_kind(lhs_kind))
	{
		auto const is_rhs_zero = context.builder.CreateICmpEQ(rhs_val, llvm::ConstantInt::get(rhs_val->getType(), 0));
		auto const begin_bb = context.builder.GetInsertBlock();

		auto const panic_bb = context.add_basic_block("divide_by_zero_check");
		context.builder.SetInsertPoint(panic_bb);
		emit_panic_call(src_tokens, "integer division by zero", context);

		auto const end_bb = context.add_basic_block("divide_by_zero_check_end");
		context.builder.SetInsertPoint(begin_bb);
		context.builder.CreateCondBr(is_rhs_zero, panic_bb, end_bb);
		context.builder.SetInsertPoint(end_bb);
	}

	auto const res = [&]() -> llvm::Value * {
		if (ast::is_signed_integer_kind(lhs_kind))
		{
			auto const result_type = lhs_val->getType();
			auto const min_value = [&]() {
				switch (lhs_kind)
				{
				case ast::type_info::int8_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int8_t>::min());
				case ast::type_info::int16_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int16_t>::min());
				case ast::type_info::int32_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int32_t>::min());
				case ast::type_info::int64_:
					return llvm::ConstantInt::getSigned(result_type, std::numeric_limits<int64_t>::min());
				default:
					bz_unreachable;
				}
			}();
			auto const lhs_is_overflow = context.builder.CreateICmpEQ(lhs_val, min_value);
			auto const rhs_is_overflow = context.builder.CreateICmpEQ(rhs_val, llvm::ConstantInt::getSigned(result_type, -1));
			auto const is_overflow = context.builder.CreateAnd(lhs_is_overflow, rhs_is_overflow);

			auto const begin_bb = context.builder.GetInsertBlock();
			auto const non_overflow_bb = context.add_basic_block("mod_overflow_check");
			auto const end_bb = context.add_basic_block("mod_overflow_check_end");

			context.builder.CreateCondBr(is_overflow, end_bb, non_overflow_bb);
			context.builder.SetInsertPoint(non_overflow_bb);
			auto const non_overflow_result = context.builder.CreateSRem(lhs_val, rhs_val, "mod_tmp");
			context.builder.CreateBr(end_bb);

			context.builder.SetInsertPoint(end_bb);
			auto const result = context.builder.CreatePHI(result_type, 2, "mod_tmp");
			result->addIncoming(non_overflow_result, non_overflow_bb);
			result->addIncoming(llvm::ConstantInt::getSigned(result_type, 0), begin_bb);

			return result;
		}
		else
		{
			bz_assert(ast::is_unsigned_integer_kind(lhs_kind));
			return context.builder.CreateURem(lhs_val, rhs_val, "mod_tmp");
		}
	}();
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_builtin_binary_cmp(
	uint32_t op,
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(
		op == lex::token::equals
		|| op == lex::token::not_equals
		|| op == lex::token::less_than
		|| op == lex::token::less_than_eq
		|| op == lex::token::greater_than
		|| op == lex::token::greater_than_eq
	);
	auto const lhs_t = lhs.get_expr_type().remove_mut_reference();
	auto const rhs_t = rhs.get_expr_type().remove_mut_reference();
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
				llvm::CmpInst::FCMP_UNE,
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

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
		bz_assert(lhs_kind != ast::type_info::str_);
		auto const pred = ast::is_floating_point_kind(lhs_kind) ? get_cmp_predicate(2)
			: ast::is_signed_integer_kind(lhs_kind) ? get_cmp_predicate(0)
			: get_cmp_predicate(1);
		auto const result_val = ast::is_floating_point_kind(lhs_kind)
			? context.builder.CreateFCmp(pred, lhs_val, rhs_val)
			: context.builder.CreateICmp(pred, lhs_val, rhs_val);
		return value_or_result_address(result_val, result_address, context);
	}
	else if (lhs_t.is<ast::ts_enum>() && rhs_t.is<ast::ts_enum>())
	{
		auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
		auto const type_kind = lhs_t.get<ast::ts_enum>().decl->underlying_type.get<ast::ts_base_type>().info->kind;
		auto const pred = ast::is_signed_integer_kind(type_kind) ? get_cmp_predicate(0) : get_cmp_predicate(1);
		auto const result_val = context.builder.CreateICmp(pred, lhs_val, rhs_val);
		return value_or_result_address(result_val, result_address, context);
	}
	else if (
		(lhs_t.is<ast::ts_optional>() && rhs_t.is<ast::ts_base_type>())
		|| (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_optional>())
	)
	{
		auto const lhs_val = emit_bitcode(lhs, context, nullptr);
		auto const rhs_val = emit_bitcode(rhs, context, nullptr);
		auto const optional_val = lhs_t.is<ast::ts_optional>() ? lhs_val : rhs_val;
		auto const has_value = optional_has_value(optional_val, context);
		bz_assert(op == lex::token::equals || op == lex::token::not_equals);
		auto const result_val = op == lex::token::not_equals
			? has_value
			: context.builder.CreateNot(has_value);
		return value_or_result_address(result_val, result_address, context);
	}
	else // if pointer or function
	{
		auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
		bz_assert(lhs_val->getType()->isPointerTy());
		bz_assert(rhs_val->getType()->isPointerTy());

		auto const p = get_cmp_predicate(1); // unsigned compare
		auto const result_val = context.builder.CreateICmp(p, lhs_val, rhs_val, "cmp_tmp");
		return value_or_result_address(result_val, result_address, context);
	}
}

static val_ptr emit_builtin_binary_bit_and(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(lhs.get_expr_type() == rhs.get_expr_type());
	bz_assert(lhs.get_expr_type().is<ast::ts_base_type>());
	bz_assert(
		ast::is_unsigned_integer_kind(lhs.get_expr_type().get<ast::ts_base_type>().info->kind)
		|| lhs.get_expr_type().get<ast::ts_base_type>().info->kind == ast::type_info::bool_
	);
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateAnd(lhs_val, rhs_val, "bit_and_tmp");
	return value_or_result_address(result_val, result_address, context);
}

static val_ptr emit_builtin_binary_bit_and_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	bz_assert(lhs.get_expr_type().get_mut_reference() == rhs.get_expr_type());
	bz_assert(lhs.get_expr_type().get_mut_reference().is<ast::ts_base_type>());
	bz_assert(
		ast::is_unsigned_integer_kind(lhs.get_expr_type().get_mut_reference().get<ast::ts_base_type>().info->kind)
		|| lhs.get_expr_type().get_mut_reference().get<ast::ts_base_type>().info->kind == ast::type_info::bool_
	);
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = context.builder.CreateAnd(lhs_val, rhs_val, "bit_and_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_builtin_binary_bit_xor(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(lhs.get_expr_type() == rhs.get_expr_type());
	bz_assert(lhs.get_expr_type().is<ast::ts_base_type>());
	bz_assert(
		ast::is_unsigned_integer_kind(lhs.get_expr_type().get<ast::ts_base_type>().info->kind)
		|| lhs.get_expr_type().get<ast::ts_base_type>().info->kind == ast::type_info::bool_
	);
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateXor(lhs_val, rhs_val, "bit_xor_tmp");
	return value_or_result_address(result_val, result_address, context);
}

static val_ptr emit_builtin_binary_bit_xor_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	bz_assert(lhs.get_expr_type().get_mut_reference() == rhs.get_expr_type());
	bz_assert(lhs.get_expr_type().get_mut_reference().is<ast::ts_base_type>());
	bz_assert(
		ast::is_unsigned_integer_kind(lhs.get_expr_type().get_mut_reference().get<ast::ts_base_type>().info->kind)
		|| lhs.get_expr_type().get_mut_reference().get<ast::ts_base_type>().info->kind == ast::type_info::bool_
	);
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = context.builder.CreateXor(lhs_val, rhs_val, "bit_xor_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_builtin_binary_bit_or(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(lhs.get_expr_type() == rhs.get_expr_type());
	bz_assert(lhs.get_expr_type().is<ast::ts_base_type>());
	bz_assert(
		ast::is_unsigned_integer_kind(lhs.get_expr_type().get<ast::ts_base_type>().info->kind)
		|| lhs.get_expr_type().get<ast::ts_base_type>().info->kind == ast::type_info::bool_
	);
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateOr(lhs_val, rhs_val, "bit_or_tmp");
	return value_or_result_address(result_val, result_address, context);
}

static val_ptr emit_builtin_binary_bit_or_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	bz_assert(lhs.get_expr_type().get_mut_reference() == rhs.get_expr_type());
	bz_assert(lhs.get_expr_type().get_mut_reference().is<ast::ts_base_type>());
	bz_assert(
		ast::is_unsigned_integer_kind(lhs.get_expr_type().get_mut_reference().get<ast::ts_base_type>().info->kind)
		|| lhs.get_expr_type().get_mut_reference().get<ast::ts_base_type>().info->kind == ast::type_info::bool_
	);
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = context.builder.CreateOr(lhs_val, rhs_val, "bit_or_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_builtin_binary_left_shift(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = lhs.get_expr_type();

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs.get_expr_type().is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_unsigned_integer_kind(lhs_kind));
	bz_assert(ast::is_integer_kind(rhs.get_expr_type().get<ast::ts_base_type>().info->kind));
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const cast_rhs_val = context.builder.CreateIntCast(rhs_val, context.get_builtin_type(lhs_kind), false);
	auto const result_val = context.builder.CreateShl(lhs_val, cast_rhs_val, "lshift_tmp");
	return value_or_result_address(result_val, result_address, context);
}

static val_ptr emit_builtin_binary_left_shift_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	auto const lhs_t = lhs.get_expr_type().get_mut_reference();

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs.get_expr_type().is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_unsigned_integer_kind(lhs_kind));
	bz_assert(ast::is_integer_kind(rhs.get_expr_type().get<ast::ts_base_type>().info->kind));
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const cast_rhs_val = context.builder.CreateIntCast(rhs_val, context.get_builtin_type(lhs_kind), false);
	auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = context.builder.CreateShl(lhs_val, cast_rhs_val, "lshift_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
}

static val_ptr emit_builtin_binary_right_shift(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = lhs.get_expr_type();

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs.get_expr_type().is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_unsigned_integer_kind(lhs_kind));
	bz_assert(ast::is_integer_kind(rhs.get_expr_type().get<ast::ts_base_type>().info->kind));
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const cast_rhs_val = context.builder.CreateIntCast(rhs_val, context.get_builtin_type(lhs_kind), false);
	auto const result_val = context.builder.CreateLShr(lhs_val, cast_rhs_val, "rshift_tmp");
	return value_or_result_address(result_val, result_address, context);
}

static val_ptr emit_builtin_binary_right_shift_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context
)
{
	auto const lhs_t = lhs.get_expr_type().get_mut_reference();

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs.get_expr_type().is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_unsigned_integer_kind(lhs_kind));
	bz_assert(ast::is_integer_kind(rhs.get_expr_type().get<ast::ts_base_type>().info->kind));
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const cast_rhs_val = context.builder.CreateIntCast(rhs_val, context.get_builtin_type(lhs_kind), false);
	auto const lhs_val_ref = emit_bitcode(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = context.builder.CreateLShr(lhs_val, cast_rhs_val, "rshift_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	return lhs_val_ref;
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

static val_ptr emit_builtin_subscript_range(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs_type = lhs.get_expr_type().remove_mut_reference();
	auto const rhs_type = rhs.get_expr_type();
	auto const lhs_val = emit_bitcode(lhs, context, nullptr);
	auto const rhs_val = emit_bitcode(rhs, context, nullptr);

	if (result_address == nullptr)
	{
		result_address = context.create_alloca(context.get_slice_t());
	}
	auto const result_value = val_ptr::get_reference(result_address, context.get_slice_t());

	bz_assert(rhs_type.is<ast::ts_base_type>());
	bz_assert(rhs_type.get<ast::ts_base_type>().info->type_name.values.size() == 1);
	auto const kind = range_kind_from_name(rhs_type.get<ast::ts_base_type>().info->type_name.values[0]);

	auto const is_unsigned_index = [&]() {
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
		return ast::is_unsigned_integer_kind(index_type.get<ast::ts_base_type>().info->kind);
	}();

	auto const cast_index = [&](llvm::Value *index) -> llvm::Value * {
		if (is_unsigned_index)
		{
			return context.builder.CreateIntCast(index, context.get_usize_t(), false);
		}
		else
		{
			return index;
		}
	};

	struct begin_end_pair_t
	{
		llvm::Value *begin;
		llvm::Value *end;
	};

	auto const [begin_index, end_index] = [&]() -> begin_end_pair_t {
		switch (kind)
		{
		case range_kind::regular:
			bz_assert(rhs_val.get_type()->getStructNumElements() == 2);
			return {
				.begin = cast_index(context.get_struct_element(rhs_val, 0).get_value(context.builder)),
				.end   = cast_index(context.get_struct_element(rhs_val, 1).get_value(context.builder)),
			};
		case range_kind::from:
			bz_assert(rhs_val.get_type()->getStructNumElements() == 1);
			return {
				.begin = cast_index(context.get_struct_element(rhs_val, 0).get_value(context.builder)),
				.end   = nullptr,
			};
		case range_kind::to:
			bz_assert(rhs_val.get_type()->getStructNumElements() == 1);
			return {
				.begin = nullptr,
				.end   = cast_index(context.get_struct_element(rhs_val, 0).get_value(context.builder)),
			};
		case range_kind::unbounded:
			return {
				.begin = nullptr,
				.end   = nullptr,
			};
		}
	}();

	if (lhs_type.is<ast::ts_array_slice>())
	{
		auto const elem_type = get_llvm_type(lhs_type.get<ast::ts_array_slice>().elem_type, context);
		auto const [begin_ptr, end_ptr] = [&]() -> begin_end_pair_t {
			switch (kind)
			{
			case range_kind::regular:
			{
				auto const lhs_begin_ptr = context.get_struct_element(lhs_val, 0).get_value(context.builder);
				return {
					.begin = context.create_gep(elem_type, lhs_begin_ptr, begin_index),
					.end   = context.create_gep(elem_type, lhs_begin_ptr, end_index),
				};
			}
			case range_kind::from:
			{
				auto const lhs_begin_ptr = context.get_struct_element(lhs_val, 0).get_value(context.builder);
				auto const lhs_end_ptr   = context.get_struct_element(lhs_val, 1).get_value(context.builder);
				return {
					.begin = context.create_gep(elem_type, lhs_begin_ptr, begin_index),
					.end   = lhs_end_ptr,
				};
			}
			case range_kind::to:
			{
				auto const lhs_begin_ptr = context.get_struct_element(lhs_val, 0).get_value(context.builder);
				return {
					.begin = lhs_begin_ptr,
					.end   = context.create_gep(elem_type, lhs_begin_ptr, end_index),
				};
			}
			case range_kind::unbounded:
			{
				auto const lhs_begin_ptr = context.get_struct_element(lhs_val, 0).get_value(context.builder);
				auto const lhs_end_ptr   = context.get_struct_element(lhs_val, 1).get_value(context.builder);
				return {
					.begin = lhs_begin_ptr,
					.end   = lhs_end_ptr,
				};
			}
			}
		}();

		context.builder.CreateStore(begin_ptr, context.get_struct_element(result_value, 0).val);
		context.builder.CreateStore(end_ptr,   context.get_struct_element(result_value, 1).val);
	}
	else if (lhs_type.is<ast::ts_array>())
	{
		bz_assert(lhs_val.kind == val_ptr::reference);
		auto const [begin_ptr, end_ptr] = [&]() -> begin_end_pair_t {
			switch (kind)
			{
			case range_kind::regular:
				return {
					.begin = context.create_array_gep(lhs_val.get_type(), lhs_val.val, begin_index),
					.end   = context.create_array_gep(lhs_val.get_type(), lhs_val.val, end_index),
				};
			case range_kind::from:
				return {
					.begin = context.create_array_gep(lhs_val.get_type(), lhs_val.val, begin_index),
					.end   = context.create_gep(lhs_val.get_type(), lhs_val.val, 0, lhs_type.get<ast::ts_array>().size),
				};
			case range_kind::to:
				return {
					.begin = context.create_gep(lhs_val.get_type(), lhs_val.val, 0, 0),
					.end   = context.create_array_gep(lhs_val.get_type(), lhs_val.val, end_index),
				};
			case range_kind::unbounded:
				return {
					.begin = context.create_gep(lhs_val.get_type(), lhs_val.val, 0, 0),
					.end   = context.create_gep(lhs_val.get_type(), lhs_val.val, 0, lhs_type.get<ast::ts_array>().size),
				};
			}
		}();

		context.builder.CreateStore(begin_ptr, context.get_struct_element(result_value, 0).val);
		context.builder.CreateStore(end_ptr,   context.get_struct_element(result_value, 1).val);
	}
	else
	{
		bz_unreachable;
	}

	return result_value;
}

static val_ptr emit_builtin_binary_bool_and(
	ast::expr_binary_op const &binary_op,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;

	bz_assert(lhs.get_expr_type() == rhs.get_expr_type());
	bz_assert(lhs.get_expr_type().is<ast::ts_base_type>());
	bz_assert(lhs.get_expr_type().get<ast::ts_base_type>().info->kind == ast::type_info::bool_);

	// generate computation of lhs
	auto const lhs_prev_info = context.push_expression_scope();
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	context.pop_expression_scope(lhs_prev_info);
	auto const lhs_bb_end = context.builder.GetInsertBlock();

	// generate computation of rhs
	auto const rhs_bb = context.add_basic_block("bool_and_rhs");
	context.builder.SetInsertPoint(rhs_bb);
	auto const rhs_prev_info = context.push_expression_scope();
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	context.pop_expression_scope(rhs_prev_info);
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

	return value_or_result_address(phi, result_address, context);
}

static val_ptr emit_builtin_binary_bool_xor(
	ast::expr_binary_op const &binary_op,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;

	bz_assert(lhs.get_expr_type() == rhs.get_expr_type());
	bz_assert(lhs.get_expr_type().is<ast::ts_base_type>());
	bz_assert(lhs.get_expr_type().get<ast::ts_base_type>().info->kind == ast::type_info::bool_);
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateXor(lhs_val, rhs_val, "bool_xor_tmp");
	return value_or_result_address(result_val, result_address, context);
}

static val_ptr emit_builtin_binary_bool_or(
	ast::expr_binary_op const &binary_op,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto &lhs = binary_op.lhs;
	auto &rhs = binary_op.rhs;

	bz_assert(lhs.get_expr_type() == rhs.get_expr_type());
	bz_assert(lhs.get_expr_type().is<ast::ts_base_type>());
	bz_assert(lhs.get_expr_type().get<ast::ts_base_type>().info->kind == ast::type_info::bool_);

	// generate computation of lhs
	auto const lhs_prev_info = context.push_expression_scope();
	auto const lhs_val = emit_bitcode(lhs, context, nullptr).get_value(context.builder);
	context.pop_expression_scope(lhs_prev_info);
	auto const lhs_bb_end = context.builder.GetInsertBlock();

	// generate computation of rhs
	auto const rhs_bb = context.add_basic_block("bool_or_rhs");
	context.builder.SetInsertPoint(rhs_bb);
	auto const rhs_prev_info = context.push_expression_scope();
	auto const rhs_val = emit_bitcode(rhs, context, nullptr).get_value(context.builder);
	context.pop_expression_scope(rhs_prev_info);
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

	return value_or_result_address(phi, result_address, context);
}


static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_binary_op const &binary_op,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	switch (binary_op.op)
	{
	// ==== non-overloadable ====
	case lex::token::comma:              // ','
		emit_bitcode(binary_op.lhs, context, nullptr);
		return emit_bitcode(binary_op.rhs, context, result_address);
	case lex::token::bool_and:           // '&&'
		return emit_builtin_binary_bool_and(binary_op, context, result_address);
	case lex::token::bool_xor:           // '^^'
		return emit_builtin_binary_bool_xor(binary_op, context, result_address);
	case lex::token::bool_or:            // '||'
		return emit_builtin_binary_bool_or(binary_op, context, result_address);

	// ==== overloadable ====
	// they are handled as intrinsic functions
	default:
		bz_unreachable;
		return val_ptr::get_none();
	}
}

struct call_args_info_t
{
	ast::arena_vector<llvm::Value *> args;
	ast::arena_vector<is_byval_and_type_pair> args_is_byval;
};

static call_args_info_t emit_function_call_args(
	llvm::Type *result_type,
	abi::pass_kind result_kind,
	ast::expr_function_call const &func_call,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	ast::arena_vector<llvm::Value *> args = {};
	ast::arena_vector<is_byval_and_type_pair> args_is_byval = {};
	args.reserve(
		func_call.params.size()
		+ (result_kind == abi::pass_kind::reference || result_kind == abi::pass_kind::non_trivial ? 1 : 0)
	);
	args_is_byval.reserve(
		func_call.params.size()
		+ (result_kind == abi::pass_kind::reference || result_kind == abi::pass_kind::non_trivial ? 1 : 0)
	);

	auto const emit_arg = [&]<bool push_to_front>(size_t i, bz::meta::integral_constant<bool, push_to_front>)
	{
		auto &p = func_call.params[i];
		auto &param_type = func_call.func_body->params[i].get_type();
		if (ast::is_generic_parameter(func_call.func_body->params[i]))
		{
			// do nothing for typename args
			return;
		}
		else if (p.is_error())
		{
			auto const param_llvm_type = get_llvm_type(param_type, context);
			emit_bitcode(p, context, nullptr);
			auto const param_val = val_ptr::get_value(llvm::UndefValue::get(param_llvm_type));
			add_call_parameter<push_to_front>(param_type, param_llvm_type, param_val, args, args_is_byval, context);
		}
		else
		{
			auto const param_llvm_type = get_llvm_type(param_type, context);
			auto const param_val = emit_bitcode(p, context, nullptr);
			bz_assert(param_val.val != nullptr || param_val.consteval_val != nullptr);
			add_call_parameter<push_to_front>(param_type, param_llvm_type, param_val, args, args_is_byval, context);
		}
	};

	if (func_call.param_resolve_order == ast::resolve_order::reversed)
	{
		for (auto const i : bz::iota(0, func_call.params.size()).reversed())
		{
			emit_arg(i, bz::meta::integral_constant<bool, true>{});
		}
	}
	else
	{
		for (auto const i : bz::iota(0, func_call.params.size()))
		{
			emit_arg(i, bz::meta::integral_constant<bool, false>{});
		}
	}

	if (result_kind == abi::pass_kind::reference || result_kind == abi::pass_kind::non_trivial)
	{
		auto const output_ptr = result_address != nullptr
			? result_address
			: context.create_alloca(result_type);
		args.push_front(output_ptr);
		args_is_byval.push_front({ false, nullptr });
	}

	return { std::move(args), std::move(args_is_byval) };
}

static val_ptr emit_function_call(
	ast::typespec_view return_type,
	llvm::Type *result_type,
	abi::pass_kind result_kind,
	llvm::FunctionType *fn_type,
	llvm::Value *fn,
	llvm::CallingConv::ID calling_convention,
	bz::array_view<llvm::Value * const> args,
	bz::array_view<is_byval_and_type_pair const> args_is_byval,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const call = [&]() {
		auto const call = context.create_call(
			{ fn_type, fn },
			calling_convention,
			llvm::ArrayRef(args.data(), args.size())
		);
		auto is_byval_it = args_is_byval.begin();
		auto const is_byval_end = args_is_byval.end();
		unsigned i = 0;
		bz_assert(fn_type->getNumParams() == call->arg_size());
		if (result_kind == abi::pass_kind::reference || result_kind == abi::pass_kind::non_trivial)
		{
			call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context.get_llvm_context(), result_type));
			bz_assert(is_byval_it != is_byval_end);
			++is_byval_it, ++i;
		}
		for (; is_byval_it != is_byval_end; ++is_byval_it, ++i)
		{
			if (is_byval_it->is_byval)
			{
				add_byval_attributes(call, is_byval_it->type, i, context);
			}
		}
		return call;
	}();

	switch (result_kind)
	{
	case abi::pass_kind::reference:
	case abi::pass_kind::non_trivial:
		bz_assert(result_address == nullptr || args.front() == result_address);
		return val_ptr::get_reference(args.front(), result_type);
	case abi::pass_kind::value:
		if (call->getType()->isVoidTy())
		{
			return val_ptr::get_none();
		}
		else if (return_type.is<ast::ts_lvalue_reference>())
		{
			auto const inner_result_type = return_type.get<ast::ts_lvalue_reference>();
			auto const inner_result_llvm_type = get_llvm_type(inner_result_type, context);
			bz_assert(result_address == nullptr);
			return val_ptr::get_reference(call, inner_result_llvm_type);
		}
		return value_or_result_address(call, result_address, context);
	case abi::pass_kind::one_register:
	case abi::pass_kind::two_registers:
	{
		auto const call_result_type = call->getType();
		if (result_address != nullptr)
		{
			context.builder.CreateStore(call, result_address);
			return val_ptr::get_reference(result_address, result_type);
		}
		else if (result_type == call_result_type)
		{
			return val_ptr::get_value(call);
		}
		else
		{
			auto const result_ptr = context.create_alloca(result_type);
			context.builder.CreateStore(call, result_ptr);
			return val_ptr::get_reference(result_ptr, result_type);
		}
	}
	default:
		bz_unreachable;
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_function_call const &func_call,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	if (func_call.func_body->is_only_consteval())
	{
		auto notes = [&]() {
			bz::vector<ctx::source_highlight> result;
			if (!func_call.func_body->is_intrinsic())
			{
				result.push_back(context.make_note(func_call.func_body->src_tokens, "function was declared 'consteval' here"));
			}
			else
			{
				result.push_back(context.make_note(func_call.func_body->src_tokens, bz::format(
					"builtin function '{}' can only be used in a constant expression",
					func_call.func_body->get_signature()
				)));
			}
			return result;
		}();
		context.report_error(
			func_call.src_tokens,
			"a function marked as 'consteval' can only be used in a constant expression",
			std::move(notes)
		);
		if (func_call.func_body->return_type.is<ast::ts_void>())
		{
			return val_ptr::get_none();
		}
		else
		{
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}
			return val_ptr::get_reference(result_address, result_type);
		}
	}

	if (func_call.func_body->is_intrinsic() && func_call.func_body->body.is_null())
	{
		switch (func_call.func_body->intrinsic_kind)
		{
		static_assert(ast::function_body::_builtin_last - ast::function_body::_builtin_first == 285);
		static_assert(ast::function_body::_builtin_default_constructor_last - ast::function_body::_builtin_default_constructor_first == 14);
		static_assert(ast::function_body::_builtin_unary_operator_last - ast::function_body::_builtin_unary_operator_first == 7);
		static_assert(ast::function_body::_builtin_binary_operator_last - ast::function_body::_builtin_binary_operator_first == 28);
		case ast::function_body::builtin_str_begin_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const arg = emit_bitcode(func_call.params[0], context, nullptr);
			auto const begin_ptr = context.get_struct_element(arg, 0).get_value(context.builder);
			return value_or_result_address(begin_ptr, result_address, context);
		}
		case ast::function_body::builtin_str_end_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const arg = emit_bitcode(func_call.params[0], context, nullptr);
			auto const end_ptr = context.get_struct_element(arg, 1).get_value(context.builder);
			return value_or_result_address(end_ptr, result_address, context);
		}
		case ast::function_body::builtin_str_from_ptrs:
		{
			bz_assert(func_call.params.size() == 2);
			auto const begin_ptr = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const end_ptr   = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			if (result_address != nullptr)
			{
				auto const str_t = context.get_str_t();
				auto const result_begin_ptr = context.create_struct_gep(str_t, result_address, 0);
				auto const result_end_ptr   = context.create_struct_gep(str_t, result_address, 1);
				context.builder.CreateStore(begin_ptr, result_begin_ptr);
				context.builder.CreateStore(end_ptr, result_end_ptr);
				return val_ptr::get_reference(result_address, str_t);
			}
			else
			{
				bz_assert(context.get_str_t()->isStructTy());
				auto const str_t = static_cast<llvm::StructType *>(context.get_str_t());
				auto const str_member_t = str_t->getElementType(0);
				auto const undef_value = llvm::UndefValue::get(str_member_t);
				llvm::Value *result = llvm::ConstantStruct::get(str_t, undef_value, undef_value);
				result = context.builder.CreateInsertValue(result, begin_ptr, 0);
				result = context.builder.CreateInsertValue(result, end_ptr,   1);
				return val_ptr::get_value(result);
			}
		}
		case ast::function_body::builtin_slice_begin_ptr:
		case ast::function_body::builtin_slice_begin_mut_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const slice = emit_bitcode(func_call.params[0], context, nullptr);
			auto const begin_ptr = context.get_struct_element(slice, 0).get_value(context.builder);
			return value_or_result_address(begin_ptr, result_address, context);
		}
		case ast::function_body::builtin_slice_end_ptr:
		case ast::function_body::builtin_slice_end_mut_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const slice = emit_bitcode(func_call.params[0], context, nullptr);
			auto const end_ptr = context.get_struct_element(slice, 1).get_value(context.builder);
			return value_or_result_address(end_ptr, result_address, context);
		}
		case ast::function_body::builtin_slice_from_ptrs:
		case ast::function_body::builtin_slice_from_mut_ptrs:
		{
			bz_assert(func_call.params.size() == 2);
			auto const begin_ptr = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const end_ptr   = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const slice_t = context.get_slice_t();
			if (result_address != nullptr)
			{
				auto const result_begin_ptr = context.create_struct_gep(slice_t, result_address, 0);
				auto const result_end_ptr   = context.create_struct_gep(slice_t, result_address, 1);
				context.builder.CreateStore(begin_ptr, result_begin_ptr);
				context.builder.CreateStore(end_ptr, result_end_ptr);
				return val_ptr::get_reference(result_address, slice_t);
			}
			else
			{
				bz_assert(begin_ptr->getType()->isPointerTy());
				bz_assert(slice_t->isStructTy());
				auto const slice_member_t = slice_t->getStructElementType(0);
				auto const undef_value = llvm::UndefValue::get(slice_member_t);
				llvm::Value *result = llvm::ConstantStruct::get(slice_t, undef_value, undef_value);
				result = context.builder.CreateInsertValue(result, begin_ptr, 0);
				result = context.builder.CreateInsertValue(result, end_ptr,   1);
				return val_ptr::get_value(result);
			}
		}
		case ast::function_body::builtin_array_begin_ptr:
		case ast::function_body::builtin_array_begin_mut_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const arr = emit_bitcode(func_call.params[0], context, nullptr);
			bz_assert(arr.kind == val_ptr::reference);
			bz_assert(arr.get_type()->isArrayTy());
			auto const begin_ptr = context.get_struct_element(arr, 0).val;
			return value_or_result_address(begin_ptr, result_address, context);
		}
		case ast::function_body::builtin_array_end_ptr:
		case ast::function_body::builtin_array_end_mut_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const arr = emit_bitcode(func_call.params[0], context, nullptr);
			bz_assert(arr.kind == val_ptr::reference);
			bz_assert(arr.get_type()->isArrayTy());
			auto const size = arr.get_type()->getArrayNumElements();
			auto const end_ptr = context.get_struct_element(arr, size).val;
			return value_or_result_address(end_ptr, result_address, context);
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
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			bz_assert(result_type->isStructTy());
			bz_assert(result_type->getStructNumElements() == 2);
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}

			auto const begin = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const end   = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);

			auto const result_begin_ref = context.create_struct_gep(result_type, result_address, 0);
			auto const result_end_ref   = context.create_struct_gep(result_type, result_address, 1);
			context.builder.CreateStore(begin, result_begin_ref);
			context.builder.CreateStore(end,   result_end_ref);
			return val_ptr::get_reference(result_address, result_type);
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
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			bz_assert(result_type->isStructTy());
			bz_assert(result_type->getStructNumElements() == 1);
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}

			auto const begin = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result_begin_ref = context.create_struct_gep(result_type, result_address, 0);
			context.builder.CreateStore(begin, result_begin_ref);
			return val_ptr::get_reference(result_address, result_type);
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
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			bz_assert(result_type->isStructTy());
			bz_assert(result_type->getStructNumElements() == 1);
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}

			auto const end = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result_end_ref = context.create_struct_gep(result_type, result_address, 0);
			context.builder.CreateStore(end, result_end_ref);
			return val_ptr::get_reference(result_address, result_type);
		}
		case ast::function_body::builtin_range_unbounded:
		{
			bz_assert(func_call.params.empty());
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			bz_assert(result_type->isStructTy());
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}
			return val_ptr::get_reference(result_address, result_type);
		}
		case ast::function_body::builtin_integer_range_begin_value:
		case ast::function_body::builtin_integer_range_inclusive_begin_value:
		{
			bz_assert(func_call.params.size() == 1);
			auto const range_val = emit_bitcode(func_call.params[0], context, nullptr);
			auto const begin_value_ptr = context.create_struct_gep(range_val.get_type(), range_val.val, 0);
			auto const result_type = range_val.get_type()->getStructElementType(0);
			auto const begin_value = context.builder.CreateLoad(result_type, begin_value_ptr);
			return value_or_result_address(begin_value, result_address, context);
		}
		case ast::function_body::builtin_integer_range_end_value:
		case ast::function_body::builtin_integer_range_inclusive_end_value:
		{
			bz_assert(func_call.params.size() == 1);
			auto const range_val = emit_bitcode(func_call.params[0], context, nullptr);
			auto const end_value_ptr = context.create_struct_gep(range_val.get_type(), range_val.val, 1);
			auto const result_type = range_val.get_type()->getStructElementType(0);
			auto const end_value = context.builder.CreateLoad(result_type, end_value_ptr);
			return value_or_result_address(end_value, result_address, context);
		}
		case ast::function_body::builtin_integer_range_from_begin_value:
		{
			bz_assert(func_call.params.size() == 1);
			auto const range_val = emit_bitcode(func_call.params[0], context, nullptr);
			auto const begin_value_ptr = context.create_struct_gep(range_val.get_type(), range_val.val, 0);
			auto const result_type = range_val.get_type()->getStructElementType(0);
			auto const begin_value = context.builder.CreateLoad(result_type, begin_value_ptr);
			return value_or_result_address(begin_value, result_address, context);
		}
		case ast::function_body::builtin_integer_range_to_end_value:
		case ast::function_body::builtin_integer_range_to_inclusive_end_value:
		{
			bz_assert(func_call.params.size() == 1);
			auto const range_val = emit_bitcode(func_call.params[0], context, nullptr);
			auto const end_value_ptr = context.create_struct_gep(range_val.get_type(), range_val.val, 0);
			auto const result_type = range_val.get_type()->getStructElementType(0);
			auto const end_value = context.builder.CreateLoad(result_type, end_value_ptr);
			return value_or_result_address(end_value, result_address, context);
		}
		case ast::function_body::builtin_integer_range_begin_iterator:
		{
			bz_assert(func_call.params.size() == 1);
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			bz_assert(result_type->isStructTy());
			bz_assert(result_type->getStructNumElements() == 1);
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}

			auto const range_value = emit_bitcode(func_call.params[0], context, nullptr);
			auto const begin_value = context.get_struct_element(range_value, 0).get_value(context.builder);

			context.builder.CreateStore(begin_value, context.create_struct_gep(result_type, result_address, 0));
			return val_ptr::get_reference(result_address, result_type);
		}
		case ast::function_body::builtin_integer_range_end_iterator:
		{
			bz_assert(func_call.params.size() == 1);
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			bz_assert(result_type->isStructTy());
			bz_assert(result_type->getStructNumElements() == 1);
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}

			auto const range_value = emit_bitcode(func_call.params[0], context, nullptr);
			auto const end_value = context.get_struct_element(range_value, 1).get_value(context.builder);

			context.builder.CreateStore(end_value, context.create_struct_gep(result_type, result_address, 0));
			return val_ptr::get_reference(result_address, result_type);
		}
		case ast::function_body::builtin_integer_range_iterator_dereference:
		{
			bz_assert(func_call.params.size() == 1);
			auto const it_value = emit_bitcode(func_call.params[0], context, nullptr);
			auto const integer_value = context.get_struct_element(it_value, 0).get_value(context.builder);
			return value_or_result_address(integer_value, result_address, context);
		}
		case ast::function_body::builtin_integer_range_iterator_equals:
		{
			bz_assert(func_call.params.size() == 2);
			auto const lhs_it_value = emit_bitcode(func_call.params[0], context, nullptr);
			auto const rhs_it_value = emit_bitcode(func_call.params[1], context, nullptr);
			auto const lhs_integer_value = context.get_struct_element(lhs_it_value, 0).get_value(context.builder);
			auto const rhs_integer_value = context.get_struct_element(rhs_it_value, 0).get_value(context.builder);
			auto const result = context.builder.CreateICmpEQ(lhs_integer_value, rhs_integer_value);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::builtin_integer_range_iterator_not_equals:
		{
			bz_assert(func_call.params.size() == 2);
			auto const lhs_it_value = emit_bitcode(func_call.params[0], context, nullptr);
			auto const rhs_it_value = emit_bitcode(func_call.params[1], context, nullptr);
			auto const lhs_integer_value = context.get_struct_element(lhs_it_value, 0).get_value(context.builder);
			auto const rhs_integer_value = context.get_struct_element(rhs_it_value, 0).get_value(context.builder);
			auto const result = context.builder.CreateICmpNE(lhs_integer_value, rhs_integer_value);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::builtin_integer_range_iterator_plus_plus:
		{
			bz_assert(func_call.params.size() == 1);
			auto const it_value = emit_bitcode(func_call.params[0], context, nullptr);
			bz_assert(it_value.kind == val_ptr::reference);
			auto const integer_value_ref = context.get_struct_element(it_value, 0);
			bz_assert(integer_value_ref.kind == val_ptr::reference);
			auto const one_value = llvm::ConstantInt::get(integer_value_ref.get_type(), 1);
			auto const new_value = context.builder.CreateAdd(integer_value_ref.get_value(context.builder), one_value);
			context.builder.CreateStore(new_value, integer_value_ref.val);
			return it_value;
		}
		case ast::function_body::builtin_integer_range_iterator_minus_minus:
		{
			bz_assert(func_call.params.size() == 1);
			auto const it_value = emit_bitcode(func_call.params[0], context, nullptr);
			bz_assert(it_value.kind == val_ptr::reference);
			auto const integer_value_ref = context.get_struct_element(it_value, 0);
			bz_assert(integer_value_ref.kind == val_ptr::reference);
			auto const one_value = llvm::ConstantInt::get(integer_value_ref.get_type(), 1);
			auto const new_value = context.builder.CreateSub(integer_value_ref.get_value(context.builder), one_value);
			context.builder.CreateStore(new_value, integer_value_ref.val);
			return it_value;
		}
		case ast::function_body::builtin_integer_range_inclusive_begin_iterator:
		{
			bz_assert(func_call.params.size() == 1);
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			bz_assert(result_type->isStructTy());
			bz_assert(result_type->getStructNumElements() == 3);
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}

			auto const range_value = emit_bitcode(func_call.params[0], context, nullptr);
			auto const begin_value = context.get_struct_element(range_value, 0).get_value(context.builder);
			auto const end_value = context.get_struct_element(range_value, 1).get_value(context.builder);
			auto const false_value = context.builder.getFalse();

			context.builder.CreateStore(begin_value, context.create_struct_gep(result_type, result_address, 0));
			context.builder.CreateStore(end_value,   context.create_struct_gep(result_type, result_address, 1));
			context.builder.CreateStore(false_value, context.create_struct_gep(result_type, result_address, 2));
			return val_ptr::get_reference(result_address, result_type);
		}
		case ast::function_body::builtin_integer_range_inclusive_end_iterator:
		{
			bz_assert(func_call.params.size() == 1);
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			bz_assert(result_type->isStructTy());
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}

			emit_bitcode(func_call.params[0], context, nullptr);
			return val_ptr::get_reference(result_address, result_type);
		}
		case ast::function_body::builtin_integer_range_inclusive_iterator_dereference:
		{
			bz_assert(func_call.params.size() == 1);
			auto const it_value = emit_bitcode(func_call.params[0], context, nullptr);
			auto const integer_value = context.get_struct_element(it_value, 0).get_value(context.builder);
			return value_or_result_address(integer_value, result_address, context);
		}
		case ast::function_body::builtin_integer_range_inclusive_iterator_left_equals:
		{
			bz_assert(func_call.params.size() == 2);
			auto const it_value = emit_bitcode(func_call.params[0], context, nullptr);
			emit_bitcode(func_call.params[1], context, nullptr);
			auto const at_end = context.get_struct_element(it_value, 2).get_value(context.builder);
			return value_or_result_address(at_end, result_address, context);
		}
		case ast::function_body::builtin_integer_range_inclusive_iterator_right_equals:
		{
			bz_assert(func_call.params.size() == 2);
			emit_bitcode(func_call.params[0], context, nullptr);
			auto const it_value = emit_bitcode(func_call.params[1], context, nullptr);
			auto const at_end = context.get_struct_element(it_value, 2).get_value(context.builder);
			return value_or_result_address(at_end, result_address, context);
		}
		case ast::function_body::builtin_integer_range_inclusive_iterator_left_not_equals:
		{
			bz_assert(func_call.params.size() == 2);
			auto const it_value = emit_bitcode(func_call.params[0], context, nullptr);
			emit_bitcode(func_call.params[1], context, nullptr);
			auto const at_end = context.get_struct_element(it_value, 2).get_value(context.builder);
			auto const result = context.builder.CreateNot(at_end);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::builtin_integer_range_inclusive_iterator_right_not_equals:
		{
			bz_assert(func_call.params.size() == 2);
			emit_bitcode(func_call.params[0], context, nullptr);
			auto const it_value = emit_bitcode(func_call.params[1], context, nullptr);
			auto const at_end = context.get_struct_element(it_value, 2).get_value(context.builder);
			auto const result = context.builder.CreateNot(at_end);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::builtin_integer_range_inclusive_iterator_plus_plus:
		{
			bz_assert(func_call.params.size() == 1);
			auto const it_value = emit_bitcode(func_call.params[0], context, nullptr);
			bz_assert(it_value.kind == val_ptr::reference);
			auto const integer_value_ref = context.get_struct_element(it_value, 0);
			bz_assert(integer_value_ref.kind == val_ptr::reference);
			auto const integer_value = integer_value_ref.get_value(context.builder);
			auto const end_value = context.get_struct_element(it_value, 1).get_value(context.builder);

			auto const begin_bb = context.builder.GetInsertBlock();
			auto const is_at_end = context.builder.CreateICmpEQ(integer_value, end_value);

			auto const increment_bb = context.add_basic_block("range_inclusive_plus_plus_increment");
			context.builder.SetInsertPoint(increment_bb);

			auto const one_value = llvm::ConstantInt::get(integer_value_ref.get_type(), 1);
			auto const new_value = context.builder.CreateAdd(integer_value_ref.get_value(context.builder), one_value);
			context.builder.CreateStore(new_value, integer_value_ref.val);

			auto const at_end_bb = context.add_basic_block("range_inclusive_plus_plus_at_end");
			context.builder.SetInsertPoint(at_end_bb);
			auto const at_end_ref = context.get_struct_element(it_value, 2);
			context.builder.CreateStore(context.builder.getTrue(), at_end_ref.val);

			auto const end_bb = context.add_basic_block("range_inclusive_plus_plus_end");
			context.builder.SetInsertPoint(begin_bb);
			context.builder.CreateCondBr(is_at_end, at_end_bb, increment_bb);
			context.builder.SetInsertPoint(increment_bb);
			context.builder.CreateBr(end_bb);
			context.builder.SetInsertPoint(at_end_bb);
			context.builder.CreateBr(end_bb);

			context.builder.SetInsertPoint(end_bb);
			return it_value;
		}
		case ast::function_body::builtin_integer_range_from_begin_iterator:
		{
			bz_assert(func_call.params.size() == 1);
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			bz_assert(result_type->isStructTy());
			bz_assert(result_type->getStructNumElements() == 1);
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}

			auto const range_value = emit_bitcode(func_call.params[0], context, nullptr);
			auto const begin_value = context.get_struct_element(range_value, 0).get_value(context.builder);

			context.builder.CreateStore(begin_value, context.create_struct_gep(result_type, result_address, 0));
			return val_ptr::get_reference(result_address, result_type);
		}
		case ast::function_body::builtin_integer_range_from_end_iterator:
		{
			bz_assert(func_call.params.size() == 1);
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			bz_assert(result_type->isStructTy());
			if (result_address == nullptr)
			{
				result_address = context.create_alloca(result_type);
			}

			emit_bitcode(func_call.params[0], context, nullptr);
			return val_ptr::get_reference(result_address, result_type);
		}
		case ast::function_body::builtin_integer_range_from_iterator_dereference:
		{
			bz_assert(func_call.params.size() == 1);
			auto const it_value = emit_bitcode(func_call.params[0], context, nullptr);
			auto const integer_value = context.get_struct_element(it_value, 0).get_value(context.builder);
			return value_or_result_address(integer_value, result_address, context);
		}
		case ast::function_body::builtin_integer_range_from_iterator_left_equals:
		case ast::function_body::builtin_integer_range_from_iterator_right_equals:
		{
			bz_assert(func_call.params.size() == 2);
			emit_bitcode(func_call.params[0], context, nullptr);
			emit_bitcode(func_call.params[1], context, nullptr);
			return value_or_result_address(context.builder.getFalse(), result_address, context);
		}
		case ast::function_body::builtin_integer_range_from_iterator_left_not_equals:
		case ast::function_body::builtin_integer_range_from_iterator_right_not_equals:
		{
			bz_assert(func_call.params.size() == 2);
			emit_bitcode(func_call.params[0], context, nullptr);
			emit_bitcode(func_call.params[1], context, nullptr);
			return value_or_result_address(context.builder.getTrue(), result_address, context);
		}
		case ast::function_body::builtin_integer_range_from_iterator_plus_plus:
		{
			bz_assert(func_call.params.size() == 1);
			auto const it_value = emit_bitcode(func_call.params[0], context, nullptr);
			bz_assert(it_value.kind == val_ptr::reference);
			auto const integer_value_ref = context.get_struct_element(it_value, 0);
			bz_assert(integer_value_ref.kind == val_ptr::reference);
			auto const one_value = llvm::ConstantInt::get(integer_value_ref.get_type(), 1);
			auto const new_value = context.builder.CreateAdd(integer_value_ref.get_value(context.builder), one_value);
			context.builder.CreateStore(new_value, integer_value_ref.val);
			return it_value;
		}
		case ast::function_body::builtin_optional_get_value_ref:
		case ast::function_body::builtin_optional_get_mut_value_ref:
		{
			bz_assert(func_call.params.size() == 1);
			auto const optional_val = emit_bitcode(func_call.params[0], context, nullptr);
			emit_null_optional_get_value_check(func_call.src_tokens, optional_val, context);
			bz_assert(result_address == nullptr);
			return optional_get_value_ptr(optional_val, context);
		}
		case ast::function_body::builtin_optional_get_value:
			bz_unreachable;
		case ast::function_body::builtin_pointer_cast:
		{
			bz_assert(func_call.params.size() == 2);
			bz_assert(func_call.params[0].is_typename());
			auto const ptr = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			bz_assert(ptr->getType() == get_llvm_type(func_call.params[0].get_typename(), context));
			return value_or_result_address(ptr, result_address, context);
		}
		case ast::function_body::builtin_pointer_to_int:
		{
			bz_assert(func_call.params.size() == 1);
			auto const ptr = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			bz_assert(ptr->getType()->isPointerTy());
			auto const result = context.builder.CreatePtrToInt(ptr, context.get_usize_t());
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::builtin_int_to_pointer:
		{
			bz_assert(func_call.params.size() == 2);
			bz_assert(func_call.params[0].is_typename());
			auto const dest_type = get_llvm_type(func_call.params[0].get_typename(), context);
			auto const val = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			bz_assert(val->getType()->isIntegerTy());
			auto const result = context.builder.CreateIntToPtr(val, dest_type);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::builtin_enum_value:
			bz_assert(func_call.params.size() == 1);
			return emit_bitcode(func_call.params[0], context, result_address);
		case ast::function_body::builtin_destruct_value:
			// this is already handled in src/ctx/parse_context.cpp, in the function make_expr_function_call_from_body
			bz_unreachable;
		case ast::function_body::builtin_inplace_construct:
		{
			bz_assert(func_call.params.size() == 2);
			auto const dest_ptr = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			emit_bitcode(func_call.params[1], context, dest_ptr);
			return val_ptr::get_none();
		}
		case ast::function_body::builtin_swap:
			// this is already handled in src/ctx/parse_context.cpp, in the function make_expr_function_call_from_body
			bz_unreachable;
		case ast::function_body::builtin_is_comptime:
		{
			return value_or_result_address(context.builder.getFalse(), result_address, context);
		}
		case ast::function_body::builtin_panic:
		{
			auto const handler_fn = context.get_builtin_function(ast::function_body::builtin_panic_handler);
			if (handler_fn == nullptr)
			{
				context.builder.CreateIntrinsic(llvm::Intrinsic::trap, {});
				return val_ptr::get_none();
			}

			bz_assert(func_call.params.size() == 1);
			auto const param = emit_bitcode(func_call.params[0], context, nullptr);
			auto const param_type = func_call.func_body->params[0].get_type().as_typespec_view();
			auto const param_llvm_type = context.get_str_t();

			ast::arena_vector<llvm::Value *> params = {};
			params.reserve(2); // on linux str is passed in two registers
			ast::arena_vector<is_byval_and_type_pair> params_is_byval = {};
			params.reserve(2);
			add_call_parameter<false>(param_type, param_llvm_type, param, params, params_is_byval, context);
			auto const panic_handler = context.get_function(handler_fn);
			auto const call = context.create_call(panic_handler, llvm::ArrayRef(params.data(), params.size()));
			bz_assert(panic_handler->arg_size() == call->arg_size());
			for (auto const &[is_byval, i] : params_is_byval.enumerate())
			{
				if (is_byval.is_byval)
				{
					add_byval_attributes(call, is_byval.type, i, context);
				}
			}

			// just to be sure...
			context.builder.CreateIntrinsic(llvm::Intrinsic::trap, {});

			return val_ptr::get_none();
		}

		case ast::function_body::trivially_copy_values:
		{
			bz_assert(func_call.params.size() == 3);
			auto const dest = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const source = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const count = emit_bitcode(func_call.params[2], context, nullptr).get_value(context.builder);
			bz_assert(func_call.params[0].get_expr_type().is_optional_pointer());
			auto const type = get_llvm_type(func_call.params[0].get_expr_type().get_optional_pointer(), context);
			auto const type_size = llvm::ConstantInt::get(count->getType(), context.get_size(type));
			auto const size = context.builder.CreateMul(count, type_size);

			auto const align = context.get_data_layout().getPrefTypeAlign(type);
			context.builder.CreateMemCpy(dest, align, source, align, size);
			return val_ptr::get_none();
		}
		case ast::function_body::trivially_copy_overlapping_values:
		{
			bz_assert(func_call.params.size() == 3);
			auto const dest = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const source = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const count = emit_bitcode(func_call.params[2], context, nullptr).get_value(context.builder);
			bz_assert(func_call.params[0].get_expr_type().is_optional_pointer());
			auto const type = get_llvm_type(func_call.params[0].get_expr_type().get_optional_pointer(), context);
			auto const type_size = llvm::ConstantInt::get(count->getType(), context.get_size(type));
			auto const size = context.builder.CreateMul(count, type_size);

			auto const align = context.get_data_layout().getPrefTypeAlign(type);
			context.builder.CreateMemMove(dest, align, source, align, size);
			return val_ptr::get_none();
		}
		case ast::function_body::trivially_relocate_values:
		{
			bz_assert(func_call.params.size() == 3);
			auto const dest = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const source = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const count = emit_bitcode(func_call.params[2], context, nullptr).get_value(context.builder);
			bz_assert(func_call.params[0].get_expr_type().is_optional_pointer());
			auto const type = get_llvm_type(func_call.params[0].get_expr_type().get_optional_pointer(), context);
			auto const type_size = llvm::ConstantInt::get(count->getType(), context.get_size(type));
			auto const size = context.builder.CreateMul(count, type_size);

			auto const align = context.get_data_layout().getPrefTypeAlign(type);
			context.builder.CreateMemMove(dest, align, source, align, size);
			return val_ptr::get_none();
		}
		case ast::function_body::trivially_set_values:
		{
			bz_assert(func_call.params.size() == 3);
			auto const dest = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const value = emit_bitcode(func_call.params[1], context, nullptr);
			auto const count = emit_bitcode(func_call.params[2], context, nullptr).get_value(context.builder);
			llvm::Type * const type = value.get_type();

			if (type == context.get_uint8_t())
			{
				context.builder.CreateMemSet(dest, value.get_value(context.builder), count, std::nullopt);
				return val_ptr::get_none();
			}
			else
			{
				auto const value_to_copy = type->isAggregateType() ? value : val_ptr::get_value(value.get_value(context.builder));
				auto const end = context.create_gep(type, dest, count);

				auto const begin_bb = context.builder.GetInsertBlock();

				auto const condition_check_bb = context.add_basic_block("trivially_set_values_condition_check");
				context.builder.CreateBr(condition_check_bb);
				context.builder.SetInsertPoint(condition_check_bb);
				auto const it = context.builder.CreatePHI(dest->getType(), 2);
				it->addIncoming(dest, begin_bb);

				auto const should_continue = context.builder.CreateICmpNE(it, end);

				auto const loop_bb = context.add_basic_block("trivially_set_values_loop");
				auto const end_bb = context.add_basic_block("trivially_set_values_end");

				context.builder.CreateCondBr(should_continue, loop_bb, end_bb);

				context.builder.SetInsertPoint(loop_bb);
				emit_value_copy(value_to_copy, it, context);
				auto const next_it = context.builder.CreateConstGEP1_64(type, it, 1);
				context.builder.CreateBr(condition_check_bb);
				it->addIncoming(next_it, loop_bb);

				context.builder.SetInsertPoint(end_bb);
				return val_ptr::get_none();
			}
		}
		case ast::function_body::bit_cast:
			// this handled as a separate expression
			bz_unreachable;

		case ast::function_body::trap:
		{
			bz_assert(func_call.params.size() == 0);
			context.builder.CreateIntrinsic(llvm::Intrinsic::trap, {});
			bz_assert(result_address == nullptr);
			return val_ptr::get_none();
		}
		case ast::function_body::memcpy:
		{
			bz_assert(func_call.params.size() == 3);
			auto const dest = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const src = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const n = emit_bitcode(func_call.params[2], context, nullptr).get_value(context.builder);
			context.builder.CreateMemCpy(dest, std::nullopt, src, std::nullopt, n);
			bz_assert(result_address == nullptr);
			return val_ptr::get_none();
		}
		case ast::function_body::memmove:
		{
			bz_assert(func_call.params.size() == 3);
			auto const dest = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const src = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const n = emit_bitcode(func_call.params[2], context, nullptr).get_value(context.builder);
			context.builder.CreateMemMove(dest, std::nullopt, src, std::nullopt, n);
			bz_assert(result_address == nullptr);
			return val_ptr::get_none();
		}
		case ast::function_body::memset:
		{
			bz_assert(func_call.params.size() == 3);
			auto const dest = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const val = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const n = emit_bitcode(func_call.params[2], context, nullptr).get_value(context.builder);
			context.builder.CreateMemSet(dest, val, n, std::nullopt);
			bz_assert(result_address == nullptr);
			return val_ptr::get_none();
		}

		// https://llvm.org/docs/LangRef.html#llvm-is-fpclass-intrinsic
		case ast::function_body::isnan_f32:
		case ast::function_body::isnan_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			unsigned const test =
				(1u << 0) // signaling nan
				| (1u << 1); // quiet nan
			auto const result = context.builder.createIsFPClass(x, test);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::isinf_f32:
		case ast::function_body::isinf_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			unsigned const test =
				(1u << 2) // negative infinity
				| (1u << 9); // positive infinity
			auto const result = context.builder.createIsFPClass(x, test);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::isfinite_f32:
		case ast::function_body::isfinite_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			unsigned const test =
				(1u << 3) // negative normal
				| (1u << 4) // negative subnormal
				| (1u << 5) // negative zero
				| (1u << 6) // positive zero
				| (1u << 7) // positive subnormal
				| (1u << 8); // positive normal
			auto const result = context.builder.createIsFPClass(x, test);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::isnormal_f32:
		case ast::function_body::isnormal_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			unsigned const test =
				(1u << 3) // negative normal
				| (1u << 8); // positive normal
			auto const result = context.builder.createIsFPClass(x, test);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::issubnormal_f32:
		case ast::function_body::issubnormal_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			unsigned const test =
				(1u << 4) // negative subnormal
				| (1u << 7); // positive subnormal
			auto const result = context.builder.createIsFPClass(x, test);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::iszero_f32:
		case ast::function_body::iszero_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			unsigned const test =
				(1u << 5) // negative zero
				| (1u << 6); // positive zero
			auto const result = context.builder.createIsFPClass(x, test);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::abs_i8:
		case ast::function_body::abs_i16:
		case ast::function_body::abs_i32:
		case ast::function_body::abs_i64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const a = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateIntrinsic(
				llvm::Intrinsic::abs,
				a->getType(),
				{ a, context.builder.getFalse() }
			);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::abs_f32:
		case ast::function_body::abs_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const a = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::fabs, a);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::min_i8:
		case ast::function_body::min_i16:
		case ast::function_body::min_i32:
		case ast::function_body::min_i64:
		{
			bz_assert(func_call.params.size() == 2);
			auto const a = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const b = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateBinaryIntrinsic(llvm::Intrinsic::smin, a, b);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::min_u8:
		case ast::function_body::min_u16:
		case ast::function_body::min_u32:
		case ast::function_body::min_u64:
		{
			bz_assert(func_call.params.size() == 2);
			auto const a = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const b = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateBinaryIntrinsic(llvm::Intrinsic::umin, a, b);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::min_f32:
		case ast::function_body::min_f64:
		{
			bz_assert(func_call.params.size() == 2);
			auto const a = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const b = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateMinNum(a, b);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::max_i8:
		case ast::function_body::max_i16:
		case ast::function_body::max_i32:
		case ast::function_body::max_i64:
		{
			bz_assert(func_call.params.size() == 2);
			auto const a = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const b = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateBinaryIntrinsic(llvm::Intrinsic::smax, a, b);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::max_u8:
		case ast::function_body::max_u16:
		case ast::function_body::max_u32:
		case ast::function_body::max_u64:
		{
			bz_assert(func_call.params.size() == 2);
			auto const a = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const b = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateBinaryIntrinsic(llvm::Intrinsic::umax, a, b);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::max_f32:
		case ast::function_body::max_f64:
		{
			bz_assert(func_call.params.size() == 2);
			auto const a = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const b = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateMaxNum(a, b);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::exp_f32:
		case ast::function_body::exp_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::exp, x);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::exp2_f32:
		case ast::function_body::exp2_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::exp2, x);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::log_f32:
		case ast::function_body::log_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::log, x);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::log10_f32:
		case ast::function_body::log10_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::log10, x);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::log2_f32:
		case ast::function_body::log2_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::log2, x);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::sqrt_f32:
		case ast::function_body::sqrt_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::sqrt, x);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::pow_f32:
		case ast::function_body::pow_f64:
		{
			bz_assert(func_call.params.size() == 2);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const y = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateBinaryIntrinsic(llvm::Intrinsic::pow, x, y);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::sin_f32:
		case ast::function_body::sin_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::sin, x);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::cos_f32:
		case ast::function_body::cos_f64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const x = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::cos, x);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::bitreverse_u8:
		case ast::function_body::bitreverse_u16:
		case ast::function_body::bitreverse_u32:
		case ast::function_body::bitreverse_u64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const n = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::bitreverse, n);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::popcount_u8:
		case ast::function_body::popcount_u16:
		case ast::function_body::popcount_u32:
		case ast::function_body::popcount_u64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const n = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::ctpop, n);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::byteswap_u16:
		case ast::function_body::byteswap_u32:
		case ast::function_body::byteswap_u64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const n = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateUnaryIntrinsic(llvm::Intrinsic::bswap, n);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::clz_u8:
		case ast::function_body::clz_u16:
		case ast::function_body::clz_u32:
		case ast::function_body::clz_u64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const n = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateIntrinsic(
				llvm::Intrinsic::ctlz,
				n->getType(),
				{ n, context.builder.getFalse() }
			);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::ctz_u8:
		case ast::function_body::ctz_u16:
		case ast::function_body::ctz_u32:
		case ast::function_body::ctz_u64:
		{
			bz_assert(func_call.params.size() == 1);
			auto const n = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateIntrinsic(
				llvm::Intrinsic::cttz,
				n->getType(),
				{ n, context.builder.getFalse() }
			);
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::fshl_u8:
		case ast::function_body::fshl_u16:
		case ast::function_body::fshl_u32:
		case ast::function_body::fshl_u64:
		{
			bz_assert(func_call.params.size() == 3);
			auto const a = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const b = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const amount = emit_bitcode(func_call.params[2], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateIntrinsic(llvm::Intrinsic::fshl, { a->getType() }, { a, b, amount });
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::fshr_u8:
		case ast::function_body::fshr_u16:
		case ast::function_body::fshr_u32:
		case ast::function_body::fshr_u64:
		{
			bz_assert(func_call.params.size() == 3);
			auto const a = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const b = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const amount = emit_bitcode(func_call.params[2], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateIntrinsic(llvm::Intrinsic::fshr, { a->getType() }, { a, b, amount });
			return value_or_result_address(result, result_address, context);
		}
		case ast::function_body::arithmetic_shift_right_u8:
		case ast::function_body::arithmetic_shift_right_u16:
		case ast::function_body::arithmetic_shift_right_u32:
		case ast::function_body::arithmetic_shift_right_u64:
		{
			bz_assert(func_call.params.size() == 2);
			auto const n = emit_bitcode(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const amount = emit_bitcode(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const result = context.builder.CreateAShr(n, amount);
			return value_or_result_address(result, result_address, context);
		}

		case ast::function_body::comptime_malloc:
		case ast::function_body::comptime_free:
		case ast::function_body::comptime_print:
		case ast::function_body::comptime_compile_error:
		case ast::function_body::comptime_compile_warning:
		case ast::function_body::comptime_add_global_array_data:
		case ast::function_body::comptime_create_global_string:
		case ast::function_body::comptime_concatenate_strs:
		case ast::function_body::typename_as_str:
		case ast::function_body::is_mut:
		case ast::function_body::is_consteval:
		case ast::function_body::is_pointer:
		case ast::function_body::is_optional:
		case ast::function_body::is_reference:
		case ast::function_body::is_move_reference:
		case ast::function_body::is_slice:
		case ast::function_body::is_array:
		case ast::function_body::is_tuple:
		case ast::function_body::is_enum:
		case ast::function_body::remove_mut:
		case ast::function_body::remove_consteval:
		case ast::function_body::remove_pointer:
		case ast::function_body::remove_optional:
		case ast::function_body::remove_reference:
		case ast::function_body::remove_move_reference:
		case ast::function_body::slice_value_type:
		case ast::function_body::array_value_type:
		case ast::function_body::tuple_value_type:
		case ast::function_body::concat_tuple_types:
		case ast::function_body::enum_underlying_type:
		case ast::function_body::is_default_constructible:
		case ast::function_body::is_copy_constructible:
		case ast::function_body::is_trivially_copy_constructible:
		case ast::function_body::is_move_constructible:
		case ast::function_body::is_trivially_move_constructible:
		case ast::function_body::is_trivially_destructible:
		case ast::function_body::is_trivially_move_destructible:
		case ast::function_body::is_trivially_relocatable:
		case ast::function_body::is_trivial:
		case ast::function_body::create_initialized_array: // this is handled as a separate expression
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
			// these functions are guaranteed to be evaluated at compile time
			bz_unreachable;

		case ast::function_body::builtin_unary_plus:
			return emit_builtin_unary_plus(func_call.params[0], context, result_address);
		case ast::function_body::builtin_unary_minus:
			return emit_builtin_unary_minus(func_call.params[0], context, result_address);
		case ast::function_body::builtin_unary_dereference:
			bz_assert(result_address == nullptr);
			return emit_builtin_unary_dereference(func_call.src_tokens, func_call.params[0], context);
		case ast::function_body::builtin_unary_bit_not:
			return emit_builtin_unary_bit_not(func_call.params[0], context, result_address);
		case ast::function_body::builtin_unary_bool_not:
			return emit_builtin_unary_bool_not(func_call.params[0], context, result_address);
		case ast::function_body::builtin_unary_plus_plus:
			bz_assert(result_address == nullptr);
			return emit_builtin_unary_plus_plus(func_call.params[0], context);
		case ast::function_body::builtin_unary_minus_minus:
			bz_assert(result_address == nullptr);
			return emit_builtin_unary_minus_minus(func_call.params[0], context);

		case ast::function_body::builtin_binary_assign:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_assign(func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_plus:
			return emit_builtin_binary_plus(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_plus_eq:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_plus_eq(func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_minus:
			return emit_builtin_binary_minus(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_minus_eq:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_minus_eq(func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_multiply:
			return emit_builtin_binary_multiply(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_multiply_eq:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_multiply_eq(func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_divide:
			return emit_builtin_binary_divide(func_call.src_tokens, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_divide_eq:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_divide_eq(func_call.src_tokens, func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_modulo:
			return emit_builtin_binary_modulo(func_call.src_tokens, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_modulo_eq:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_modulo_eq(func_call.src_tokens, func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_equals:
			return emit_builtin_binary_cmp(lex::token::equals, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_not_equals:
			return emit_builtin_binary_cmp(lex::token::not_equals, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_less_than:
			return emit_builtin_binary_cmp(lex::token::less_than, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_less_than_eq:
			return emit_builtin_binary_cmp(lex::token::less_than_eq, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_greater_than:
			return emit_builtin_binary_cmp(lex::token::greater_than, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_greater_than_eq:
			return emit_builtin_binary_cmp(lex::token::greater_than_eq, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_and:
			return emit_builtin_binary_bit_and(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_and_eq:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_bit_and_eq(func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_bit_xor:
			return emit_builtin_binary_bit_xor(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_xor_eq:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_bit_xor_eq(func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_bit_or:
			return emit_builtin_binary_bit_or(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_or_eq:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_bit_or_eq(func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_bit_left_shift:
			return emit_builtin_binary_left_shift(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_left_shift_eq:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_left_shift_eq(func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_bit_right_shift:
			return emit_builtin_binary_right_shift(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_right_shift_eq:
			bz_assert(result_address == nullptr);
			return emit_builtin_binary_right_shift_eq(func_call.params[0], func_call.params[1], context);
		case ast::function_body::builtin_binary_subscript:
			// integer subscripts are handled as separate expressions, because of lifetime complexity
			return emit_builtin_subscript_range(func_call.params[0], func_call.params[1], context, result_address);

		default:
			break;
		}
	}
	bz_assert(!func_call.func_body->is_default_copy_constructor());
	bz_assert(!func_call.func_body->is_default_move_constructor());
	bz_assert(!func_call.func_body->is_default_default_constructor());
	bz_assert(!func_call.func_body->is_default_op_assign());
	bz_assert(!func_call.func_body->is_default_op_move_assign());

	bz_assert(func_call.func_body != nullptr);

	auto const fn = context.get_function(func_call.func_body);
	bz_assert(fn != nullptr);

	auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
	auto const result_kind = context.get_pass_kind(func_call.func_body->return_type, result_type);

	auto const [args, args_is_byval] = emit_function_call_args(
		result_type,
		result_kind,
		func_call,
		context,
		result_address
	);

	return emit_function_call(
		func_call.func_body->return_type,
		result_type,
		result_kind,
		fn->getFunctionType(),
		fn,
		fn->getCallingConv(),
		args,
		args_is_byval,
		context,
		result_address
	);
}

static call_args_info_t emit_function_call_args(
	llvm::Type *result_type,
	abi::pass_kind result_kind,
	ast::expr_indirect_function_call const &func_call,
	bz::array_view<ast::typespec const> param_types,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	ast::arena_vector<llvm::Value *> args = {};
	ast::arena_vector<is_byval_and_type_pair> args_is_byval = {};
	args.reserve(
		func_call.params.size()
		+ (result_kind == abi::pass_kind::reference || result_kind == abi::pass_kind::non_trivial ? 1 : 0)
	);
	args_is_byval.reserve(
		func_call.params.size()
		+ (result_kind == abi::pass_kind::reference || result_kind == abi::pass_kind::non_trivial ? 1 : 0)
	);

	auto const emit_arg = [&]<bool push_to_front>(size_t i, bz::meta::integral_constant<bool, push_to_front>)
	{
		auto &p = func_call.params[i];
		auto &param_type = param_types[i];
		if (p.is_error())
		{
			auto const param_llvm_type = get_llvm_type(param_type, context);
			emit_bitcode(p, context, nullptr);
			auto const param_val = val_ptr::get_value(llvm::UndefValue::get(param_llvm_type));
			add_call_parameter<push_to_front>(param_type, param_llvm_type, param_val, args, args_is_byval, context);
		}
		else
		{
			auto const param_llvm_type = get_llvm_type(param_type, context);
			auto const param_val = emit_bitcode(p, context, nullptr);
			bz_assert(param_val.val != nullptr || param_val.consteval_val != nullptr);
			add_call_parameter<push_to_front>(param_type, param_llvm_type, param_val, args, args_is_byval, context);
		}
	};

	for (auto const i : bz::iota(0, func_call.params.size()))
	{
		emit_arg(i, bz::meta::integral_constant<bool, false>{});
	}

	if (result_kind == abi::pass_kind::reference || result_kind == abi::pass_kind::non_trivial)
	{
		auto const output_ptr = result_address != nullptr
			? result_address
			: context.create_alloca(result_type);
		args.push_front(output_ptr);
		args_is_byval.push_front({ false, nullptr });
	}

	return { std::move(args), std::move(args_is_byval) };
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_indirect_function_call const &func_call,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const fn_type = func_call.called.get_expr_type().remove_mut_reference();
	bz_assert(fn_type.is<ast::ts_function>());
	auto const return_type = fn_type.get<ast::ts_function>().return_type.as_typespec_view();

	auto const result_type = get_llvm_type(return_type, context);
	auto const result_kind = context.get_pass_kind(return_type, result_type);

	auto const called = emit_bitcode(func_call.called, context, result_address);
	auto const fn = called.get_value(context.builder);

	auto const [args, args_is_byval] = emit_function_call_args(
		result_type,
		result_kind,
		func_call,
		fn_type.get<ast::ts_function>().param_types,
		context,
		result_address
	);

	auto const fn_llvm_type = [&, &args = args]() {
		auto const param_llvm_types = args
			.transform([](auto const arg) { return arg->getType(); })
			.template collect<ast::arena_vector>();
		return llvm::FunctionType::get(
			result_type,
			llvm::ArrayRef<llvm::Type *>(param_llvm_types.data(), param_llvm_types.size()),
			false
		);
	}();
	return emit_function_call(
		return_type,
		result_type,
		result_kind,
		fn_llvm_type,
		fn,
		llvm::CallingConv::C,
		args,
		args_is_byval,
		context,
		result_address
	);
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_tuple_subscript const &tuple_subscript,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(tuple_subscript.index.is<ast::constant_expression>());
	auto const &index_value = tuple_subscript.index.get<ast::constant_expression>().value;
	bz_assert(index_value.is_uint() || index_value.is_sint());
	auto const index_int_value = index_value.is_uint()
		? index_value.get_uint()
		: static_cast<uint64_t>(index_value.get_sint());

	val_ptr result = val_ptr::get_none();
	for (auto const i : bz::iota(0, tuple_subscript.base.elems.size()))
	{
		if (i == index_int_value)
		{
			result = emit_bitcode(tuple_subscript.base.elems[i], context, result_address);
		}
		else
		{
			emit_bitcode(tuple_subscript.base.elems[i], context, nullptr);
		}
	}
	return result;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_rvalue_tuple_subscript const &rvalue_tuple_subscript,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(rvalue_tuple_subscript.index.is<ast::constant_expression>());
	auto const &index_value = rvalue_tuple_subscript.index.get<ast::constant_expression>().value;
	bz_assert(index_value.is_uint() || index_value.is_sint());
	auto const index_int_value = index_value.is_uint()
		? index_value.get_uint()
		: static_cast<uint64_t>(index_value.get_sint());

	auto const base_val = emit_bitcode(rvalue_tuple_subscript.base, context, nullptr);
	bz_assert(base_val.kind == val_ptr::reference);
	auto const is_reference_result = rvalue_tuple_subscript.elem_refs[index_int_value].get_expr_type().is_reference();

	val_ptr result = val_ptr::get_none();
	for (auto const i : bz::iota(0, rvalue_tuple_subscript.elem_refs.size()))
	{
		if (rvalue_tuple_subscript.elem_refs[i].is_null())
		{
			continue;
		}

		auto const elem_ptr = [&]() {
			if (i == index_int_value && is_reference_result)
			{
				auto const ref_ptr = base_val.kind == val_ptr::value
					? context.builder.CreateExtractValue(base_val.get_value(context.builder), index_int_value)
					: context.builder.CreateLoad(
						context.get_opaque_pointer_t(),
						context.create_struct_gep(base_val.get_type(), base_val.val, index_int_value)
					);
				auto const elem_ts = rvalue_tuple_subscript.elem_refs[index_int_value].get_expr_type();
				auto const elem_type = get_llvm_type(elem_ts.get_reference(), context);
				return val_ptr::get_reference(ref_ptr, elem_type);
			}
			else if (base_val.kind == val_ptr::value)
			{
				return val_ptr::get_value(context.builder.CreateExtractValue(base_val.get_value(context.builder), index_int_value));
			}
			else
			{
				auto const elem_type = base_val.get_type()->getStructElementType(i);
				auto const elem_ptr = context.create_struct_gep(base_val.get_type(), base_val.val, i);
				return val_ptr::get_reference(elem_ptr, elem_type);
			}
		}();
		auto const prev_value = context.push_value_reference(elem_ptr);
		if (i == index_int_value)
		{
			result = emit_bitcode(rvalue_tuple_subscript.elem_refs[i], context, result_address);
		}
		else
		{
			emit_bitcode(rvalue_tuple_subscript.elem_refs[i], context, nullptr);
		}
		context.pop_value_reference(prev_value);
	}
	return result;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_subscript const &subscript,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const base_type = subscript.base.get_expr_type().remove_mut_reference();
	if (base_type.is<ast::ts_array>())
	{
		auto const array = emit_bitcode(subscript.base, context, nullptr);
		auto index_val = emit_bitcode(subscript.index, context, nullptr).get_value(context.builder);
		bz_assert(subscript.index.get_expr_type().remove_any_mut().is<ast::ts_base_type>());
		auto const kind = subscript.index.get_expr_type().remove_any_mut().get<ast::ts_base_type>().info->kind;
		if (ast::is_unsigned_integer_kind(kind))
		{
			index_val = context.builder.CreateIntCast(index_val, context.get_usize_t(), false);
		}

		llvm::Value *result_ptr;
		if (array.kind == val_ptr::reference)
		{
			result_ptr = context.create_array_gep(array.get_type(), array.val, index_val);
		}
		else
		{
			auto const array_value = array.get_value(context.builder);
			auto const array_type = array_value->getType();
			auto const array_address = context.create_alloca(array_type);
			context.builder.CreateStore(array_value, array_address);
			result_ptr = context.create_array_gep(array_type, array_address, index_val);
		}

		auto const elem_type = base_type.get<ast::ts_array>().elem_type.as_typespec_view();
		auto const elem_llvm_type = get_llvm_type(elem_type, context);

		bz_assert(result_address == nullptr);
		return val_ptr::get_reference(result_ptr, elem_llvm_type);
	}
	else if (base_type.is<ast::ts_array_slice>())
	{
		auto const array = emit_bitcode(subscript.base, context, nullptr);
		auto const array_val = array.get_value(context.builder);
		auto const begin_ptr = context.builder.CreateExtractValue(array_val, 0);
		bz_assert(subscript.index.get_expr_type().remove_any_mut().is<ast::ts_base_type>());
		auto const kind = subscript.index.get_expr_type().remove_any_mut().get<ast::ts_base_type>().info->kind;
		auto index_val = emit_bitcode(subscript.index, context, nullptr).get_value(context.builder);
		if (ast::is_unsigned_integer_kind(kind))
		{
			index_val = context.builder.CreateIntCast(index_val, context.get_usize_t(), false);
		}

		auto const elem_type = base_type.get<ast::ts_array_slice>().elem_type.as_typespec_view();
		auto const elem_llvm_type = get_llvm_type(elem_type, context);

		auto const result_ptr = context.create_gep(elem_llvm_type, begin_ptr, index_val);

		bz_assert(result_address == nullptr);
		return val_ptr::get_reference(result_ptr, elem_llvm_type);
	}
	else
	{
		bz_assert(base_type.is<ast::ts_tuple>());
		auto const tuple = emit_bitcode(subscript.base, context, nullptr);
		bz_assert(subscript.index.is_constant());
		auto const &index_value = subscript.index.get_constant_value();
		bz_assert(index_value.is_uint() || index_value.is_sint());
		auto const index_int_value = index_value.is_uint()
			? index_value.get_uint()
			: static_cast<uint64_t>(index_value.get_sint());

		auto const accessed_type = base_type.is<ast::ts_tuple>()
			? base_type.get<ast::ts_tuple>().types[index_int_value].as_typespec_view()
			: subscript.base.get_tuple().elems[index_int_value].get_expr_type();

		if (tuple.kind == val_ptr::reference || (tuple.kind == val_ptr::value && accessed_type.is<ast::ts_lvalue_reference>()))
		{
			bz_assert(tuple.get_type()->isStructTy());
			auto const result_ptr = [&]() -> llvm::Value * {
				if (tuple.kind == val_ptr::value)
				{
					return context.builder.CreateExtractValue(tuple.get_value(context.builder), index_int_value);
				}
				else if (accessed_type.is<ast::ts_lvalue_reference>())
				{
					auto const ref_ptr = context.create_struct_gep(tuple.get_type(), tuple.val, index_int_value);
					return context.builder.CreateLoad(context.get_opaque_pointer_t(), ref_ptr);
				}
				else
				{
					return context.create_struct_gep(tuple.get_type(), tuple.val, index_int_value);
				}
			}();
			auto const result_type = get_llvm_type(accessed_type.remove_reference(), context);
			bz_assert(result_address == nullptr);
			return val_ptr::get_reference(result_ptr, result_type);
		}
		else
		{
			auto const result_val = context.builder.CreateExtractValue(tuple.get_value(context.builder), index_int_value);
			return value_or_result_address(result_val, result_address, context);
		}
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_rvalue_array_subscript const &subscript,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const array = emit_bitcode(subscript.base, context, nullptr);
	auto index_val = emit_bitcode(subscript.index, context, nullptr).get_value(context.builder);
	bz_assert(subscript.index.get_expr_type().remove_any_mut().is<ast::ts_base_type>());
	auto const kind = subscript.index.get_expr_type().remove_any_mut().get<ast::ts_base_type>().info->kind;
	if (ast::is_unsigned_integer_kind(kind))
	{
		index_val = context.builder.CreateIntCast(index_val, context.get_usize_t(), false);
	}

	if (array.kind == val_ptr::value)
	{
		auto const array_value = array.get_value(context.builder);
		auto const array_type = array_value->getType();
		auto const array_address = context.create_alloca(array_type);
		context.builder.CreateStore(array_value, array_address);
		auto const result_ptr = context.create_array_gep(array_type, array_address, index_val);
		return val_ptr::get_reference(result_ptr, array_type->getArrayElementType());
	}

	auto const array_type = array.get_type();
	auto const result_ptr = context.create_array_gep(array_type, array.val, index_val);
	auto const result_type = array.get_type()->getArrayElementType();

	context.push_rvalue_array_destruct_operation(subscript.elem_destruct_op, array.val, array_type, result_ptr);

	bz_assert(result_address == nullptr);
	return val_ptr::get_reference(result_ptr, result_type);
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_cast const &cast,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const expr_t = cast.expr.get_expr_type().remove_mut_reference();
	auto const dest_t = cast.type.remove_any_mut();

	if (expr_t.is<ast::ts_base_type>() && dest_t.is<ast::ts_base_type>())
	{
		auto const llvm_dest_t = get_llvm_type(dest_t, context);
		auto const expr = emit_bitcode(cast.expr, context, nullptr).get_value(context.builder);
		auto const expr_kind = expr_t.get<ast::ts_base_type>().info->kind;
		auto const dest_kind = dest_t.get<ast::ts_base_type>().info->kind;

		if (ast::is_integer_kind(expr_kind) && ast::is_integer_kind(dest_kind))
		{
			auto const result_val = context.builder.CreateIntCast(
				expr,
				llvm_dest_t,
				ast::is_signed_integer_kind(expr_kind),
				"cast_tmp"
			);
			return value_or_result_address(result_val, result_address, context);
		}
		else if (ast::is_floating_point_kind(expr_kind) && ast::is_floating_point_kind(dest_kind))
		{
			auto const result_val = context.builder.CreateFPCast(expr, llvm_dest_t, "cast_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
		else if (ast::is_floating_point_kind(expr_kind))
		{
			bz_assert(ast::is_integer_kind(dest_kind));
			auto const result_val = ast::is_signed_integer_kind(dest_kind)
				? context.builder.CreateFPToSI(expr, llvm_dest_t, "cast_tmp")
				: context.builder.CreateFPToUI(expr, llvm_dest_t, "cast_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
		else if (ast::is_integer_kind(expr_kind) && ast::is_floating_point_kind(dest_kind))
		{
			auto const result_val = ast::is_signed_integer_kind(expr_kind)
				? context.builder.CreateSIToFP(expr, llvm_dest_t, "cast_tmp")
				: context.builder.CreateUIToFP(expr, llvm_dest_t, "cast_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
		else if (expr_kind == ast::type_info::bool_ && ast::is_integer_kind(dest_kind))
		{
			auto const result_val = context.builder.CreateIntCast(expr, llvm_dest_t, false, "cast_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
		else
		{
			// this is a cast from i32 or to i32 in IR, so we emit an integer cast
			bz_assert(
				(expr_kind == ast::type_info::char_ && ast::is_integer_kind(dest_kind))
				|| ( ast::is_integer_kind(expr_kind) && dest_kind == ast::type_info::char_)
			);
			auto const result_val = context.builder.CreateIntCast(expr, llvm_dest_t, ast::is_signed_integer_kind(expr_kind), "cast_tmp");
			return value_or_result_address(result_val, result_address, context);
		}
	}
	else if (
		(expr_t.is<ast::ts_pointer>() || expr_t.is_optional_pointer())
		&& (dest_t.is<ast::ts_pointer>() || dest_t.is_optional_pointer())
	)
	{
		auto const ptr = emit_bitcode(cast.expr, context, nullptr).get_value(context.builder);
		bz_assert(ptr->getType() == get_llvm_type(dest_t, context));
		return value_or_result_address(ptr, result_address, context);
	}
	else if (expr_t.is<ast::ts_array>() && dest_t.is<ast::ts_array_slice>())
	{
		auto const expr_val = emit_bitcode(cast.expr, context, nullptr);
		auto const [begin_ptr, end_ptr] = [&]() {
			if (expr_val.kind == val_ptr::reference)
			{
				auto const begin_ptr = context.create_struct_gep(expr_val.get_type(), expr_val.val, 0);
				auto const end_ptr   = context.create_struct_gep(expr_val.get_type(), expr_val.val, expr_t.get<ast::ts_array>().size);
				return std::make_pair(begin_ptr, end_ptr);
			}
			else
			{
				auto const alloca = context.create_alloca(expr_val.get_type());
				context.builder.CreateStore(expr_val.get_value(context.builder), alloca);
				auto const begin_ptr = context.create_struct_gep(expr_val.get_type(), alloca, 0);
				auto const end_ptr   = context.create_struct_gep(expr_val.get_type(), alloca, expr_t.get<ast::ts_array>().size);
				return std::make_pair(begin_ptr, end_ptr);
			}
		}();
		auto const slice_t = get_llvm_type(dest_t, context);
		if (result_address == nullptr)
		{
			bz_assert(begin_ptr->getType()->isPointerTy());
			bz_assert(slice_t->isStructTy());
			auto const slice_struct_t = static_cast<llvm::StructType *>(slice_t);
			auto const slice_member_t = slice_struct_t->getElementType(0);
			auto const undef_value = llvm::UndefValue::get(slice_member_t);
			llvm::Value *result = llvm::ConstantStruct::get(slice_struct_t, undef_value, undef_value);
			result = context.builder.CreateInsertValue(result, begin_ptr, 0);
			result = context.builder.CreateInsertValue(result, end_ptr,   1);
			return val_ptr::get_value(result);
		}
		else
		{
			auto const result_begin_ptr = context.create_struct_gep(slice_t, result_address, 0);
			auto const result_end_ptr   = context.create_struct_gep(slice_t, result_address, 1);
			context.builder.CreateStore(begin_ptr, result_begin_ptr);
			context.builder.CreateStore(end_ptr, result_end_ptr);
			return val_ptr::get_reference(result_address, slice_t);
		}
	}
	else
	{
		bz_unreachable;
		return val_ptr::get_none();
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_bit_cast const &bit_cast,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const dest_type = get_llvm_type(bit_cast.type, context);
	if (result_address == nullptr)
	{
		result_address = context.create_alloca(dest_type);
	}

	emit_bitcode(bit_cast.expr, context, result_address);
	return val_ptr::get_reference(result_address, dest_type);
}

static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_optional_cast const &optional_cast,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	if (optional_cast.type.is_optional_reference())
	{
		auto const result = emit_bitcode(optional_cast.expr, context, nullptr);
		bz_assert(result.kind == val_ptr::reference);
		return value_or_result_address(result.val, result_address, context);
	}
	else if (auto const result_type = get_llvm_type(optional_cast.type, context); result_type->isPointerTy())
	{
		return emit_bitcode(optional_cast.expr, context, result_address);
	}
	else
	{
		bz_assert(result_address != nullptr);
		auto const result_val = val_ptr::get_reference(result_address, result_type);
		auto const value_ptr = optional_get_value_ptr(result_val, context);

		emit_bitcode(optional_cast.expr, context, value_ptr.val);
		optional_set_has_value(result_val, true, context);

		return result_val;
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_noop_forward const &noop_forward,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	return emit_bitcode(noop_forward.expr, context, result_address);
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_take_reference const &take_ref,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const result = emit_bitcode(take_ref.expr, context, nullptr);
	bz_assert(result_address == nullptr);
	return result;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_take_move_reference const &take_move_ref,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const result = emit_bitcode(take_move_ref.expr, context, nullptr);
	bz_assert(result_address == nullptr);
	if (result.kind == val_ptr::reference)
	{
		return result;
	}
	else
	{
		auto const type = result.get_type();
		auto const alloca = context.create_alloca(type);
		context.builder.CreateStore(result.get_value(context.builder), alloca);
		return val_ptr::get_reference(alloca, type);
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_aggregate_init const &aggregate_init,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const type = get_llvm_type(aggregate_init.type, context);
	bz_assert(type->isAggregateType());
	auto const result_ptr = result_address != nullptr ? result_address : context.create_alloca(type);
	for (auto const i : bz::iota(0, aggregate_init.exprs.size()))
	{
		auto const member_ptr = context.create_struct_gep(type, result_ptr, i);
		if (aggregate_init.exprs[i].get_expr_type().is_reference())
		{
			auto const ref = emit_bitcode(aggregate_init.exprs[i], context, nullptr);
			bz_assert(ref.kind == val_ptr::reference);
			context.builder.CreateStore(ref.val, member_ptr);
		}
		else
		{
			emit_bitcode(aggregate_init.exprs[i], context, member_ptr);
		}
	}
	return val_ptr::get_reference(result_ptr, type);
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_array_value_init const &array_value_init,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const llvm_type = get_llvm_type(array_value_init.type, context);
	if (result_address == nullptr)
	{
		result_address = context.create_alloca(llvm_type);
	}

	bz_assert(array_value_init.type.is<ast::ts_array>());
	auto const size = array_value_init.type.get<ast::ts_array>().size;
	if (size <= array_loop_threshold)
	{
		auto const value = emit_bitcode(array_value_init.value, context, nullptr);
		auto const prev_value = context.push_value_reference(value);
		for (auto const i : bz::iota(0, size))
		{
			auto const elem_result_address = context.create_struct_gep(llvm_type, result_address, i);
			emit_bitcode(array_value_init.copy_expr, context, elem_result_address);
		}
		context.pop_value_reference(prev_value);
		return val_ptr::get_reference(result_address, llvm_type);
	}
	else
	{
		auto const value = emit_bitcode(array_value_init.value, context, nullptr);
		auto const prev_value = context.push_value_reference(value);

		auto const loop_info = create_loop_start(size, context);

		auto const elem_result_address = context.create_array_gep(llvm_type, result_address, loop_info.iter_val);
		emit_bitcode(array_value_init.copy_expr, context, elem_result_address);

		create_loop_end(loop_info, context);

		context.pop_value_reference(prev_value);
		return val_ptr::get_reference(result_address, llvm_type);
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_aggregate_default_construct const &aggregate_default_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const type = get_llvm_type(aggregate_default_construct.type, context);
	bz_assert(type->isStructTy());
	auto const result_ptr = result_address != nullptr ? result_address : context.create_alloca(type);
	for (auto const i : bz::iota(0, aggregate_default_construct.default_construct_exprs.size()))
	{
		auto const member_ptr = context.create_struct_gep(type, result_ptr, i);
		emit_bitcode(aggregate_default_construct.default_construct_exprs[i], context, member_ptr);
	}
	return val_ptr::get_reference(result_ptr, type);
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_array_default_construct const &array_default_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const llvm_type = get_llvm_type(array_default_construct.type, context);
	if (result_address == nullptr)
	{
		result_address = context.create_alloca(llvm_type);
	}

	bz_assert(array_default_construct.type.is<ast::ts_array>());
	auto const size = array_default_construct.type.get<ast::ts_array>().size;
	if (size <= array_loop_threshold)
	{
		for (auto const i : bz::iota(0, size))
		{
			auto const elem_result_address = context.create_struct_gep(llvm_type, result_address, i);
			emit_bitcode(array_default_construct.default_construct_expr, context, elem_result_address);
		}
		return val_ptr::get_reference(result_address, llvm_type);
	}
	else
	{
		auto const loop_info = create_loop_start(size, context);

		auto const elem_result_address = context.create_array_gep(llvm_type, result_address, loop_info.iter_val);
		emit_bitcode(array_default_construct.default_construct_expr, context, elem_result_address);

		create_loop_end(loop_info, context);

		return val_ptr::get_reference(result_address, llvm_type);
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_optional_default_construct const &optional_default_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const llvm_type = get_llvm_type(optional_default_construct.type, context);

	if (llvm_type->isPointerTy())
	{
		bz_assert(llvm_type == context.get_opaque_pointer_t());
		auto const value = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(llvm_type));
		return value_or_result_address(value, result_address, context);
	}
	else
	{
		if (result_address == nullptr)
		{
			result_address = context.create_alloca(llvm_type);
		}

		auto const result = val_ptr::get_reference(result_address, llvm_type);
		optional_set_has_value(result, false, context);

		return result;
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_builtin_default_construct const &builtin_default_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const type = builtin_default_construct.type.as_typespec_view();
	if (type.is<ast::ts_array_slice>())
	{
		auto const ptr_type = context.get_opaque_pointer_t();
		auto const result_type = context.get_slice_t();
		auto const null_value = llvm::ConstantPointerNull::get(ptr_type);
		if (result_address != nullptr)
		{
			auto const begin_ptr = context.create_struct_gep(result_type, result_address, 0);
			auto const end_ptr   = context.create_struct_gep(result_type, result_address, 1);
			bz_assert(begin_ptr->getType() == end_ptr->getType());
			context.builder.CreateStore(null_value, begin_ptr);
			context.builder.CreateStore(null_value, end_ptr);
			return val_ptr::get_reference(result_address, result_type);
		}
		else
		{
			return val_ptr::get_value(llvm::ConstantStruct::get(result_type, null_value, null_value));
		}
	}
	else
	{
		bz_unreachable;
		return val_ptr::get_none();
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_aggregate_copy_construct const &aggregate_copy_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const copied_val = emit_bitcode(aggregate_copy_construct.copied_value, context, nullptr);
	auto const type = copied_val.get_type();
	bz_assert(type->isStructTy());
	auto const result_ptr = result_address != nullptr ? result_address : context.create_alloca(type);
	for (auto const i : bz::iota(0, aggregate_copy_construct.copy_exprs.size()))
	{
		auto const result_member_ptr = context.create_struct_gep(type, result_ptr, i);
		auto const member_val = copied_val.kind == val_ptr::reference
			? val_ptr::get_reference(context.create_struct_gep(type, copied_val.val, i), type->getStructElementType(i))
			: val_ptr::get_value(context.builder.CreateExtractValue(copied_val.get_value(context.builder), i));
		auto const prev_value = context.push_value_reference(member_val);
		emit_bitcode(aggregate_copy_construct.copy_exprs[i], context, result_member_ptr);
		context.pop_value_reference(prev_value);
	}
	return val_ptr::get_reference(result_ptr, type);
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_array_copy_construct const &array_copy_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const copied_val = emit_bitcode(array_copy_construct.copied_value, context, nullptr);

	auto const type = copied_val.get_type();
	bz_assert(type->isArrayTy());
	auto const elem_type = type->getArrayElementType();
	auto const size = type->getArrayNumElements();
	auto const result_ptr = result_address != nullptr ? result_address : context.create_alloca(type);

	if (copied_val.kind == val_ptr::value)
	{
		context.builder.CreateStore(copied_val.get_value(context.builder), result_ptr);
		return val_ptr::get_reference(result_ptr, type);
	}

	if (size <= array_loop_threshold)
	{
		for (auto const i : bz::iota(0, size))
		{
			auto const result_elem_ptr = context.create_struct_gep(type, result_ptr, i);
			auto const elem_val = val_ptr::get_reference(context.create_struct_gep(type, copied_val.val, i), elem_type);
			auto const prev_value = context.push_value_reference(elem_val);
			emit_bitcode(array_copy_construct.copy_expr, context, result_elem_ptr);
			context.pop_value_reference(prev_value);
		}
		return val_ptr::get_reference(result_ptr, type);
	}
	else
	{
		auto const loop_info = create_loop_start(size, context);

		auto const result_elem_ptr = context.create_array_gep(type, result_ptr, loop_info.iter_val);
		auto const elem_val = val_ptr::get_reference(context.create_array_gep(type, copied_val.val, loop_info.iter_val), elem_type);
		auto const prev_value = context.push_value_reference(elem_val);
		emit_bitcode(array_copy_construct.copy_expr, context, result_elem_ptr);
		context.pop_value_reference(prev_value);

		create_loop_end(loop_info, context);

		return val_ptr::get_reference(result_ptr, type);
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_optional_copy_construct const &optional_copy_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const copied_val = emit_bitcode(optional_copy_construct.copied_value, context, nullptr);
	auto const type = copied_val.get_type();
	bz_assert(type->isStructTy());
	bz_assert(copied_val.kind == val_ptr::reference);

	auto const result = val_ptr::get_reference(
		result_address != nullptr ? result_address : context.create_alloca(type),
		type
	);
	auto const has_value = optional_has_value(copied_val, context);

	optional_set_has_value(result, has_value, context);

	auto const decide_bb = context.builder.GetInsertBlock();

	auto const copy_bb = context.add_basic_block("optional_copy_construct_has_value");
	context.builder.SetInsertPoint(copy_bb);

	auto const result_value_ptr = optional_get_value_ptr(result, context);
	auto const prev_value = context.push_value_reference(optional_get_value_ptr(copied_val, context));
	emit_bitcode(optional_copy_construct.value_copy_expr, context, result_value_ptr.val);
	context.pop_value_reference(prev_value);

	auto const end_bb = context.add_basic_block("optional_copy_construct_end");
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(decide_bb);
	context.builder.CreateCondBr(has_value, copy_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);

	return result;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_trivial_copy_construct const &trivial_copy_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const result_val = emit_bitcode(trivial_copy_construct.copied_value, context, nullptr);
	if (result_address == nullptr && result_val.get_type()->isAggregateType())
	{
		result_address = context.create_alloca(result_val.get_type());
	}

	if (result_address != nullptr)
	{
		emit_value_copy(result_val, result_address, context);
		return val_ptr::get_reference(result_address, result_val.get_type());
	}
	else
	{
		return val_ptr::get_value(result_val.get_value(context.builder));
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_aggregate_move_construct const &aggregate_move_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const moved_val = emit_bitcode(aggregate_move_construct.moved_value, context, nullptr);
	auto const type = moved_val.get_type();
	bz_assert(type->isStructTy());
	auto const result_ptr = result_address != nullptr ? result_address : context.create_alloca(type);
	for (auto const i : bz::iota(0, aggregate_move_construct.move_exprs.size()))
	{
		auto const result_member_ptr = context.create_struct_gep(type, result_ptr, i);
		auto const member_val = moved_val.kind == val_ptr::reference
			? val_ptr::get_reference(context.create_struct_gep(type, moved_val.val, i), type->getStructElementType(i))
			: val_ptr::get_value(context.builder.CreateExtractValue(moved_val.get_value(context.builder), i));
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(member_val);
		emit_bitcode(aggregate_move_construct.move_exprs[i], context, result_member_ptr);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}
	return val_ptr::get_reference(result_ptr, type);
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_array_move_construct const &array_move_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const moved_val = emit_bitcode(array_move_construct.moved_value, context, nullptr);

	auto const type = moved_val.get_type();
	bz_assert(type->isArrayTy());
	auto const elem_type = type->getArrayElementType();
	auto const size = type->getArrayNumElements();
	auto const result_ptr = result_address != nullptr ? result_address : context.create_alloca(type);

	if (moved_val.kind == val_ptr::value)
	{
		context.builder.CreateStore(moved_val.get_value(context.builder), result_ptr);
		return val_ptr::get_reference(result_ptr, type);
	}

	if (size <= array_loop_threshold)
	{
		for (auto const i : bz::iota(0, size))
		{
			auto const result_elem_ptr = context.create_struct_gep(type, result_ptr, i);
			auto const elem_val = val_ptr::get_reference(context.create_struct_gep(type, moved_val.val, i), elem_type);
			auto const prev_info = context.push_expression_scope();
			auto const prev_value = context.push_value_reference(elem_val);
			emit_bitcode(array_move_construct.move_expr, context, result_elem_ptr);
			context.pop_value_reference(prev_value);
			context.pop_expression_scope(prev_info);
		}
		return val_ptr::get_reference(result_ptr, type);
	}
	else
	{
		auto const loop_info = create_loop_start(size, context);

		auto const result_elem_ptr = context.create_array_gep(type, result_ptr, loop_info.iter_val);
		auto const elem_val = val_ptr::get_reference(context.create_array_gep(type, moved_val.val, loop_info.iter_val), elem_type);
		auto const prev_value = context.push_value_reference(elem_val);
		emit_bitcode(array_move_construct.move_expr, context, result_elem_ptr);
		context.pop_value_reference(prev_value);

		create_loop_end(loop_info, context);

		return val_ptr::get_reference(result_ptr, type);
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_optional_move_construct const &optional_move_construct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const moved_val = emit_bitcode(optional_move_construct.moved_value, context, nullptr);
	auto const type = moved_val.get_type();
	bz_assert(type->isStructTy());

	auto const result = val_ptr::get_reference(
		result_address != nullptr ? result_address : context.create_alloca(type),
		type
	);

	if (moved_val.kind == val_ptr::value)
	{
		context.builder.CreateStore(moved_val.get_value(context.builder), result.val);
		return result;
	}

	auto const has_value = optional_has_value(moved_val, context);

	optional_set_has_value(result, has_value, context);

	auto const decide_bb = context.builder.GetInsertBlock();

	auto const copy_bb = context.add_basic_block("optional_move_construct_has_value");
	context.builder.SetInsertPoint(copy_bb);

	auto const prev_info = context.push_expression_scope();
	auto const result_value_ptr = optional_get_value_ptr(result, context);
	auto const prev_value = context.push_value_reference(optional_get_value_ptr(moved_val, context));
	emit_bitcode(optional_move_construct.value_move_expr, context, result_value_ptr.val);
	context.pop_value_reference(prev_value);
	context.pop_expression_scope(prev_info);

	auto const end_bb = context.add_basic_block("optional_move_construct_end");
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(decide_bb);
	context.builder.CreateCondBr(has_value, copy_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);

	return result;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_trivial_relocate const &trivial_relocate,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const val = emit_bitcode(trivial_relocate.value, context, nullptr);
	auto const type = val.get_type();

	if (val.kind == val_ptr::value)
	{
		return value_or_result_address(val.get_value(context.builder), result_address, context);
	}
	else
	{
		auto const result_ptr = result_address != nullptr ? result_address : context.create_alloca(type);
		emit_value_copy(val, result_ptr, context);
		return val_ptr::get_reference(result_ptr, type);
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_aggregate_destruct const &aggregate_destruct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(result_address == nullptr);

	auto const val = emit_bitcode(aggregate_destruct.value, context, nullptr);
	bz_assert(val.kind == val_ptr::reference);
	auto const type = val.get_type();
	bz_assert(type->isStructTy());
	bz_assert(
		aggregate_destruct.elem_destruct_calls.empty()
		|| type->getStructNumElements() == aggregate_destruct.elem_destruct_calls.size()
	);

	for (auto const i : bz::iota(0, aggregate_destruct.elem_destruct_calls.size()).reversed())
	{
		if (aggregate_destruct.elem_destruct_calls[i].not_null())
		{
			auto const elem_ptr = context.create_struct_gep(type, val.val, i);
			auto const elem_type = type->getStructElementType(i);
			auto const prev_value = context.push_value_reference(val_ptr::get_reference(elem_ptr, elem_type));
			emit_bitcode(aggregate_destruct.elem_destruct_calls[i], context, nullptr);
			context.pop_value_reference(prev_value);
		}
	}

	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_array_destruct const &array_destruct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(result_address == nullptr);

	auto const val = emit_bitcode(array_destruct.value, context, nullptr);
	bz_assert(val.kind == val_ptr::reference);
	auto const type = val.get_type();
	bz_assert(type->isArrayTy());
	auto const elem_type = type->getArrayElementType();
	auto const size = type->getArrayNumElements();

	if (size <= array_loop_threshold)
	{
		for (auto const i : bz::iota(0, size).reversed())
		{
			auto const elem_ptr = context.create_struct_gep(type, val.val, i);
			auto const prev_value = context.push_value_reference(val_ptr::get_reference(elem_ptr, elem_type));
			emit_bitcode(array_destruct.elem_destruct_call, context, nullptr);
			context.pop_value_reference(prev_value);
		}
	}
	else
	{
		auto const loop_info = create_reversed_loop_start(size, context);

		auto const elem_ptr = context.create_array_gep(type, val.val, loop_info.iter_val);
		auto const prev_value = context.push_value_reference(val_ptr::get_reference(elem_ptr, elem_type));
		emit_bitcode(array_destruct.elem_destruct_call, context, nullptr);
		context.pop_value_reference(prev_value);

		create_reversed_loop_end(loop_info, context);
	}

	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_optional_destruct const &optional_destruct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(result_address == nullptr);
	bz_assert(optional_destruct.value_destruct_call.not_null());

	auto const val = emit_bitcode(optional_destruct.value, context, nullptr);
	bz_assert(val.kind == val_ptr::reference);

	auto const has_value = optional_has_value(val, context);

	auto const begin_bb = context.builder.GetInsertBlock();
	auto const destruct_bb = context.add_basic_block("optional_destruct_destruct");
	context.builder.SetInsertPoint(destruct_bb);

	auto const prev_value = context.push_value_reference(optional_get_value_ptr(val, context));
	emit_bitcode(optional_destruct.value_destruct_call, context, nullptr);
	context.pop_value_reference(prev_value);

	auto const end_bb = context.add_basic_block("optional_destruct_end");
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(begin_bb);
	context.builder.CreateCondBr(has_value, destruct_bb, end_bb);

	context.builder.SetInsertPoint(end_bb);

	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_base_type_destruct const &base_type_destruct,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(result_address == nullptr);

	auto const val = emit_bitcode(base_type_destruct.value, context, nullptr);
	bz_assert(val.kind == val_ptr::reference);
	auto const type = val.get_type();
	bz_assert(type->isStructTy());
	bz_assert(
		base_type_destruct.member_destruct_calls.empty()
		|| type->getStructNumElements() == base_type_destruct.member_destruct_calls.size()
	);

	if (base_type_destruct.destruct_call.not_null())
	{
		auto const prev_value = context.push_value_reference(val);
		emit_bitcode(base_type_destruct.destruct_call, context, nullptr);
		context.pop_value_reference(prev_value);
	}

	for (auto const i : bz::iota(0, base_type_destruct.member_destruct_calls.size()).reversed())
	{
		if (base_type_destruct.member_destruct_calls[i].not_null())
		{
			auto const elem_ptr = context.create_struct_gep(type, val.val, i);
			auto const elem_type = type->getStructElementType(i);
			auto const prev_value = context.push_value_reference(val_ptr::get_reference(elem_ptr, elem_type));
			emit_bitcode(base_type_destruct.member_destruct_calls[i], context, nullptr);
			context.pop_value_reference(prev_value);
		}
	}

	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_destruct_value const &destruct_value,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const value = emit_bitcode(destruct_value.value, context, nullptr);
	if (destruct_value.destruct_call.not_null())
	{
		auto const prev_value = context.push_value_reference(value);
		emit_bitcode(destruct_value.destruct_call, context, nullptr);
		context.pop_value_reference(prev_value);
	}

	bz_assert(result_address == nullptr);
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_aggregate_assign const &aggregate_assign,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const rhs = emit_bitcode(aggregate_assign.rhs, context, nullptr);
	auto const lhs = emit_bitcode(aggregate_assign.lhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);
	auto const lhs_type = lhs.get_type();
	auto const rhs_type = rhs.get_type();
	bz_assert(lhs_type->isStructTy());
	bz_assert(rhs_type->isStructTy());

	for (auto const i : bz::iota(0, aggregate_assign.assign_exprs.size()))
	{
		auto const lhs_member_ptr = context.create_struct_gep(lhs_type, lhs.val, i);
		auto const rhs_member_val = rhs.kind == val_ptr::reference
			? val_ptr::get_reference(context.create_struct_gep(rhs_type, rhs.val, i), rhs_type->getStructElementType(i))
			: val_ptr::get_value(context.builder.CreateExtractValue(rhs.get_value(context.builder), i));
		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(val_ptr::get_reference(lhs_member_ptr, lhs_type->getStructElementType(i)));
		auto const rhs_prev_value = context.push_value_reference(rhs_member_val);
		emit_bitcode(aggregate_assign.assign_exprs[i], context, nullptr);
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}

	bz_assert(result_address == nullptr);
	return lhs;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_array_assign const &array_assign,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const rhs = emit_bitcode(array_assign.rhs, context, nullptr);
	auto const lhs = emit_bitcode(array_assign.lhs, context, nullptr);
	bz_assert(rhs.kind == val_ptr::reference);
	bz_assert(lhs.kind == val_ptr::reference);
	auto const lhs_type = lhs.get_type();
	auto const rhs_type = rhs.get_type();
	bz_assert(lhs_type->isArrayTy());
	bz_assert(rhs_type->isArrayTy());
	auto const lhs_elem_type = lhs_type->getArrayElementType();
	auto const rhs_elem_type = rhs_type->getArrayElementType();

	bz_assert(lhs_type->getArrayNumElements() == rhs_type->getArrayNumElements());
	auto const size = lhs_type->getArrayNumElements();

	if (size <= array_loop_threshold)
	{
		for (auto const i : bz::iota(0, size))
		{
			auto const lhs_elem_ptr = context.create_struct_gep(lhs_type, lhs.val, i);
			auto const rhs_elem_ptr = context.create_struct_gep(rhs_type, rhs.val, i);
			auto const lhs_prev_value = context.push_value_reference(val_ptr::get_reference(lhs_elem_ptr, lhs_elem_type));
			auto const rhs_prev_value = context.push_value_reference(val_ptr::get_reference(rhs_elem_ptr, rhs_elem_type));
			emit_bitcode(array_assign.assign_expr, context, nullptr);
			context.pop_value_reference(rhs_prev_value);
			context.pop_value_reference(lhs_prev_value);
		}

		bz_assert(result_address == nullptr);
		return lhs;
	}
	else
	{
		auto const loop_info = create_loop_start(size, context);

		auto const lhs_elem_ptr = context.create_array_gep(lhs_type, lhs.val, loop_info.iter_val);
		auto const rhs_elem_ptr = context.create_array_gep(rhs_type, rhs.val, loop_info.iter_val);
		auto const lhs_prev_value = context.push_value_reference(val_ptr::get_reference(lhs_elem_ptr, lhs_elem_type));
		auto const rhs_prev_value = context.push_value_reference(val_ptr::get_reference(rhs_elem_ptr, rhs_elem_type));
		emit_bitcode(array_assign.assign_expr, context, nullptr);
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);

		create_loop_end(loop_info, context);

		bz_assert(result_address == nullptr);
		return lhs;
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_optional_assign const &optional_assign,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const rhs = emit_bitcode(optional_assign.rhs, context, nullptr);
	auto const lhs = emit_bitcode(optional_assign.lhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);
	bz_assert(rhs.kind == val_ptr::reference);

	bz_assert(lhs.get_type()->isStructTy());
	bz_assert(rhs.get_type()->isStructTy());

	auto const assign_begin_bb = context.builder.GetInsertBlock();

	// decide which branch to go down on
	auto const lhs_has_value = optional_has_value(lhs, context);
	auto const rhs_has_value = optional_has_value(rhs, context);
	auto const any_has_value = context.builder.CreateOr(lhs_has_value, rhs_has_value);

	auto const any_has_value_bb = context.add_basic_block("optional_assign_any_has_value");
	context.builder.SetInsertPoint(any_has_value_bb);

	auto const both_have_value = context.builder.CreateAnd(lhs_has_value, rhs_has_value);

	// both optionals have a value, so we do assignment
	auto const both_have_value_bb = context.add_basic_block("optional_assign_both_have_value");
	context.builder.SetInsertPoint(both_have_value_bb);
	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(optional_get_value_ptr(lhs, context));
		auto const rhs_prev_value = context.push_value_reference(optional_get_value_ptr(rhs, context));
		emit_bitcode(optional_assign.value_assign_expr, context, nullptr);
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}
	auto const both_have_value_bb_end = context.builder.GetInsertBlock();

	auto const one_has_value_bb = context.add_basic_block("optional_assign_one_has_value");
	// context.builder.SetInsertPoint(one_has_value_bb);

	// only lhs has value, so we need to destruct it
	auto const lhs_has_value_bb = context.add_basic_block("optional_assign_lhs_has_value");
	context.builder.SetInsertPoint(lhs_has_value_bb);
	if (optional_assign.value_destruct_expr.not_null())
	{
		auto const prev_value = context.push_value_reference(optional_get_value_ptr(lhs, context));
		emit_bitcode(optional_assign.value_destruct_expr, context, nullptr);
		context.pop_value_reference(prev_value);

		optional_set_has_value(lhs, false, context);
	}
	auto const lhs_has_value_bb_end = context.builder.GetInsertBlock();

	// only rhs has value, so we need to copy construct it into lhs
	auto const rhs_has_value_bb = context.add_basic_block("optional_assign_rhs_has_value");
	context.builder.SetInsertPoint(rhs_has_value_bb);
	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_value_ptr = optional_get_value_ptr(lhs, context);
		auto const prev_value = context.push_value_reference(optional_get_value_ptr(rhs, context));
		emit_bitcode(optional_assign.value_construct_expr, context, lhs_value_ptr.val);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);

		optional_set_has_value(lhs, true, context);
	}
	auto const rhs_has_value_bb_end = context.builder.GetInsertBlock();

	auto const end_bb = context.add_basic_block("optional_assign_end");

	context.builder.SetInsertPoint(assign_begin_bb);
	context.builder.CreateCondBr(any_has_value, any_has_value_bb, end_bb);

	context.builder.SetInsertPoint(any_has_value_bb);
	context.builder.CreateCondBr(both_have_value, both_have_value_bb, one_has_value_bb);

	context.builder.SetInsertPoint(both_have_value_bb_end);
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(one_has_value_bb);
	context.builder.CreateCondBr(lhs_has_value, lhs_has_value_bb, rhs_has_value_bb);

	context.builder.SetInsertPoint(lhs_has_value_bb_end);
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(rhs_has_value_bb_end);
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(end_bb);

	bz_assert(result_address == nullptr);
	return lhs;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_optional_null_assign const &optional_null_assign,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	emit_bitcode(optional_null_assign.rhs, context, nullptr);
	auto const lhs = emit_bitcode(optional_null_assign.lhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);

	auto const has_value = optional_has_value(lhs, context);

	auto const begin_bb = context.builder.GetInsertBlock();

	auto const destruct_bb = context.add_basic_block("optional_null_assign_destruct");
	context.builder.SetInsertPoint(destruct_bb);

	if (optional_null_assign.value_destruct_expr.not_null())
	{
		auto const prev_value = context.push_value_reference(optional_get_value_ptr(lhs, context));
		emit_bitcode(optional_null_assign.value_destruct_expr, context, nullptr);
		context.pop_value_reference(prev_value);
	}
	optional_set_has_value(lhs, false, context);

	auto const end_bb = context.add_basic_block("optional_null_assign_end");
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(begin_bb);
	context.builder.CreateCondBr(has_value, destruct_bb, end_bb);

	context.builder.SetInsertPoint(end_bb);

	bz_assert(result_address == nullptr);
	return lhs;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_optional_value_assign const &optional_value_assign,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const rhs = emit_bitcode(optional_value_assign.rhs, context, nullptr);
	auto const lhs = emit_bitcode(optional_value_assign.lhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);

	auto const has_value = optional_has_value(lhs, context);

	auto const begin_bb = context.builder.GetInsertBlock();

	auto const assign_bb = context.add_basic_block("optional_value_assign_assign");
	context.builder.SetInsertPoint(assign_bb);

	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(optional_get_value_ptr(lhs, context));
		auto const rhs_prev_value = context.push_value_reference(rhs);
		emit_bitcode(optional_value_assign.value_assign_expr, context, nullptr);
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}

	auto const assign_end_bb = context.builder.GetInsertBlock();

	auto const construct_bb = context.add_basic_block("optional_value_assign_construct");
	context.builder.SetInsertPoint(construct_bb);

	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(rhs);
		auto const lhs_value_ptr = optional_get_value_ptr(lhs, context);
		emit_bitcode(optional_value_assign.value_construct_expr, context, lhs_value_ptr.val);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);

		optional_set_has_value(lhs, true, context);
	}

	auto const end_bb = context.add_basic_block("optional_null_assign_end");
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(assign_end_bb);
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(begin_bb);
	context.builder.CreateCondBr(has_value, assign_bb, construct_bb);

	context.builder.SetInsertPoint(end_bb);

	bz_assert(result_address == nullptr);
	return lhs;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_optional_reference_value_assign const &optional_reference_value_assign,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const rhs = emit_bitcode(optional_reference_value_assign.rhs, context, nullptr);
	auto const lhs = emit_bitcode(optional_reference_value_assign.lhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);
	bz_assert(rhs.kind == val_ptr::reference);

	context.builder.CreateStore(rhs.val, lhs.val);

	bz_assert(result_address == nullptr);
	return lhs;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_base_type_assign const &base_type_assign,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const rhs = emit_bitcode(base_type_assign.rhs, context, nullptr);
	auto const lhs = emit_bitcode(base_type_assign.lhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);
	bz_assert(lhs.get_type() == rhs.get_type());

	llvm::BasicBlock *ptr_eq_bb = nullptr;
	if (
		base_type_assign.rhs.get_expr_type().is_reference()
		&& lhs.kind == val_ptr::reference
		&& rhs.kind == val_ptr::reference
	)
	{
		auto const are_equal = context.builder.CreateICmpEQ(lhs.val, rhs.val);
		ptr_eq_bb = context.add_basic_block("assign_ptr_eq");
		auto const neq_bb = context.add_basic_block("assign_ptr_neq");
		context.builder.CreateCondBr(are_equal, ptr_eq_bb, neq_bb);
		context.builder.SetInsertPoint(neq_bb);
	}

	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(lhs);
		emit_bitcode(base_type_assign.lhs_destruct_expr, context, nullptr);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}

	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(rhs);
		emit_bitcode(base_type_assign.rhs_copy_expr, context, lhs.val);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}

	if (ptr_eq_bb != nullptr)
	{
		context.builder.CreateBr(ptr_eq_bb);
		context.builder.SetInsertPoint(ptr_eq_bb);
	}

	bz_assert(result_address == nullptr);
	return lhs;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_trivial_assign const &trivial_assign,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const rhs = emit_bitcode(trivial_assign.rhs, context, nullptr);
	auto const lhs = emit_bitcode(trivial_assign.lhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);
	bz_assert(lhs.get_type() == rhs.get_type());

	llvm::BasicBlock *ptr_eq_bb = nullptr;
	if (lhs.kind == val_ptr::reference && rhs.kind == val_ptr::reference)
	{
		auto const are_equal = context.builder.CreateICmpEQ(lhs.val, rhs.val);
		ptr_eq_bb = context.add_basic_block("assign_ptr_eq");
		auto const neq_bb = context.add_basic_block("assign_ptr_neq");
		context.builder.CreateCondBr(are_equal, ptr_eq_bb, neq_bb);
		context.builder.SetInsertPoint(neq_bb);
	}

	emit_value_copy(rhs, lhs.val, context);

	if (ptr_eq_bb != nullptr)
	{
		context.builder.CreateBr(ptr_eq_bb);
		context.builder.SetInsertPoint(ptr_eq_bb);
	}

	bz_assert(result_address == nullptr);
	return lhs;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_aggregate_swap const &aggregate_swap,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs = emit_bitcode(aggregate_swap.lhs, context, nullptr);
	auto const rhs = emit_bitcode(aggregate_swap.rhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);
	bz_assert(rhs.kind == val_ptr::reference);
	auto const lhs_type = lhs.get_type();
	auto const rhs_type = rhs.get_type();
	bz_assert(lhs_type->isStructTy());
	bz_assert(rhs_type->isStructTy());

	for (auto const i : bz::iota(0, aggregate_swap.swap_exprs.size()))
	{
		auto const lhs_member_ptr = context.create_struct_gep(lhs_type, lhs.val, i);
		auto const rhs_member_ptr = context.create_struct_gep(rhs_type, rhs.val, i);
		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(val_ptr::get_reference(lhs_member_ptr, lhs_type->getStructElementType(i)));
		auto const rhs_prev_value = context.push_value_reference(val_ptr::get_reference(rhs_member_ptr, rhs_type->getStructElementType(i)));
		emit_bitcode(aggregate_swap.swap_exprs[i], context, nullptr);
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}

	bz_assert(result_address == nullptr);
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_array_swap const &array_swap,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs = emit_bitcode(array_swap.lhs, context, nullptr);
	auto const rhs = emit_bitcode(array_swap.rhs, context, nullptr);
	bz_assert(rhs.kind == val_ptr::reference);
	bz_assert(lhs.kind == val_ptr::reference);
	auto const lhs_type = lhs.get_type();
	auto const rhs_type = rhs.get_type();
	bz_assert(lhs_type->isArrayTy());
	bz_assert(rhs_type->isArrayTy());
	auto const lhs_elem_type = lhs_type->getArrayElementType();
	auto const rhs_elem_type = rhs_type->getArrayElementType();

	bz_assert(lhs_type->getArrayNumElements() == rhs_type->getArrayNumElements());
	auto const size = lhs_type->getArrayNumElements();

	if (size <= array_loop_threshold)
	{
		for (auto const i : bz::iota(0, size))
		{
			auto const lhs_elem_ptr = context.create_struct_gep(lhs_type, lhs.val, i);
			auto const rhs_elem_ptr = context.create_struct_gep(rhs_type, rhs.val, i);
			auto const lhs_prev_value = context.push_value_reference(val_ptr::get_reference(lhs_elem_ptr, lhs_elem_type));
			auto const rhs_prev_value = context.push_value_reference(val_ptr::get_reference(rhs_elem_ptr, rhs_elem_type));
			emit_bitcode(array_swap.swap_expr, context, nullptr);
			context.pop_value_reference(rhs_prev_value);
			context.pop_value_reference(lhs_prev_value);
		}
	}
	else
	{
		auto const loop_info = create_loop_start(size, context);

		auto const lhs_elem_ptr = context.create_array_gep(lhs_type, lhs.val, loop_info.iter_val);
		auto const rhs_elem_ptr = context.create_array_gep(rhs_type, rhs.val, loop_info.iter_val);
		auto const lhs_prev_value = context.push_value_reference(val_ptr::get_reference(lhs_elem_ptr, lhs_elem_type));
		auto const rhs_prev_value = context.push_value_reference(val_ptr::get_reference(rhs_elem_ptr, rhs_elem_type));
		emit_bitcode(array_swap.swap_expr, context, nullptr);
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);

		create_loop_end(loop_info, context);
	}
	bz_assert(result_address == nullptr);
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_optional_swap const &optional_swap,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs = emit_bitcode(optional_swap.lhs, context, nullptr);
	auto const rhs = emit_bitcode(optional_swap.rhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);
	bz_assert(rhs.kind == val_ptr::reference);
	bz_assert(lhs.get_type() == rhs.get_type());

	bz_assert(lhs.get_type()->isStructTy());

	auto const begin_bb = context.builder.GetInsertBlock();
	auto const are_pointers_equal = context.builder.CreateICmpEQ(lhs.val, rhs.val);

	auto const swap_begin_bb = context.add_basic_block("optional_swap");
	context.builder.SetInsertPoint(swap_begin_bb);

	// decide which branch to go down on
	auto const lhs_has_value = optional_has_value(lhs, context);
	auto const rhs_has_value = optional_has_value(rhs, context);
	auto const any_has_value = context.builder.CreateOr(lhs_has_value, rhs_has_value);

	auto const any_has_value_bb = context.add_basic_block("optional_swap_any_has_value");
	context.builder.SetInsertPoint(any_has_value_bb);

	auto const both_have_value = context.builder.CreateAnd(lhs_has_value, rhs_has_value);

	// both optionals have a value, so we do a swap
	auto const both_have_value_bb = context.add_basic_block("optional_swap_both_have_value");
	context.builder.SetInsertPoint(both_have_value_bb);
	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(optional_get_value_ptr(lhs, context));
		auto const rhs_prev_value = context.push_value_reference(optional_get_value_ptr(rhs, context));
		emit_bitcode(optional_swap.value_swap_expr, context, nullptr);
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}
	auto const both_have_value_bb_end = context.builder.GetInsertBlock();

	auto const one_has_value_bb = context.add_basic_block("optional_swap_one_has_value");
	// context.builder.SetInsertPoint(one_has_value_bb);

	// only lhs has value, so we need to move it to rhs
	auto const lhs_has_value_bb = context.add_basic_block("optional_swap_lhs_has_value");
	context.builder.SetInsertPoint(lhs_has_value_bb);
	{
		auto const prev_info = context.push_expression_scope();
		auto const rhs_value_ptr = optional_get_value_ptr(rhs, context);
		auto const prev_value = context.push_value_reference(optional_get_value_ptr(lhs, context));
		emit_bitcode(optional_swap.lhs_move_expr, context, rhs_value_ptr.val);
		context.pop_value_reference(prev_value);

		optional_set_has_value(lhs, false, context);
		optional_set_has_value(rhs, true, context);
		context.pop_expression_scope(prev_info);
	}
	auto const lhs_has_value_bb_end = context.builder.GetInsertBlock();

	// only rhs has value, so we need to move it to lhs
	auto const rhs_has_value_bb = context.add_basic_block("optional_swap_rhs_has_value");
	context.builder.SetInsertPoint(rhs_has_value_bb);
	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_value_ptr = optional_get_value_ptr(lhs, context);
		auto const prev_value = context.push_value_reference(optional_get_value_ptr(rhs, context));
		emit_bitcode(optional_swap.rhs_move_expr, context, lhs_value_ptr.val);
		context.pop_value_reference(prev_value);

		optional_set_has_value(lhs, true, context);
		optional_set_has_value(rhs, false, context);
		context.pop_expression_scope(prev_info);
	}
	auto const rhs_has_value_bb_end = context.builder.GetInsertBlock();

	auto const end_bb = context.add_basic_block("optional_swap_end");

	context.builder.SetInsertPoint(begin_bb);
	context.builder.CreateCondBr(are_pointers_equal, end_bb, swap_begin_bb);

	context.builder.SetInsertPoint(swap_begin_bb);
	context.builder.CreateCondBr(any_has_value, any_has_value_bb, end_bb);

	context.builder.SetInsertPoint(any_has_value_bb);
	context.builder.CreateCondBr(both_have_value, both_have_value_bb, one_has_value_bb);

	context.builder.SetInsertPoint(both_have_value_bb_end);
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(one_has_value_bb);
	context.builder.CreateCondBr(lhs_has_value, lhs_has_value_bb, rhs_has_value_bb);

	context.builder.SetInsertPoint(lhs_has_value_bb_end);
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(rhs_has_value_bb_end);
	context.builder.CreateBr(end_bb);

	context.builder.SetInsertPoint(end_bb);

	bz_assert(result_address == nullptr);
	return lhs;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_base_type_swap const &base_type_swap,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs = emit_bitcode(base_type_swap.lhs, context, nullptr);
	auto const rhs = emit_bitcode(base_type_swap.rhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);
	bz_assert(rhs.kind == val_ptr::reference);
	bz_assert(lhs.get_type() == rhs.get_type());
	auto const type = lhs.get_type();

	auto const are_equal = context.builder.CreateICmpEQ(lhs.val, rhs.val);
	auto const ptr_eq_bb = context.add_basic_block("swap_ptr_eq");
	auto const neq_bb = context.add_basic_block("swap_ptr_neq");
	context.builder.CreateCondBr(are_equal, ptr_eq_bb, neq_bb);
	context.builder.SetInsertPoint(neq_bb);

	auto const size = context.get_size(type);
	auto const temp = val_ptr::get_reference(context.create_alloca_without_lifetime_start(type), type);

	context.start_lifetime(temp.val, size);

	// temp = move lhs
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(lhs);
		emit_bitcode(base_type_swap.lhs_move_expr, context, temp.val);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}
	// lhs = move rhs
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(rhs);
		emit_bitcode(base_type_swap.rhs_move_expr, context, lhs.val);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}
	// rhs = move temp
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(temp);
		emit_bitcode(base_type_swap.temp_move_expr, context, rhs.val);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}

	context.end_lifetime(temp.val, size);

	context.builder.CreateBr(ptr_eq_bb);
	context.builder.SetInsertPoint(ptr_eq_bb);

	bz_assert(result_address == nullptr);
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_trivial_swap const &trivial_swap,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const lhs = emit_bitcode(trivial_swap.lhs, context, nullptr);
	auto const rhs = emit_bitcode(trivial_swap.rhs, context, nullptr);
	bz_assert(lhs.kind == val_ptr::reference);
	bz_assert(rhs.kind == val_ptr::reference);
	bz_assert(lhs.get_type() == rhs.get_type());
	auto const type = lhs.get_type();

	auto const are_equal = context.builder.CreateICmpEQ(lhs.val, rhs.val);
	auto const ptr_eq_bb = context.add_basic_block("swap_ptr_eq");
	auto const neq_bb = context.add_basic_block("swap_ptr_neq");
	context.builder.CreateCondBr(are_equal, ptr_eq_bb, neq_bb);
	context.builder.SetInsertPoint(neq_bb);

	if (!type->isAggregateType())
	{
		auto const lhs_val = lhs.get_value(context.builder);
		auto const rhs_val = rhs.get_value(context.builder);
		context.builder.CreateStore(rhs_val, lhs.val);
		context.builder.CreateStore(lhs_val, rhs.val);
	}
	else
	{
		auto const size = context.get_size(type);
		auto const temp = context.create_alloca_without_lifetime_start(type);

		context.start_lifetime(temp, size);
		emit_memcpy(temp, lhs.val, size, context);
		emit_memcpy(lhs.val, rhs.val, size, context);
		emit_memcpy(rhs.val, temp, size, context);
		context.end_lifetime(temp, size);
	}

	context.builder.CreateBr(ptr_eq_bb);
	context.builder.SetInsertPoint(ptr_eq_bb);

	bz_assert(result_address == nullptr);
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_member_access const &member_access,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const base = emit_bitcode(member_access.base, context, nullptr);
	auto const base_type = member_access.base.get_expr_type().remove_mut_reference();
	bz_assert(base_type.is<ast::ts_base_type>());
	auto const accessed_type = base_type.get<ast::ts_base_type>()
		.info->member_variables[member_access.index]->get_type().as_typespec_view();
	if (base.kind == val_ptr::reference || (base.kind == val_ptr::value && accessed_type.is<ast::ts_lvalue_reference>()))
	{
		auto const result_ptr = [&]() -> llvm::Value * {
			if (base.kind == val_ptr::value)
			{
				return context.builder.CreateExtractValue(base.get_value(context.builder), member_access.index);
			}
			else if (accessed_type.is<ast::ts_lvalue_reference>())
			{
				auto const ref_ptr = context.create_struct_gep(base.get_type(), base.val, member_access.index);
				return context.builder.CreateLoad(context.get_opaque_pointer_t(), ref_ptr);
			}
			else
			{
				return context.create_struct_gep(base.get_type(), base.val, member_access.index);
			}
		}();
		auto const result_type = get_llvm_type(accessed_type.remove_reference(), context);
		bz_assert(result_address == nullptr);
		return val_ptr::get_reference(result_ptr, result_type);
	}
	else
	{
		auto const result_val = context.builder.CreateExtractValue(
			base.get_value(context.builder), member_access.index
		);
		return value_or_result_address(result_val, result_address, context);
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_optional_extract_value const &optional_extract_value,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const optional_val = emit_bitcode(optional_extract_value.optional_value, context, nullptr);
	emit_null_optional_get_value_check(src_tokens, optional_val, context);

	if (optional_extract_value.optional_value.get_expr_type().remove_any_mut().is_optional_reference())
	{
		bz_assert(optional_val.get_type()->isPointerTy());
		auto const value_ref = optional_val.get_value(context.builder);

		bz_assert(result_address == nullptr);
		auto const type = get_llvm_type(
			optional_extract_value.optional_value.get_expr_type().remove_any_mut().get_optional_reference(),
			context
		);
		return val_ptr::get_reference(value_ref, type);
	}
	else
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_val = context.push_value_reference(optional_get_value_ptr(optional_val, context));
		auto const result_val = emit_bitcode(optional_extract_value.value_move_expr, context, result_address);
		context.pop_value_reference(prev_val);
		context.pop_expression_scope(prev_info);

		return result_val;
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_rvalue_member_access const &rvalue_member_access,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const base = emit_bitcode(rvalue_member_access.base, context, nullptr);
	bz_assert(base.kind == val_ptr::reference);
	auto const base_type = rvalue_member_access.base.get_expr_type().remove_mut_reference();
	bz_assert(base_type.is<ast::ts_base_type>());
	auto const accessed_type = base_type.get<ast::ts_base_type>()
		.info->member_variables[rvalue_member_access.index]->get_type().as_typespec_view();

	auto const prev_info = context.push_expression_scope();
	val_ptr result = val_ptr::get_none();
	for (auto const i : bz::iota(0, rvalue_member_access.member_refs.size()))
	{
		if (rvalue_member_access.member_refs[i].is_null())
		{
			continue;
		}

		auto const member_val = [&]() {
			if (i == rvalue_member_access.index && accessed_type.is<ast::ts_lvalue_reference>())
			{
				auto const ref_member_ptr = context.create_struct_gep(base.get_type(), base.val, i);
				auto const member_ptr = context.builder.CreateLoad(context.get_opaque_pointer_t(), ref_member_ptr);
				auto const member_type = get_llvm_type(accessed_type.get<ast::ts_lvalue_reference>(), context);
				return val_ptr::get_reference(member_ptr, member_type);
			}
			else
			{
				auto const member_ptr = context.create_struct_gep(base.get_type(), base.val, i);
				auto const member_type = base.get_type()->getStructElementType(i);
				return val_ptr::get_reference(member_ptr, member_type);
			}
		}();

		auto const prev_value = context.push_value_reference(member_val);
		if (i == rvalue_member_access.index)
		{
			auto const prev_info = context.push_expression_scope();
			result = emit_bitcode(rvalue_member_access.member_refs[i], context, result_address);
			context.pop_expression_scope(prev_info);
		}
		else
		{
			emit_bitcode(rvalue_member_access.member_refs[i], context, nullptr);
		}
		context.pop_value_reference(prev_value);
	}
	context.pop_expression_scope(prev_info);

	return result;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_type_member_access const &member_access,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(member_access.var_decl != nullptr);
	auto const decl = member_access.var_decl;
	auto const [ptr, type] = context.get_variable(decl);

	bz_assert(result_address == nullptr);
	return val_ptr::get_reference(ptr, type);
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_compound const &compound_expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const prev_info = context.push_expression_scope();
	for (auto &stmt : compound_expr.statements)
	{
		emit_bitcode(stmt, context);
	}
	if (compound_expr.final_expr.is_null())
	{
		context.pop_expression_scope(prev_info);
		return val_ptr::get_none();
	}
	else
	{
		auto const result = emit_bitcode(compound_expr.final_expr, context, result_address);
		context.pop_expression_scope(prev_info);
		return result;
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_if const &if_expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const prev_info = context.push_expression_scope();
	auto condition = emit_bitcode(if_expr.condition, context, nullptr).get_value(context.builder);
	context.pop_expression_scope(prev_info);
	// assert that the condition is an i1 (bool)
	bz_assert(condition->getType()->isIntegerTy() && condition->getType()->getIntegerBitWidth() == 1);
	// the original block
	auto const entry_bb = context.builder.GetInsertBlock();

	if (auto const constant_condition = llvm::dyn_cast<llvm::ConstantInt>(condition))
	{
		if (constant_condition->equalsInt(1))
		{
			return emit_bitcode(if_expr.then_block, context, result_address);
		}
		else if (if_expr.else_block.not_null())
		{
			return emit_bitcode(if_expr.else_block, context, result_address);
		}
		else
		{
			return val_ptr::get_none();
		}
	}

	// emit code for the then block
	auto const then_bb = context.add_basic_block("then");
	context.builder.SetInsertPoint(then_bb);
	auto const then_prev_info = context.push_expression_scope();
	auto const then_val = emit_bitcode(if_expr.then_block, context, result_address);
	context.pop_expression_scope(then_prev_info);
	auto const then_bb_end = context.builder.GetInsertBlock();

	// emit code for the else block if there's any
	auto const else_bb = if_expr.else_block.is_null() ? nullptr : context.add_basic_block("else");
	val_ptr else_val = val_ptr::get_none();
	if (else_bb != nullptr)
	{
		context.builder.SetInsertPoint(else_bb);
		auto const else_prev_info = context.push_expression_scope();
		else_val = emit_bitcode(if_expr.else_block, context, result_address);
		context.pop_expression_scope(else_prev_info);
	}
	auto const else_bb_end = else_bb ? context.builder.GetInsertBlock() : nullptr;

	// if both branches have a return at the end, then don't create the end block
	if (else_bb_end != nullptr && context.has_terminator(then_bb_end) && context.has_terminator(else_bb_end))
	{
		context.builder.SetInsertPoint(entry_bb);
		// else_bb must be valid here
		context.builder.CreateCondBr(condition, then_bb, else_bb);
		return val_ptr::get_none();
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
	context.builder.CreateCondBr(condition, then_bb, else_bb != nullptr ? else_bb : end_bb);

	// create branches for the then and else blocks, if there's no return at the end
	if (!context.has_terminator(then_bb_end))
	{
		context.builder.SetInsertPoint(then_bb_end);
		context.builder.CreateBr(end_bb);
	}
	if (else_bb_end != nullptr && !context.has_terminator(else_bb_end))
	{
		context.builder.SetInsertPoint(else_bb_end);
		context.builder.CreateBr(end_bb);
	}

	context.builder.SetInsertPoint(end_bb);
	if ((!then_val.has_value() && !if_expr.then_block.is_noreturn()) || (!else_val.has_value() && !if_expr.else_block.is_noreturn()))
	{
		return val_ptr::get_none();
	}

	auto const result_type = then_val.get_type();
	if (if_expr.then_block.is_noreturn())
	{
		return else_val;
	}
	else if (if_expr.else_block.is_noreturn())
	{
		return then_val;
	}
	else if (result_address != nullptr)
	{
		bz_assert(then_val.val == result_address && else_val.val == result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
	else if (then_val.kind == val_ptr::reference && else_val.kind == val_ptr::reference)
	{
		auto const result = context.builder.CreatePHI(then_val.val->getType(), 2);
		bz_assert(then_val.val != nullptr);
		bz_assert(else_val.val != nullptr);
		result->addIncoming(then_val.val, then_bb_end);
		result->addIncoming(else_val.val, else_bb_end);
		return val_ptr::get_reference(result, result_type);
	}
	else
	{
		bz_assert(then_val_value != nullptr && else_val_value != nullptr);
		auto const result = context.builder.CreatePHI(then_val_value->getType(), 2);
		result->addIncoming(then_val_value, then_bb_end);
		result->addIncoming(else_val_value, else_bb_end);
		return val_ptr::get_value(result);
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_if_consteval const &if_expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(if_expr.condition.is_constant());
	auto const &condition_value = if_expr.condition.get_constant_value();
	bz_assert(condition_value.is_boolean());
	if (condition_value.get_boolean())
	{
		return emit_bitcode(if_expr.then_block, context, result_address);
	}
	else if (if_expr.else_block.not_null())
	{
		return emit_bitcode(if_expr.else_block, context, result_address);
	}
	else
	{
		bz_assert(result_address == nullptr);
		return val_ptr::get_none();
	}
}

static val_ptr emit_integral_switch(
	lex::src_tokens const &src_tokens,
	llvm::Value *matched_value,
	ast::expr_switch const &switch_expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(matched_value->getType()->isIntegerTy());
	auto const default_bb = context.add_basic_block("switch_else");
	auto const has_default = switch_expr.default_case.not_null();
	bz_assert(result_address == nullptr  || switch_expr.is_complete);

	auto const case_count = switch_expr.cases.transform([](auto const &switch_case) { return switch_case.values.size(); }).sum();

	auto const switch_inst = context.builder.CreateSwitch(matched_value, default_bb, static_cast<unsigned>(case_count));
	ast::arena_vector<std::pair<llvm::BasicBlock *, val_ptr>> case_result_vals;
	case_result_vals.reserve(switch_expr.cases.size() + 1);
	if (has_default)
	{
		context.builder.SetInsertPoint(default_bb);
		auto const prev_info = context.push_expression_scope();
		auto const default_val = emit_bitcode(switch_expr.default_case, context, result_address);
		context.pop_expression_scope(prev_info);
		if (!context.has_terminator())
		{
			case_result_vals.push_back({ context.builder.GetInsertBlock(), default_val });
		}
	}
	else if (switch_expr.is_complete)
	{
		context.builder.SetInsertPoint(default_bb);
		if (global_data::panic_on_invalid_switch)
		{
			emit_panic_call(src_tokens, "invalid value used in 'switch'", context);
		}
		else
		{
			context.builder.CreateUnreachable();
		}
		bz_assert(context.has_terminator());
	}
	for (auto const &[case_vals, case_expr] : switch_expr.cases)
	{
		auto const bb = context.add_basic_block("case");
		for (auto const &expr : case_vals)
		{
			bz_assert(expr.is_constant());
			auto const &const_expr = expr.get_constant();
			auto const val = get_value(const_expr.value, const_expr.type, &const_expr, context);
			auto const const_int_val = llvm::cast<llvm::ConstantInt>(val);
			switch_inst->addCase(const_int_val, bb);
		}
		context.builder.SetInsertPoint(bb);
		auto const prev_info = context.push_expression_scope();
		auto const case_val = emit_bitcode(case_expr, context, result_address);
		context.pop_expression_scope(prev_info);
		if (!context.has_terminator())
		{
			case_result_vals.push_back({ context.builder.GetInsertBlock(), case_val });
		}
	}
	auto const end_bb = switch_expr.is_complete ? context.add_basic_block("switch_end") : default_bb;
	auto const has_value = case_result_vals.not_empty() && case_result_vals.is_all([&](auto const &pair) {
		return pair.second.val != nullptr || pair.second.consteval_val != nullptr;
	});
	if (result_address == nullptr && switch_expr.is_complete && has_value)
	{
		auto const is_all_ref = case_result_vals.is_all([&](auto const &pair) {
			return pair.second.val != nullptr && pair.second.kind == val_ptr::reference;
		});
		context.builder.SetInsertPoint(end_bb);
		bz_assert(case_result_vals.not_empty());
		auto const result_type = case_result_vals.front().second.get_type();
		bz_assert(case_result_vals.not_empty());
		bz_assert(!is_all_ref || case_result_vals.front().second.val != nullptr);
		auto const phi_type = is_all_ref
			? case_result_vals.front().second.val->getType()
			: case_result_vals.front().second.get_type();
		auto const phi = context.builder.CreatePHI(phi_type, case_result_vals.size());
		if (is_all_ref)
		{
			for (auto const &[bb, val] : case_result_vals)
			{
				bz_assert(!context.has_terminator(bb));
				context.builder.SetInsertPoint(bb);
				context.builder.CreateBr(end_bb);
				phi->addIncoming(val.val, bb);
			}
		}
		else
		{
			for (auto const &[bb, val] : case_result_vals)
			{
				bz_assert(!context.has_terminator(bb));
				context.builder.SetInsertPoint(bb);
				phi->addIncoming(val.get_value(context.builder), bb);
				context.builder.CreateBr(end_bb);
				bz_assert(context.builder.GetInsertBlock() == bb);
			}
		}
		context.builder.SetInsertPoint(end_bb);
		return is_all_ref
			? val_ptr::get_reference(phi, result_type)
			: val_ptr::get_value(phi);
	}
	else if (switch_expr.is_complete && has_value)
	{
		for (auto const &[bb, _] : case_result_vals)
		{
			bz_assert(!context.has_terminator(bb));
			context.builder.SetInsertPoint(bb);
			context.builder.CreateBr(end_bb);
		}
		context.builder.SetInsertPoint(end_bb);

		bz_assert(result_address != nullptr);
		bz_assert(case_result_vals.not_empty());
		auto const result_type = case_result_vals.front().second.get_type();
		return val_ptr::get_reference(result_address, result_type);
	}
	else
	{
		for (auto const &[bb, _] : case_result_vals)
		{
			bz_assert(!context.has_terminator(bb));
			context.builder.SetInsertPoint(bb);
			context.builder.CreateBr(end_bb);
		}
		context.builder.SetInsertPoint(end_bb);
		bz_assert(result_address == nullptr);
		return val_ptr::get_none();
	}
}

struct string_switch_case_info_t
{
	size_t str_size;
	llvm::BasicBlock *bb;

	struct value_bb_pair_t
	{
		bz::u8string_view value;
		llvm::BasicBlock *bb;
	};

	ast::arena_vector<value_bb_pair_t> values;
};

static llvm::ConstantInt *get_string_int_val(bz::u8string_view str, llvm::Type *int_type, bitcode_context &context)
{
	bz_assert(str.size() <= 8);
	uint64_t result = 0;
	if (context.get_data_layout().isLittleEndian())
	{
		for (auto const i : bz::iota(0, str.size()))
		{
			auto const c = static_cast<uint64_t>(static_cast<uint8_t>(*(str.data() + i)));
			result |= c << (i * 8);
		}
	}
	else
	{
		for (auto const i : bz::iota(0, str.size()))
		{
			auto const c = static_cast<uint64_t>(static_cast<uint8_t>(*(str.data() + i)));
			result |= c << ((7 - i) * 8);
		}
	}

	return llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(int_type, result));
}

static llvm::Value *are_strings_equal(llvm::Value *begin_ptr, bz::u8string_view str, llvm::BasicBlock *else_bb, bitcode_context &context)
{
	auto const global_str = context.create_string(str);
	auto const size = str.size();

	auto const int_type = context.get_uint64_t();
	auto const char_type = context.get_uint8_t();
	auto const lhs_int_ref = context.create_alloca_without_lifetime_start(int_type);
	auto const rhs_int_ref = context.create_alloca_without_lifetime_start(int_type);
	auto const zero_val = llvm::ConstantInt::get(int_type, 0);

	auto lhs_it = begin_ptr;
	auto rhs_it = global_str;

	for ([[maybe_unused]] auto const _ : bz::iota(0, size / 8))
	{
		context.builder.CreateStore(zero_val, lhs_int_ref);
		context.builder.CreateStore(zero_val, rhs_int_ref);
		context.builder.CreateMemCpy(lhs_int_ref, std::nullopt, lhs_it, std::nullopt, 8);
		context.builder.CreateMemCpy(rhs_int_ref, std::nullopt, rhs_it, std::nullopt, 8);
		auto const lhs_val = context.builder.CreateLoad(int_type, lhs_int_ref);
		auto const rhs_val = context.builder.CreateLoad(int_type, rhs_int_ref);
		auto const are_equal = context.builder.CreateICmpEQ(lhs_val, rhs_val);
		auto const equal_bb = context.add_basic_block("string_switch_long_string");
		context.builder.CreateCondBr(are_equal, equal_bb, else_bb);
		context.builder.SetInsertPoint(equal_bb);
		lhs_it = context.builder.CreateConstGEP1_64(char_type, lhs_it, 8);
		rhs_it = context.builder.CreateConstGEP1_64(char_type, rhs_it, 8);
	}

	auto const remaining_size = size % 8;
	context.builder.CreateStore(zero_val, lhs_int_ref);
	context.builder.CreateStore(zero_val, rhs_int_ref);
	context.builder.CreateMemCpy(lhs_int_ref, std::nullopt, lhs_it, std::nullopt, remaining_size);
	context.builder.CreateMemCpy(rhs_int_ref, std::nullopt, rhs_it, std::nullopt, remaining_size);
	auto const lhs_val = context.builder.CreateLoad(int_type, lhs_int_ref);
	auto const rhs_val = context.builder.CreateLoad(int_type, rhs_int_ref);
	auto const are_equal = context.builder.CreateICmpEQ(lhs_val, rhs_val);
	return are_equal;
}

static val_ptr emit_string_switch(
	val_ptr matched_value,
	ast::expr_switch const &switch_expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	// switching on strings is done in two main steps:
	//   1. do a switch on the size of the string to narrow it down
	//   2. for each unique size in the cases determine the matching case

	auto const begin_ptr = context.get_struct_element(matched_value, 0).get_value(context.builder);
	auto const end_ptr   = context.get_struct_element(matched_value, 1).get_value(context.builder);
	auto const size = context.builder.CreatePtrDiff(context.get_uint8_t(), end_ptr, begin_ptr);

	auto size_cases = ast::arena_vector<string_switch_case_info_t>();

	// start building up the size info of the cases
	for (auto const &[case_vals, _] : switch_expr.cases)
	{
		for (auto const &val : case_vals)
		{
			bz_assert(val.is_constant() && val.get_constant_value().is_string());
			auto const val_str = val.get_constant_value().get_string();
			auto const val_str_size = val_str.size();
			auto const it = std::find_if(
				size_cases.begin(), size_cases.end(),
				[val_str_size](auto const &case_) { return case_.str_size == val_str_size; }
			);
			if (it == size_cases.end())
			{
				size_cases.push_back({ val_str_size, context.add_basic_block("string_switch_size_case"), {} });
			}
		}
	}

	auto const default_bb = context.add_basic_block("string_switch_else");
	auto const has_default = switch_expr.default_case.not_null();
	bz_assert(result_address == nullptr || has_default);

	// switch on the string size
	auto const switch_inst = context.builder.CreateSwitch(size, default_bb, size_cases.size());
	for (auto const &[case_size, bb, _] : size_cases)
	{
		auto const size_val = llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(size->getType(), case_size));
		switch_inst->addCase(size_val, bb);
	}

	// emit the case expressions and finish building up the size switch info
	ast::arena_vector<std::pair<llvm::BasicBlock *, val_ptr>> case_result_vals;
	case_result_vals.reserve(switch_expr.cases.size() + 1);
	if (has_default)
	{
		context.builder.SetInsertPoint(default_bb);
		auto const prev_info = context.push_expression_scope();
		auto const default_val = emit_bitcode(switch_expr.default_case, context, result_address);
		context.pop_expression_scope(prev_info);
		if (!context.has_terminator())
		{
			case_result_vals.push_back({ context.builder.GetInsertBlock(), default_val });
		}
	}
	for (auto const &[case_vals, case_expr] : switch_expr.cases)
	{
		auto const expr_bb = context.add_basic_block("string_switch_case");

		for (auto const &val : case_vals)
		{
			auto const val_str = val.get_constant_value().get_string();
			auto const val_str_size = val_str.size();
			auto const it = std::find_if(
				size_cases.begin(), size_cases.end(),
				[val_str_size](auto const &case_) { return case_.str_size == val_str_size; }
			);
			bz_assert(it != size_cases.end());
			it->values.push_back({ val_str, expr_bb });
		}

		context.builder.SetInsertPoint(expr_bb);
		auto const prev_info = context.push_expression_scope();
		auto const case_val = emit_bitcode(case_expr, context, result_address);
		context.pop_expression_scope(prev_info);
		if (!context.has_terminator())
		{
			case_result_vals.push_back({ context.builder.GetInsertBlock(), case_val });
		}
	}

	// for each size case determine which case we have
	for (auto const &[str_size, bb, values] : size_cases)
	{
		context.builder.SetInsertPoint(bb);
		// if the string is less than 8 bytes we copy them into an integer and do a switch on that,
		// otherwise we do an if-else chain
		if (str_size <= 8)
		{
			auto const int_type = context.get_uint64_t();
			auto const str_int_ref = context.create_alloca_without_lifetime_start(int_type);
			context.builder.CreateStore(llvm::ConstantInt::get(int_type, 0), str_int_ref);
			context.builder.CreateMemCpy(str_int_ref, std::nullopt, begin_ptr, std::nullopt, str_size);

			auto const str_int_val = context.builder.CreateLoad(int_type, str_int_ref);
			auto const str_int_switch = context.builder.CreateSwitch(str_int_val, default_bb, values.size());
			for (auto const &[value, expr_bb] : values)
			{
				auto const value_int_val = get_string_int_val(value, int_type, context);
				str_int_switch->addCase(llvm::cast<llvm::ConstantInt>(value_int_val), expr_bb);
			}
		}
		else
		{
			auto current_bb = bb;
			for (auto const &[value, expr_bb] : values)
			{
				auto const else_bb = context.add_basic_block("string_switch_long_string");
				auto const are_equal = are_strings_equal(begin_ptr, value, else_bb, context);
				context.builder.CreateCondBr(are_equal, expr_bb, else_bb);
				current_bb = else_bb;
				context.builder.SetInsertPoint(current_bb);
			}
			context.builder.CreateBr(default_bb);
		}
	}

	auto const end_bb = has_default ? context.add_basic_block("string_switch_end") : default_bb;
	auto const has_value = case_result_vals.not_empty() && case_result_vals.is_all([&](auto const &pair) {
		return pair.second.val != nullptr || pair.second.consteval_val != nullptr;
	});
	if (result_address == nullptr && has_default && has_value)
	{
		auto const is_all_ref = case_result_vals.is_all([&](auto const &pair) {
			return pair.second.val != nullptr && pair.second.kind == val_ptr::reference;
		});
		context.builder.SetInsertPoint(end_bb);
		bz_assert(case_result_vals.not_empty());
		auto const result_type = case_result_vals.front().second.get_type();
		bz_assert(case_result_vals.not_empty());
		bz_assert(!is_all_ref || case_result_vals.front().second.val != nullptr);
		auto const phi_type = is_all_ref
			? case_result_vals.front().second.val->getType()
			: case_result_vals.front().second.get_type();
		auto const phi = context.builder.CreatePHI(phi_type, case_result_vals.size());
		if (is_all_ref)
		{
			for (auto const &[bb, val] : case_result_vals)
			{
				bz_assert(!context.has_terminator(bb));
				context.builder.SetInsertPoint(bb);
				context.builder.CreateBr(end_bb);
				phi->addIncoming(val.val, bb);
			}
		}
		else
		{
			for (auto const &[bb, val] : case_result_vals)
			{
				bz_assert(!context.has_terminator(bb));
				context.builder.SetInsertPoint(bb);
				phi->addIncoming(val.get_value(context.builder), bb);
				context.builder.CreateBr(end_bb);
				bz_assert(context.builder.GetInsertBlock() == bb);
			}
		}
		context.builder.SetInsertPoint(end_bb);
		return is_all_ref
			? val_ptr::get_reference(phi, result_type)
			: val_ptr::get_value(phi);
	}
	else if (has_default && has_value)
	{
		for (auto const &[bb, _] : case_result_vals)
		{
			bz_assert(!context.has_terminator(bb));
			context.builder.SetInsertPoint(bb);
			context.builder.CreateBr(end_bb);
		}
		context.builder.SetInsertPoint(end_bb);

		bz_assert(result_address != nullptr);
		bz_assert(case_result_vals.not_empty());
		auto const result_type = case_result_vals.front().second.get_type();
		return val_ptr::get_reference(result_address, result_type);
	}
	else
	{
		for (auto const &[bb, _] : case_result_vals)
		{
			bz_assert(!context.has_terminator(bb));
			context.builder.SetInsertPoint(bb);
			context.builder.CreateBr(end_bb);
		}
		context.builder.SetInsertPoint(end_bb);
		bz_assert(result_address == nullptr);
		return val_ptr::get_none();
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_switch const &switch_expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const matched_value_prev_info = context.push_expression_scope();
	auto const matched_value = emit_bitcode(switch_expr.matched_expr, context, nullptr);
	context.pop_expression_scope(matched_value_prev_info);
	if (matched_value.get_type()->isIntegerTy())
	{
		return emit_integral_switch(src_tokens, matched_value.get_value(context.builder), switch_expr, context, result_address);
	}
	else
	{
		bz_assert(matched_value.get_type() == context.get_str_t());
		return emit_string_switch(matched_value, switch_expr, context, result_address);
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_break const &,
	bitcode_context &context,
	llvm::Value *
)
{
	bz_assert(context.loop_info.break_bb != nullptr);
	context.emit_loop_destruct_operations();
	context.emit_loop_end_lifetime_calls();
	bz_assert(!context.has_terminator());
	context.builder.CreateBr(context.loop_info.break_bb);
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_continue const &,
	bitcode_context &context,
	llvm::Value *
)
{
	bz_assert(context.loop_info.continue_bb != nullptr);
	context.emit_loop_destruct_operations();
	context.emit_loop_end_lifetime_calls();
	bz_assert(!context.has_terminator());
	context.builder.CreateBr(context.loop_info.continue_bb);
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_unreachable const &unreachable_expr,
	bitcode_context &context,
	llvm::Value *
)
{
	if (global_data::panic_on_unreachable)
	{
		emit_bitcode(unreachable_expr.panic_fn_call, context, nullptr);
		auto const return_type = context.current_function.second->getReturnType();
		if (return_type->isVoidTy())
		{
			context.builder.CreateRetVoid();
		}
		else
		{
			context.builder.CreateRet(llvm::UndefValue::get(return_type));
		}
	}
	else
	{
		context.builder.CreateUnreachable();
	}
	return val_ptr::get_none();
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_generic_type_instantiation const &,
	bitcode_context &,
	llvm::Value *
)
{
	bz_unreachable;
}

static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_bitcode_value_reference const &bitcode_value_reference,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(result_address == nullptr);
	return context.get_value_reference(bitcode_value_reference.index);
}

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
		return value.get_uint_array().is_all([](auto const value) { return value == 0; });
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

static llvm::Constant *get_array_value(
	bz::array_view<ast::constant_value const> values,
	ast::ts_array const &array_type,
	llvm::ArrayType *type,
	bitcode_context &context
)
{
	if (values.is_all([](auto const &value) { return is_zero_value(value); }))
	{
		return llvm::ConstantAggregateZero::get(type);
	}
	else if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(type->getElementType()->isArrayTy());
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		auto const elems = bz::iota(0, array_type.size)
			.transform([&](auto const i) {
				auto const begin_index = i * stride;
				return values.slice(begin_index, begin_index + stride);
			})
			.transform([&](auto const inner_values) {
				return get_array_value(inner_values, array_type.elem_type.get<ast::ts_array>(), llvm::cast<llvm::ArrayType>(type->getElementType()), context);
			})
			.template collect<ast::arena_vector>();
		return llvm::ConstantArray::get(type, llvm::ArrayRef(elems.data(), elems.size()));
	}
	else
	{
		auto const elems = values
			.transform([&](auto const &value) {
				return get_value(value, array_type.elem_type, nullptr, context);
			})
			.template collect<ast::arena_vector>();
		return llvm::ConstantArray::get(type, llvm::ArrayRef(elems.data(), elems.size()));
	}
}

static llvm::Constant *get_sint_array_value(
	bz::array_view<int64_t const> values,
	ast::ts_array const &array_type,
	llvm::ArrayType *type,
	bitcode_context &context
)
{
	if (values.is_all([](auto const value) { return value == 0; }))
	{
		return llvm::ConstantAggregateZero::get(type);
	}
	else if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(type->getElementType()->isArrayTy());
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		auto const elems = bz::iota(0, array_type.size)
			.transform([&](auto const i) {
				auto const begin_index = i * stride;
				return values.slice(begin_index, begin_index + stride);
			})
			.transform([&](auto const inner_values) {
				return get_sint_array_value(inner_values, array_type.elem_type.get<ast::ts_array>(), llvm::cast<llvm::ArrayType>(type->getElementType()), context);
			})
			.template collect<ast::arena_vector>();
		return llvm::ConstantArray::get(type, llvm::ArrayRef(elems.data(), elems.size()));
	}
	else
	{
		auto const elem_type = type->getElementType();
		auto const elems = values
			.transform([&](auto const value) -> llvm::Constant * {
				return llvm::ConstantInt::get(elem_type, value);
			})
			.template collect<ast::arena_vector>();
		return llvm::ConstantArray::get(type, llvm::ArrayRef(elems.data(), elems.size()));
	}
}

static llvm::Constant *get_uint_array_value(
	bz::array_view<uint64_t const> values,
	ast::ts_array const &array_type,
	llvm::ArrayType *type,
	bitcode_context &context
)
{
	if (values.is_all([](auto const value) { return value == 0; }))
	{
		return llvm::ConstantAggregateZero::get(type);
	}
	else if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(type->getElementType()->isArrayTy());
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		auto const elems = bz::iota(0, array_type.size)
			.transform([&](auto const i) {
				auto const begin_index = i * stride;
				return values.slice(begin_index, begin_index + stride);
			})
			.transform([&](auto const inner_values) {
				return get_uint_array_value(inner_values, array_type.elem_type.get<ast::ts_array>(), llvm::cast<llvm::ArrayType>(type->getElementType()), context);
			})
			.template collect<ast::arena_vector>();
		return llvm::ConstantArray::get(type, llvm::ArrayRef(elems.data(), elems.size()));
	}
	else
	{
		auto const elem_type = type->getElementType();
		auto const elems = values
			.transform([&](auto const value) -> llvm::Constant * {
				return llvm::ConstantInt::get(elem_type, value);
			})
			.template collect<ast::arena_vector>();
		return llvm::ConstantArray::get(type, llvm::ArrayRef(elems.data(), elems.size()));
	}
}

static llvm::Constant *get_float32_array_value(
	bz::array_view<float32_t const> values,
	ast::ts_array const &array_type,
	llvm::ArrayType *type,
	bitcode_context &context
)
{
	if (values.is_all([](auto const value) { return bit_cast<uint32_t>(value) == 0; }))
	{
		return llvm::ConstantAggregateZero::get(type);
	}
	else if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(type->getElementType()->isArrayTy());
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		auto const elems = bz::iota(0, array_type.size)
			.transform([&](auto const i) {
				auto const begin_index = i * stride;
				return values.slice(begin_index, begin_index + stride);
			})
			.transform([&](auto const inner_values) {
				return get_float32_array_value(inner_values, array_type.elem_type.get<ast::ts_array>(), llvm::cast<llvm::ArrayType>(type->getElementType()), context);
			})
			.template collect<ast::arena_vector>();
		return llvm::ConstantArray::get(type, llvm::ArrayRef(elems.data(), elems.size()));
	}
	else
	{
		auto const elem_type = type->getElementType();
		bz_assert(elem_type == context.get_float32_t());
		auto const elems = values
			.transform([&](auto const value) -> llvm::Constant * {
				return llvm::ConstantFP::get(elem_type, static_cast<float64_t>(value));
			})
			.template collect<ast::arena_vector>();
		return llvm::ConstantArray::get(type, llvm::ArrayRef(elems.data(), elems.size()));
	}
}

static llvm::Constant *get_float64_array_value(
	bz::array_view<float64_t const> values,
	ast::ts_array const &array_type,
	llvm::ArrayType *type,
	bitcode_context &context
)
{
	if (values.is_all([](auto const value) { return bit_cast<uint64_t>(value) == 0; }))
	{
		return llvm::ConstantAggregateZero::get(type);
	}
	else if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(type->getElementType()->isArrayTy());
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		auto const elems = bz::iota(0, array_type.size)
			.transform([&](auto const i) {
				auto const begin_index = i * stride;
				return values.slice(begin_index, begin_index + stride);
			})
			.transform([&](auto const inner_values) {
				return get_float64_array_value(inner_values, array_type.elem_type.get<ast::ts_array>(), llvm::cast<llvm::ArrayType>(type->getElementType()), context);
			})
			.template collect<ast::arena_vector>();
		return llvm::ConstantArray::get(type, llvm::ArrayRef(elems.data(), elems.size()));
	}
	else
	{
		auto const elem_type = type->getElementType();
		bz_assert(elem_type == context.get_float64_t());
		auto const elems = values
			.transform([&](auto const value) -> llvm::Constant * {
				return llvm::ConstantFP::get(elem_type, value);
			})
			.template collect<ast::arena_vector>();
		return llvm::ConstantArray::get(type, llvm::ArrayRef(elems.data(), elems.size()));
	}
}

static llvm::Constant *get_value_helper(
	ast::constant_value const &value,
	ast::typespec_view type,
	ast::constant_expression const *const_expr,
	bitcode_context &context
)
{
	switch (value.kind())
	{
	static_assert(ast::constant_value::variant_count == 19);
	case ast::constant_value_kind::sint:
		bz_assert(!type.is_empty());
		return llvm::ConstantInt::get(
			get_llvm_type(type, context),
			bit_cast<uint64_t>(value.get_sint()),
			true
		);
	case ast::constant_value_kind::uint:
		bz_assert(!type.is_empty());
		return llvm::ConstantInt::get(
			get_llvm_type(type, context),
			value.get_uint(),
			false
		);
	case ast::constant_value_kind::float32:
		return llvm::ConstantFP::get(
			context.get_float32_t(),
			static_cast<double>(value.get_float32())
		);
	case ast::constant_value_kind::float64:
		return llvm::ConstantFP::get(
			context.get_float64_t(),
			value.get_float64()
		);
	case ast::constant_value_kind::u8char:
		return llvm::ConstantInt::get(
			context.get_char_t(),
			value.get_u8char()
		);
	case ast::constant_value_kind::string:
	{
		auto const str = value.get_string();
		auto const str_t = llvm::cast<llvm::StructType>(context.get_str_t());

		// if the string is empty, we make a zero initialized string, so
		// structs with a default value of "" get to be zero initialized
		if (str == "")
		{
			return llvm::ConstantStruct::getNullValue(str_t);
		}

		auto const string_constant = context.create_string(str);
		auto const string_type = llvm::ArrayType::get(context.get_uint8_t(), str.size() + 1);

		auto const begin_ptr = context.create_struct_gep(string_type, string_constant, 0);
		auto const const_begin_ptr = llvm::cast<llvm::Constant>(begin_ptr);

		auto const end_ptr = context.create_struct_gep(string_type, string_constant, str.size());
		auto const const_end_ptr = llvm::cast<llvm::Constant>(end_ptr);

		return llvm::ConstantStruct::get(str_t, const_begin_ptr, const_end_ptr);
	}
	case ast::constant_value_kind::boolean:
		return context.builder.getInt1(value.get_boolean());
	case ast::constant_value_kind::null:
		if (
			auto const type_without_const = type.remove_any_mut();
			type_without_const.is_optional_pointer_like()
		)
		{
			return llvm::ConstantPointerNull::get(context.get_opaque_pointer_t());
		}
		else
		{
			auto const llvm_type = get_llvm_type(type_without_const, context);
			bz_assert(llvm_type->isStructTy());
			return llvm::ConstantAggregateZero::get(llvm::cast<llvm::StructType>(llvm_type));
		}
	case ast::constant_value_kind::void_:
		return nullptr;
	case ast::constant_value_kind::enum_:
	{
		auto const [decl, enum_value] = value.get_enum();
		auto const is_signed = ast::is_signed_integer_kind(decl->underlying_type.get<ast::ts_base_type>().info->kind);
		return llvm::ConstantInt::get(
			get_llvm_type(decl->underlying_type, context),
			enum_value,
			is_signed
		);
	}
	case ast::constant_value_kind::array:
	{
		auto const array_type = type.remove_any_mut();
		bz_assert(array_type.is<ast::ts_array>());
		return get_array_value(
			value.get_array(),
			array_type.get<ast::ts_array>(),
			llvm::cast<llvm::ArrayType>(get_llvm_type(array_type, context)),
			context
		);
	}
	case ast::constant_value_kind::sint_array:
	{
		auto const array_type = type.remove_any_mut();
		bz_assert(array_type.is<ast::ts_array>());
		return get_sint_array_value(
			value.get_sint_array(),
			array_type.get<ast::ts_array>(),
			llvm::cast<llvm::ArrayType>(get_llvm_type(array_type, context)),
			context
		);
	}
	case ast::constant_value_kind::uint_array:
	{
		auto const array_type = type.remove_any_mut();
		bz_assert(array_type.is<ast::ts_array>());
		return get_uint_array_value(
			value.get_uint_array(),
			array_type.get<ast::ts_array>(),
			llvm::cast<llvm::ArrayType>(get_llvm_type(array_type, context)),
			context
		);
	}
	case ast::constant_value_kind::float32_array:
	{
		auto const array_type = type.remove_any_mut();
		bz_assert(array_type.is<ast::ts_array>());
		return get_float32_array_value(
			value.get_float32_array(),
			array_type.get<ast::ts_array>(),
			llvm::cast<llvm::ArrayType>(get_llvm_type(array_type, context)),
			context
		);
	}
	case ast::constant_value_kind::float64_array:
	{
		auto const array_type = type.remove_any_mut();
		bz_assert(array_type.is<ast::ts_array>());
		return get_float64_array_value(
			value.get_float64_array(),
			array_type.get<ast::ts_array>(),
			llvm::cast<llvm::ArrayType>(get_llvm_type(array_type, context)),
			context
		);
	}
	case ast::constant_value_kind::tuple:
	{
		auto const tuple_values = value.get_tuple();
		ast::arena_vector<llvm::Type     *> types = {};
		ast::arena_vector<llvm::Constant *> elems = {};
		types.reserve(tuple_values.size());
		elems.reserve(tuple_values.size());
		if (const_expr != nullptr && const_expr->expr.is<ast::expr_tuple>())
		{
			auto &tuple = const_expr->expr.get<ast::expr_tuple>();
			for (auto const &elem : tuple.elems)
			{
				bz_assert(elem.is_constant());
				auto const &const_elem = elem.get_constant();
				elems.push_back(get_value(const_elem.value, const_elem.type, &const_elem, context));
				types.push_back(elems.back()->getType());
			}
		}
		else
		{
			bz_assert(type.remove_any_mut().is<ast::ts_tuple>());
			auto const &tuple_t = type.remove_any_mut().get<ast::ts_tuple>();
			for (auto const &[val, t] : bz::zip(tuple_values, tuple_t.types))
			{
				elems.push_back(get_value(val, t, nullptr, context));
				types.push_back(elems.back()->getType());
			}
		}
		auto const tuple_type = context.get_tuple_t(types);
		if (elems.empty())
		{
			return llvm::ConstantStruct::getNullValue(tuple_type);
		}
		else
		{
			return llvm::ConstantStruct::get(tuple_type, llvm::ArrayRef(elems.data(), elems.size()));
		}
	}
	case ast::constant_value_kind::function:
	{
		return context.get_function(value.get_function());
	}
	case ast::constant_value_kind::aggregate:
	{
		auto const aggregate = value.get_aggregate();
		bz_assert(type.remove_any_mut().is<ast::ts_base_type>());
		auto const info = type.remove_any_mut().get<ast::ts_base_type>().info;
		auto const val_type = get_llvm_type(type, context);
		bz_assert(val_type->isStructTy());
		auto const val_struct_type = static_cast<llvm::StructType *>(val_type);
		if (aggregate.empty())
		{
			return llvm::ConstantStruct::getNullValue(val_struct_type);
		}
		else
		{
			auto const members = bz::zip(aggregate, info->member_variables)
				.transform([&context](auto const pair) {
					return get_value(pair.first, pair.second->get_type(), nullptr, context);
				})
				.collect();
			return llvm::ConstantStruct::get(val_struct_type, llvm::ArrayRef(members.data(), members.size()));
		}
	}
	case ast::constant_value_kind::type:
		bz_unreachable;
	default:
		bz_unreachable;
	}
}

static llvm::Constant *get_value(
	ast::constant_value const &value,
	ast::typespec_view type,
	ast::constant_expression const *const_expr,
	bitcode_context &context
)
{
	type = type.remove_any_mut();
	if (type.is<ast::ts_optional>() && value.is_null_constant())
	{
		auto const result_type = get_llvm_type(type, context);
		if (result_type->isPointerTy())
		{
			return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(result_type));
		}
		else
		{
			return llvm::ConstantAggregateZero::get(result_type);
		}
	}
	else if (type.is<ast::ts_optional>())
	{
		auto const const_value = get_value_helper(value, type.get<ast::ts_optional>(), const_expr, context);
		if (type.is_optional_pointer_like())
		{
			return const_value;
		}
		else
		{
			auto const result_type = get_llvm_type(type, context);
			bz_assert(result_type->isStructTy());
			return llvm::ConstantStruct::get(
				llvm::cast<llvm::StructType>(result_type),
				const_value, context.builder.getTrue()
			);
		}
	}
	else
	{
		return get_value_helper(value, type, const_expr, context);
	}
}

static void store_constant_at_address(llvm::Constant *const_val, llvm::Value *dest, bitcode_context &context)
{
	auto const type = const_val->getType();
	if (type->isAggregateType() && const_val->isNullValue())
	{
		auto const size = context.get_size(type);
		auto const zero_val = context.builder.getInt8(0);
		auto const align = context.get_data_layout().getPrefTypeAlign(type);
		context.builder.CreateMemSet(dest, zero_val, size, align);
	}
	else if (type->isArrayTy())
	{
		auto const size = type->getArrayNumElements();
		for (auto const i : bz::iota(0, size))
		{
			auto const elem_val = const_val->getAggregateElement(i);
			auto const elem_dest = context.create_struct_gep(type, dest, i);
			store_constant_at_address(elem_val, elem_dest, context);
		}
	}
	else if (type->isStructTy())
	{
		auto const elem_count = type->getStructNumElements();
		for (auto const i : bz::iota(0, elem_count))
		{
			auto const elem_val = const_val->getAggregateElement(i);
			auto const elem_dest = context.create_struct_gep(type, dest, i);
			store_constant_at_address(elem_val, elem_dest, context);
		}
	}
	else
	{
		context.builder.CreateStore(const_val, dest);
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::constant_expression const &const_expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	bz_assert(const_expr.kind != ast::expression_type_kind::noreturn);
	if (
		const_expr.kind == ast::expression_type_kind::type_name
		|| const_expr.kind == ast::expression_type_kind::none
		|| (const_expr.value.is_null() && (
			const_expr.kind == ast::expression_type_kind::function_name
			|| const_expr.kind == ast::expression_type_kind::function_alias_name
			|| const_expr.kind == ast::expression_type_kind::function_overload_set
		))
	)
	{
		bz_assert(result_address == nullptr);
		return val_ptr::get_none();
	}

	val_ptr result = val_ptr::get_none();

	// consteval variable
	if (const_expr.kind == ast::expression_type_kind::lvalue)
	{
		result = const_expr.expr.visit([&](auto const &expr) {
			return emit_bitcode(src_tokens, expr, context, nullptr);
		});
	}
	else
	{
		result.kind = val_ptr::value;
	}

	auto const const_val = get_value(const_expr.value, const_expr.type, &const_expr, context);
	bz_assert(const_val != nullptr);
	result.consteval_val = const_val;
	result.type = const_val->getType();

	if (result_address == nullptr)
	{
		return result;
	}
	else
	{
		store_constant_at_address(const_val, result_address, context);
		return val_ptr::get_reference(result_address, const_val->getType());
	}
}

static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::dynamic_expression const &dyn_expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	if (
		result_address == nullptr
		&& dyn_expr.kind == ast::expression_type_kind::rvalue
		&& !dyn_expr.type.is_reference()
		&& (
			(dyn_expr.destruct_op.not_null() && !dyn_expr.destruct_op.is<ast::trivial_destruct_self>())
			|| dyn_expr.expr.is<ast::expr_compound>()
			|| dyn_expr.expr.is<ast::expr_if>()
			|| dyn_expr.expr.is<ast::expr_switch>()
			|| dyn_expr.expr.is<ast::expr_tuple>()
		)
	)
	{
		auto const result_type = get_llvm_type(dyn_expr.type, context);
		result_address = context.create_alloca(result_type);
	}
	auto const result = dyn_expr.expr.visit([&](auto const &expr) {
		return emit_bitcode(src_tokens, expr, context, result_address);
	});
	if ((result.kind == val_ptr::reference && dyn_expr.destruct_op.not_null()) || dyn_expr.destruct_op.move_destructed_decl != nullptr)
	{
		context.push_self_destruct_operation(
			dyn_expr.destruct_op,
			result.kind == val_ptr::reference ? result.val : nullptr,
			result.get_type()
		);
	}
	return result;
}

static val_ptr emit_bitcode(
	ast::expression const &expr,
	bitcode_context &context,
	llvm::Value *result_address
)
{
	if (context.has_terminator())
	{
		return val_ptr::get_none();
	}
	switch (expr.kind())
	{
	case ast::expression::index_of<ast::constant_expression>:
		return emit_bitcode(expr.src_tokens, expr.get_constant(), context, result_address);
	case ast::expression::index_of<ast::dynamic_expression>:
		return emit_bitcode(expr.src_tokens, expr.get_dynamic(), context, result_address);
	case ast::expression::index_of<ast::error_expression>:
		bz_unreachable;

	default:
		bz_unreachable;
	}
}


// ================================================================
// -------------------------- statement ---------------------------
// ================================================================

static void emit_bitcode(
	ast::stmt_while const &while_stmt,
	bitcode_context &context
)
{
	auto const condition_check_bb = context.add_basic_block("while_condition_check");
	auto const end_bb = context.add_basic_block("endwhile");
	auto const prev_loop_info = context.push_loop(end_bb, condition_check_bb);
	context.builder.CreateBr(condition_check_bb);
	context.builder.SetInsertPoint(condition_check_bb);
	auto const condition_prev_info = context.push_expression_scope();
	auto const condition = emit_bitcode(while_stmt.condition, context, nullptr).get_value(context.builder);
	context.pop_expression_scope(condition_prev_info);
	auto const condition_check_end = context.builder.GetInsertBlock();

	auto const while_bb = context.add_basic_block("while");
	context.builder.SetInsertPoint(while_bb);
	auto const while_block_prev_info = context.push_expression_scope();
	emit_bitcode(while_stmt.while_block, context, nullptr);
	context.pop_expression_scope(while_block_prev_info);
	if (!context.has_terminator())
	{
		context.builder.CreateBr(condition_check_bb);
	}

	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition == nullptr ? context.builder.getFalse() : condition, while_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
	context.pop_loop(prev_loop_info);
}

static void emit_bitcode(
	ast::stmt_for const &for_stmt,
	bitcode_context &context
)
{
	auto const outer_prev_info = context.push_expression_scope();
	if (for_stmt.init.not_null())
	{
		emit_bitcode(for_stmt.init, context);
	}
	auto const condition_check_bb = context.add_basic_block("for_condition_check");
	auto const iteration_bb = context.add_basic_block("for_iteration");
	auto const end_bb = context.add_basic_block("endfor");
	auto const prev_loop_info = context.push_loop(end_bb, iteration_bb);

	context.builder.CreateBr(condition_check_bb);
	context.builder.SetInsertPoint(condition_check_bb);
	auto const condition_prev_info = context.push_expression_scope();
	auto const condition = for_stmt.condition.not_null()
		? emit_bitcode(for_stmt.condition, context, nullptr).get_value(context.builder)
		: context.builder.getTrue();
	context.pop_expression_scope(condition_prev_info);
	auto const condition_check_end = context.builder.GetInsertBlock();

	context.builder.SetInsertPoint(iteration_bb);
	if (for_stmt.iteration.not_null())
	{
		auto const iteration_prev_info = context.push_expression_scope();
		emit_bitcode(for_stmt.iteration, context, nullptr);
		context.pop_expression_scope(iteration_prev_info);
	}
	if (!context.has_terminator())
	{
		context.builder.CreateBr(condition_check_bb);
	}

	auto const for_bb = context.add_basic_block("for");
	context.builder.SetInsertPoint(for_bb);
	auto const for_block_prev_info = context.push_expression_scope();
	emit_bitcode(for_stmt.for_block, context, nullptr);
	context.pop_expression_scope(for_block_prev_info);
	if (!context.has_terminator())
	{
		context.builder.CreateBr(iteration_bb);
	}

	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition == nullptr ? context.builder.getFalse() : condition, for_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
	context.pop_loop(prev_loop_info);
	context.pop_expression_scope(outer_prev_info);
}

static void emit_bitcode(
	ast::stmt_foreach const &foreach_stmt,
	bitcode_context &context
)
{
	auto const outer_prev_info = context.push_expression_scope();
	emit_bitcode(foreach_stmt.range_var_decl, context);
	emit_bitcode(foreach_stmt.iter_var_decl, context);
	emit_bitcode(foreach_stmt.end_var_decl, context);

	auto const condition_check_bb = context.add_basic_block("foreach_condition_check");
	auto const iteration_bb = context.add_basic_block("foreach_iteration");
	auto const end_bb = context.add_basic_block("endforeach");
	auto const prev_loop_info = context.push_loop(end_bb, iteration_bb);

	context.builder.CreateBr(condition_check_bb);
	context.builder.SetInsertPoint(condition_check_bb);
	auto const condition_prev_info = context.push_expression_scope();
	auto const condition = emit_bitcode(foreach_stmt.condition, context, nullptr).get_value(context.builder);
	context.pop_expression_scope(condition_prev_info);
	auto const condition_check_end = context.builder.GetInsertBlock();

	context.builder.SetInsertPoint(iteration_bb);
	auto const iteration_prev_info = context.push_expression_scope();
	emit_bitcode(foreach_stmt.iteration, context, nullptr);
	context.pop_expression_scope(iteration_prev_info);
	bz_assert(!context.has_terminator());
	context.builder.CreateBr(condition_check_bb);

	auto const foreach_bb = context.add_basic_block("foreach");
	context.builder.SetInsertPoint(foreach_bb);
	auto const iter_var_prev_info = context.push_expression_scope();
	emit_bitcode(foreach_stmt.iter_deref_var_decl, context);
	emit_bitcode(foreach_stmt.for_block, context, nullptr);
	context.pop_expression_scope(iter_var_prev_info);
	if (!context.has_terminator())
	{
		context.builder.CreateBr(iteration_bb);
	}

	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition, foreach_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
	context.pop_loop(prev_loop_info);
	context.pop_expression_scope(outer_prev_info);
}

static void emit_bitcode(
	ast::stmt_return const &ret_stmt,
	bitcode_context &context
)
{
	if (ret_stmt.expr.is_null())
	{
		context.emit_all_destruct_operations();
		context.emit_all_end_lifetime_calls();
		if (context.current_function.first->is_main())
		{
			context.builder.CreateRet(context.builder.getInt32(0));
		}
		else
		{
			context.builder.CreateRetVoid();
		}
	}
	else
	{
		if (context.current_function.first->return_type.template is<ast::ts_lvalue_reference>())
		{
			auto const ret_val = emit_bitcode(ret_stmt.expr, context, context.output_pointer);
			context.emit_all_destruct_operations();
			context.emit_all_end_lifetime_calls();
			bz_assert(ret_val.kind == val_ptr::reference);
			context.builder.CreateRet(ret_val.val);
		}
		else if (context.output_pointer != nullptr)
		{
			emit_bitcode(ret_stmt.expr, context, context.output_pointer);
			context.emit_all_destruct_operations();
			context.emit_all_end_lifetime_calls();
			context.builder.CreateRetVoid();
		}
		else
		{
			auto const result_type = get_llvm_type(context.current_function.first->return_type, context);
			auto const ret_kind = context.get_pass_kind(context.current_function.first->return_type, result_type);
			switch (ret_kind)
			{
			case abi::pass_kind::reference:
			case abi::pass_kind::non_trivial:
				bz_unreachable;
			case abi::pass_kind::value:
			{
				auto const ret_val = emit_bitcode(ret_stmt.expr, context, nullptr).get_value(context.builder);
				bz_assert(ret_val != nullptr);
				context.emit_all_destruct_operations();
				context.emit_all_end_lifetime_calls();
				context.builder.CreateRet(ret_val);
				break;
			}
			case abi::pass_kind::one_register:
			case abi::pass_kind::two_registers:
			{
				auto const ret_type = context.current_function.second->getReturnType();
				auto const alloca = context.create_alloca_without_lifetime_start(result_type);
				emit_bitcode(ret_stmt.expr, context, alloca);
				auto const result = context.create_load(ret_type, alloca);
				context.emit_all_destruct_operations();
				context.emit_all_end_lifetime_calls();
				context.builder.CreateRet(result);
				break;
			}
			}
		}
	}
}

static void emit_bitcode(
	ast::stmt_defer const &defer_stmt,
	bitcode_context &context
)
{
	context.push_destruct_operation(defer_stmt.deferred_expr);
}

static void emit_bitcode(
	ast::stmt_no_op const &,
	bitcode_context &
)
{
	// we do nothing
	return;
}

static void emit_bitcode(
	ast::stmt_expression const &expr_stmt,
	bitcode_context &context
)
{
	if (expr_stmt.expr.is<ast::expanded_variadic_expression>())
	{
		for (auto &expr : expr_stmt.expr.get<ast::expanded_variadic_expression>().exprs)
		{
			auto const prev_info = context.push_expression_scope();
			emit_bitcode(expr, context, nullptr);
			context.pop_expression_scope(prev_info);
		}
	}
	else
	{
		auto const prev_info = context.push_expression_scope();
		emit_bitcode(expr_stmt.expr, context, nullptr);
		context.pop_expression_scope(prev_info);
	}
}

static void add_variable_helper(
	ast::decl_variable const &var_decl,
	llvm::Value *ptr,
	llvm::Type *type,
	bitcode_context &context
)
{
	if (var_decl.tuple_decls.empty())
	{
		context.add_variable(&var_decl, ptr, type);
		if (var_decl.is_ever_moved_from() && var_decl.destruction.not_null())
		{
			auto const indicator = context.add_move_destruct_indicator(&var_decl);
			context.push_variable_destruct_operation(var_decl.destruction, indicator);
		}
		else if (var_decl.destruction.not_null())
		{
			context.push_variable_destruct_operation(var_decl.destruction);
		}
	}
	else if (type->isStructTy())
	{
		for (auto const &[decl, i] : var_decl.tuple_decls.enumerate())
		{
			if (decl.get_type().is_any_reference())
			{
				auto const gep_ptr = context.create_struct_gep(type, ptr, i);
				auto const elem_ptr = context.create_load(context.get_opaque_pointer_t(), gep_ptr);
				auto const elem_type = get_llvm_type(decl.get_type().get_any_reference(), context);
				add_variable_helper(decl, elem_ptr, elem_type, context);
			}
			else
			{
				auto const elem_ptr = context.create_struct_gep(type, ptr, i);
				auto const elem_type = type->getStructElementType(i);
				add_variable_helper(decl, elem_ptr, elem_type, context);
			}
		}
	}
	else
	{
		bz_assert(type->isArrayTy());
		auto const elem_type = type->getArrayElementType();
		for (auto const &[decl, i] : var_decl.tuple_decls.enumerate())
		{
			auto const elem_ptr = context.create_struct_gep(type, ptr, i);
			add_variable_helper(decl, elem_ptr, elem_type, context);
		}
	}
}

static void emit_bitcode(
	ast::decl_variable const &var_decl,
	bitcode_context &context
)
{
	if (var_decl.is_global_storage())
	{
		emit_global_variable(var_decl, context);
	}
	else if (var_decl.get_type().is<ast::ts_lvalue_reference>())
	{
		bz_assert(var_decl.init_expr.not_null());
		auto const init_val = [&]() {
			if (var_decl.init_expr.is_error())
			{
				auto const type = get_llvm_type(var_decl.get_type().get<ast::ts_lvalue_reference>(), context);
				return val_ptr::get_reference(context.create_alloca_without_lifetime_start(type), type);
			}
			else
			{
				auto const prev_info = context.push_expression_scope();
				auto const result = emit_bitcode(var_decl.init_expr, context, nullptr);
				context.pop_expression_scope(prev_info);
				return result;
			}
		}();
		bz_assert(init_val.kind == val_ptr::reference);
		add_variable_helper(var_decl, init_val.val, init_val.get_type(), context);
	}
	else
	{
		auto const type = get_llvm_type(var_decl.get_type(), context);
		auto const alloca = context.create_alloca(type);
		if (var_decl.init_expr.not_null())
		{
			auto const prev_info = context.push_expression_scope();
			emit_bitcode(var_decl.init_expr, context, alloca);
			context.pop_expression_scope(prev_info);
		}
		add_variable_helper(var_decl, alloca, type, context);
	}
}


static void emit_bitcode(
	ast::statement const &stmt,
	bitcode_context &context
)
{
	if (context.has_terminator())
	{
		return;
	}

	static_assert(ast::statement::variant_count == 17);
	switch (stmt.kind())
	{
	case ast::statement::index<ast::stmt_while>:
		emit_bitcode(stmt.get<ast::stmt_while>(), context);
		break;
	case ast::statement::index<ast::stmt_for>:
		emit_bitcode(stmt.get<ast::stmt_for>(), context);
		break;
	case ast::statement::index<ast::stmt_foreach>:
		emit_bitcode(stmt.get<ast::stmt_foreach>(), context);
		break;
	case ast::statement::index<ast::stmt_return>:
		emit_bitcode(stmt.get<ast::stmt_return>(), context);
		break;
	case ast::statement::index<ast::stmt_defer>:
		emit_bitcode(stmt.get<ast::stmt_defer>(), context);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		emit_bitcode(stmt.get<ast::stmt_no_op>(), context);
		break;
	case ast::statement::index<ast::stmt_expression>:
		emit_bitcode(stmt.get<ast::stmt_expression>(), context);
		break;
	case ast::statement::index<ast::stmt_static_assert>:
		// nothing
		break;

	case ast::statement::index<ast::decl_variable>:
		emit_bitcode(stmt.get<ast::decl_variable>(), context);
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
		bz_unreachable;
	}
}

static llvm::Function *create_function_from_symbol(
	ast::function_body &func_body,
	bitcode_context &context
)
{
	if (func_body.is_bitcode_emitted())
	{
		return context.get_function(&func_body);
	}

	auto const result_t = get_llvm_type(func_body.return_type, context);
	auto const return_kind = context.get_pass_kind(func_body.return_type, result_t);

	bz::vector<is_byval_and_type_pair> is_arg_byval = {};
	bz::vector<llvm::Type *> args = {};
	is_arg_byval.reserve(func_body.params.size());
	args.reserve(
		func_body.params.size()
		+ (return_kind == abi::pass_kind::reference || return_kind == abi::pass_kind::non_trivial ? 1 : 0)
	);

	if (return_kind == abi::pass_kind::reference || return_kind == abi::pass_kind::non_trivial)
	{
		args.push_back(context.get_opaque_pointer_t());
	}
	if (func_body.is_main())
	{
		auto const str_slice = context.get_slice_t();
		// str_slice is known to be not non_trivial
		auto const pass_kind = abi::get_pass_kind(context.get_platform_abi(), str_slice, context.get_data_layout(), context.get_llvm_context());

		switch (pass_kind)
		{
		case abi::pass_kind::reference:
			is_arg_byval.push_back({ true, str_slice });
			args.push_back(context.get_opaque_pointer_t());
			break;
		case abi::pass_kind::value:
			is_arg_byval.push_back({ false, nullptr });
			args.push_back(str_slice);
			break;
		case abi::pass_kind::one_register:
			is_arg_byval.push_back({ false, nullptr });
			args.push_back(abi::get_one_register_type(context.get_platform_abi(), str_slice, context.get_data_layout(), context.get_llvm_context()));
			break;
		case abi::pass_kind::two_registers:
		{
			auto const [first_type, second_type] = abi::get_two_register_types(
				context.get_platform_abi(),
				str_slice,
				context.get_data_layout(),
				context.get_llvm_context()
			);
			is_arg_byval.push_back({ false, nullptr });
			args.push_back(first_type);
			is_arg_byval.push_back({ false, nullptr });
			args.push_back(second_type);
			break;
		}
		case abi::pass_kind::non_trivial:
			bz_unreachable;
		}
	}
	else
	{
		for (auto &p : func_body.params)
		{
			if (ast::is_generic_parameter(p))
			{
				// skip typename args
				continue;
			}
			auto const t = get_llvm_type(p.get_type(), context);
			auto const pass_kind = context.get_pass_kind(p.get_type(), t);

			switch (pass_kind)
			{
			case abi::pass_kind::reference:
				is_arg_byval.push_back({ true, t });
				args.push_back(context.get_opaque_pointer_t());
				break;
			case abi::pass_kind::value:
				is_arg_byval.push_back({ false, nullptr });
				args.push_back(t);
				break;
			case abi::pass_kind::one_register:
				is_arg_byval.push_back({ false, nullptr });
				args.push_back(abi::get_one_register_type(context.get_platform_abi(), t, context.get_data_layout(), context.get_llvm_context()));
				break;
			case abi::pass_kind::two_registers:
			{
				auto const [first_type, second_type] = abi::get_two_register_types(
					context.get_platform_abi(),
					t,
					context.get_data_layout(),
					context.get_llvm_context()
				);
				is_arg_byval.push_back({ false, nullptr });
				args.push_back(first_type);
				is_arg_byval.push_back({ false, nullptr });
				args.push_back(second_type);
				break;
			}
			case abi::pass_kind::non_trivial:
				is_arg_byval.push_back({ false, nullptr });
				args.push_back(context.get_opaque_pointer_t());
				break;
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
			case abi::pass_kind::non_trivial:
				real_result_t = context.builder.getVoidTy();
				break;
			case abi::pass_kind::value:
				real_result_t = result_t;
				break;
			case abi::pass_kind::one_register:
				real_result_t = abi::get_one_register_type(context.get_platform_abi(), result_t, context.get_data_layout(), context.get_llvm_context());
				break;
			case abi::pass_kind::two_registers:
			{
				auto const [first_type, second_type] = abi::get_two_register_types(
					context.get_platform_abi(),
					result_t,
					context.get_data_layout(),
					context.get_llvm_context()
				);
				real_result_t = llvm::StructType::get(first_type, second_type);
				break;
			}
			}
		}
		return llvm::FunctionType::get(real_result_t, llvm::ArrayRef(args.data(), args.size()), false);
	}();

	bz_assert(func_body.symbol_name != "");
	auto const name = func_body.is_main()
		? llvm::StringRef("__bozon_main")
		: llvm::StringRef(func_body.symbol_name.data_as_char_ptr(), func_body.symbol_name.size());

	auto const linkage = func_body.is_external_linkage()
		? llvm::Function::ExternalLinkage
		: llvm::Function::InternalLinkage;

	auto const fn = llvm::Function::Create(
		func_t, linkage,
		name, context.get_module()
	);

	if (result_t == context.get_bool_t())
	{
		fn->addRetAttr(llvm::Attribute::ZExt);
	}

	switch (func_body.cc)
	{
	static_assert(static_cast<size_t>(::abi::calling_convention::_last) == 3);
	case ::abi::calling_convention::c:
		fn->setCallingConv(llvm::CallingConv::C);
		break;
	case ::abi::calling_convention::fast:
		fn->setCallingConv(llvm::CallingConv::Fast);
		break;
	case ::abi::calling_convention::std:
		fn->setCallingConv(llvm::CallingConv::X86_StdCall);
		break;
	default:
		bz_unreachable;
	}

	auto is_byval_it = is_arg_byval.begin();
	auto is_byval_end = is_arg_byval.end();
	auto arg_it = fn->arg_begin();

	if (return_kind == abi::pass_kind::reference || return_kind == abi::pass_kind::non_trivial)
	{
		arg_it->addAttr(llvm::Attribute::getWithStructRetType(context.get_llvm_context(), result_t));
		arg_it->addAttr(llvm::Attribute::NoAlias);
		arg_it->addAttr(llvm::Attribute::getWithCaptureInfo(context.get_llvm_context(), llvm::CaptureInfo::none()));
		arg_it->addAttr(llvm::Attribute::NonNull);
		++arg_it;
	}

	for (; is_byval_it != is_byval_end; ++is_byval_it, ++arg_it)
	{
		if (is_byval_it->is_byval)
		{
			add_byval_attributes(*arg_it, is_byval_it->type, context);
		}
	}
	return fn;
}

void add_function_to_module(
	ast::function_body *func_body,
	bitcode_context &context
)
{
	auto const fn = create_function_from_symbol(*func_body, context);
	context.funcs_[func_body] = fn;
}

void emit_function_bitcode(
	ast::function_body &func_body,
	bitcode_context &context
)
{
	bz_assert(!func_body.is_bitcode_emitted());
	auto const fn = context.get_function(&func_body);
	bz_assert(fn != nullptr);
	bz_assert(fn->size() == 0);

	context.current_function = { &func_body, fn };

	auto const alloca_bb = context.add_basic_block("alloca");
	context.alloca_bb = alloca_bb;

	auto const entry_bb = context.add_basic_block("entry");
	context.builder.SetInsertPoint(entry_bb);

	bz_assert(func_body.body.is<bz::vector<ast::statement>>());
	ast::arena_vector<llvm::Value *> params = {};
	params.reserve(func_body.params.size());

	auto const outer_prev_info = context.push_expression_scope();
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
			if (p.get_type().is_typename())
			{
				++p_it;
				continue;
			}
			else if (ast::is_generic_parameter(p))
			{
				bz_assert(p.get_type().is<ast::ts_consteval>());
				bz_assert(p.init_expr.is_constant());
				auto const &const_expr = p.init_expr.get_constant();
				auto const val = get_value(const_expr.value, const_expr.type, &const_expr, context);
				auto const alloca = context.create_alloca(val->getType());
				context.builder.CreateStore(val, alloca);
				add_variable_helper(p, alloca, val->getType(), context);
				++p_it;
				continue;
			}
			if (p.get_type().is_any_reference())
			{
				bz_assert(fn_it->getType()->isPointerTy());
				auto const type = p.get_type().get_any_reference();
				add_variable_helper(p, fn_it, get_llvm_type(type, context), context);
			}
			else
			{
				auto const t = get_llvm_type(p.get_type(), context);
				auto const pass_kind = context.get_pass_kind(p.get_type(), t);
				switch (pass_kind)
				{
				case abi::pass_kind::reference:
				case abi::pass_kind::non_trivial:
					add_variable_helper(p, fn_it, t, context);
					break;
				case abi::pass_kind::value:
				{
					auto const alloca = context.create_alloca(t);
					context.builder.CreateStore(fn_it, alloca);
					add_variable_helper(p, alloca, t, context);
					break;
				}
				case abi::pass_kind::one_register:
				{
					auto const alloca = context.create_alloca(t);
					context.builder.CreateStore(fn_it, alloca);
					add_variable_helper(p, alloca, t, context);
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
					auto const struct_type = llvm::StructType::get(first_type, second_type);
					auto const first_address  = context.create_struct_gep(struct_type, alloca, 0);
					auto const second_address = context.create_struct_gep(struct_type, alloca, 1);
					context.builder.CreateStore(first_val, first_address);
					context.builder.CreateStore(second_val, second_address);
					add_variable_helper(p, alloca, t, context);
					break;
				}
				}
			}
			++p_it;
			++fn_it;
		}
	}

	// code emission for statements
	for (auto &stmt : func_body.get_statements())
	{
		emit_bitcode(stmt, context);
	}
	context.pop_expression_scope(outer_prev_info);

	if (!context.has_terminator())
	{
		if (context.current_function.first->is_main())
		{
			context.builder.CreateRet(context.builder.getInt32(0));
		}
		else if (auto const ret_t = context.current_function.second->getReturnType(); ret_t->isVoidTy())
		{
			context.builder.CreateRetVoid();
		}
		else
		{
			context.builder.CreateRet(llvm::UndefValue::get(ret_t));
		}
	}

	context.builder.SetInsertPoint(alloca_bb);
	context.builder.CreateBr(entry_bb);

	// true means it failed
	if (llvm::verifyFunction(*fn, &llvm::dbgs()) == true)
	{
		bz::print(
			stderr,
			"{}verifyFunction failed on '{}' !!!{}\n",
			colors::bright_red,
			func_body.get_signature(),
			colors::clear
		);
		fn->print(llvm::dbgs());
	}
	context.current_function = {};
	context.alloca_bb = nullptr;
	context.output_pointer = nullptr;
	func_body.flags |= ast::function_body::bitcode_emitted;

	// run the function pass manager on the generated function
	if (context.function_pass_manager != nullptr)
	{
		context.function_pass_manager->run(*fn, *context.function_analysis_manager);
	}
}

static void add_global_variable_helper(
	ast::decl_variable const &var_decl,
	llvm::Constant *value,
	llvm::Type *type,
	bitcode_context &context
)
{
	if (var_decl.tuple_decls.empty())
	{
		context.add_variable(&var_decl, value, type);
	}
	else
	{
		for (auto const &[inner_decl, i] : var_decl.tuple_decls.enumerate())
		{
			auto const value_gep = context.create_struct_gep(type, value, i);
			bz_assert(llvm::isa<llvm::Constant>(value_gep));
			auto const inner_type = type->isArrayTy() ? type->getArrayElementType() : type->getStructElementType(i);
			add_global_variable_helper(inner_decl, llvm::cast<llvm::Constant>(value_gep), inner_type, context);
		}
	}
}

static void emit_global_variable_impl(ast::decl_variable const &var_decl, bitcode_context &context)
{
	bz_assert(var_decl.is_global_storage());
	auto const name = var_decl.symbol_name != "" ? var_decl.symbol_name : var_decl.get_id().format_for_symbol(get_unique_id());
	auto const name_ref = llvm::StringRef(name.data_as_char_ptr(), name.size());
	auto const type = get_llvm_type(var_decl.get_type(), context);
	auto const val = context.get_module().getOrInsertGlobal(name_ref, type);
	auto const global_var = llvm::cast<llvm::GlobalVariable>(val);
	if (var_decl.is_external_linkage())
	{
		global_var->setLinkage(llvm::GlobalValue::ExternalLinkage);
	}
	else
	{
		global_var->setLinkage(llvm::GlobalValue::InternalLinkage);
	}
	if (!var_decl.is_extern())
	{
		bz_assert(var_decl.init_expr.is_constant());
		auto const &const_expr = var_decl.init_expr.get_constant();
		auto const init_val = get_value(const_expr.value, const_expr.type, &const_expr, context);
		bz_assert(!global_var->hasInitializer());
		global_var->setInitializer(init_val);
	}

	add_global_variable_helper(var_decl, global_var, type, context);
}

void emit_global_variable(ast::decl_variable const &var_decl, bitcode_context &context)
{
	if (context.vars_.contains(&var_decl))
	{
		return;
	}
	bz_assert(var_decl.global_tuple_decl_parent == nullptr);
	emit_global_variable_impl(var_decl, context);
}

void emit_global_type_symbol(ast::type_info const &info, bitcode_context &context)
{
	if (info.is_generic())
	{
		for (auto const &instantiation : info.generic_instantiations)
		{
			emit_global_type_symbol(*instantiation, context);
		}
		return;
	}

	switch (info.kind)
	{
	case ast::type_info::int8_:
	case ast::type_info::int16_:
	case ast::type_info::int32_:
	case ast::type_info::int64_:
	case ast::type_info::uint8_:
	case ast::type_info::uint16_:
	case ast::type_info::uint32_:
	case ast::type_info::uint64_:
	case ast::type_info::float32_:
	case ast::type_info::float64_:
	case ast::type_info::char_:
	case ast::type_info::bool_:
		break;

	default:
	{
		if (context.types_.find(&info) != context.types_.end())
		{
			return;
		}

		auto const name = llvm::StringRef(info.symbol_name.data_as_char_ptr(), info.symbol_name.size());
		context.add_base_type(&info, llvm::StructType::create(context.get_llvm_context(), name));
		break;
	}
	}
}

void emit_global_type(ast::type_info const &info, bitcode_context &context)
{
	if (info.is_generic())
	{
		for (auto const &instantiation : info.generic_instantiations)
		{
			emit_global_type(*instantiation, context);
		}
		return;
	}

	switch (info.kind)
	{
	case ast::type_info::int8_:
	case ast::type_info::int16_:
	case ast::type_info::int32_:
	case ast::type_info::int64_:
	case ast::type_info::uint8_:
	case ast::type_info::uint16_:
	case ast::type_info::uint32_:
	case ast::type_info::uint64_:
	case ast::type_info::float32_:
	case ast::type_info::float64_:
	case ast::type_info::char_:
	case ast::type_info::bool_:
		break;

	case ast::type_info::forward_declaration:
		break;

	default:
	{
		auto const type = context.get_base_type(&info);
		bz_assert(type != nullptr);
		bz_assert(type->isStructTy());
		auto const struct_type = static_cast<llvm::StructType *>(type);
		if (info.member_variables.not_empty())
		{
			auto const types = info.member_variables
				.transform([&](auto const &member) { return get_llvm_type(member->get_type(), context); })
				.collect<ast::arena_vector>();
			bz_assert(struct_type->isOpaque());
			struct_type->setBody(llvm::ArrayRef<llvm::Type *>(types.data(), types.size()));
		}
		else
		{
			bz_assert(struct_type->isOpaque());
			struct_type->setBody(context.get_uint8_t());
		}
		break;
	}
	}
}

void emit_necessary_functions(bitcode_context &context)
{
	for (size_t i = 0; i < context.functions_to_compile.size(); ++i)
	{
		auto const func_body = context.functions_to_compile[i];
		if (func_body->is_bitcode_emitted())
		{
			continue;
		}
		emit_function_bitcode(*func_body, context);
	}
}

static void emit_rvalue_array_destruct(
	ast::expression const &elem_destruct_expr,
	val_ptr array_value,
	llvm::Value *rvalue_array_elem_ptr,
	bitcode_context &context
)
{
	auto const array_type = array_value.get_type();
	bz_assert(array_type->isArrayTy());
	size_t const size = array_type->getArrayNumElements();
	auto const elem_type = array_type->getArrayElementType();

	if (size <= array_loop_threshold)
	{
		for (auto const i : bz::iota(0, size).reversed())
		{
			auto const elem_ptr = context.create_struct_gep(array_type, array_value.val, i);
			auto const skip_elem = context.builder.CreateICmpEQ(elem_ptr, rvalue_array_elem_ptr);

			auto const begin_bb = context.builder.GetInsertBlock();
			auto const destruct_bb = context.add_basic_block("rvalue_array_destruct_destruct");
			context.builder.SetInsertPoint(destruct_bb);

			auto const prev_value = context.push_value_reference(val_ptr::get_reference(elem_ptr, elem_type));
			emit_bitcode(elem_destruct_expr, context, nullptr);
			context.pop_value_reference(prev_value);

			auto const end_bb = context.add_basic_block("rvalue_array_destruct_end");
			context.builder.CreateBr(end_bb);

			context.builder.SetInsertPoint(begin_bb);
			context.builder.CreateCondBr(skip_elem, end_bb, destruct_bb);

			context.builder.SetInsertPoint(end_bb);
		}
	}
	else
	{
		auto const begin_bb = context.builder.GetInsertBlock();
		auto const begin_elem_ptr = context.create_struct_gep(array_type, array_value.val, 0);
		auto const end_elem_ptr = context.create_struct_gep(array_type, array_value.val, size);

		auto const loop_begin_bb = context.add_basic_block("rvalue_array_destruct_loop_begin");
		context.builder.CreateBr(loop_begin_bb);
		context.builder.SetInsertPoint(loop_begin_bb);

		auto const elem_ptr_phi = context.builder.CreatePHI(end_elem_ptr->getType(), 2);
		elem_ptr_phi->addIncoming(end_elem_ptr, begin_bb);
		auto const elem_ptr = context.builder.CreateConstGEP1_64(elem_type, elem_ptr_phi, uint64_t(-1));

		auto const skip_elem = context.builder.CreateICmpEQ(elem_ptr, rvalue_array_elem_ptr);

		auto const destruct_bb = context.add_basic_block("rvalue_array_destruct_loop_destruct");
		context.builder.SetInsertPoint(destruct_bb);

		auto const prev_value = context.push_value_reference(val_ptr::get_reference(elem_ptr, elem_type));
		emit_bitcode(elem_destruct_expr, context, nullptr);
		context.pop_value_reference(prev_value);

		auto const loop_end_bb = context.add_basic_block("rvalue_array_destruct_loop_end");
		context.builder.CreateBr(loop_end_bb);

		context.builder.SetInsertPoint(loop_begin_bb);
		context.builder.CreateCondBr(skip_elem, loop_end_bb, destruct_bb);

		context.builder.SetInsertPoint(loop_end_bb);
		elem_ptr_phi->addIncoming(elem_ptr, loop_end_bb);
		auto const end_loop = context.builder.CreateICmpEQ(elem_ptr, begin_elem_ptr);

		auto const end_bb = context.add_basic_block("rvalue_array_destruct_end");
		context.builder.CreateCondBr(end_loop, end_bb, loop_begin_bb);

		context.builder.SetInsertPoint(end_bb);
	}
}

static void emit_destruct_operation_impl(
	ast::destruct_operation const &destruct_op,
	val_ptr value,
	llvm::Value *condition,
	llvm::Value *move_destruct_indicator,
	llvm::Value *rvalue_array_elem_ptr,
	bitcode_context &context
)
{
	if (destruct_op.is<ast::destruct_variable>())
	{
		bz_assert(destruct_op.get<ast::destruct_variable>().destruct_call->not_null());
		if (condition != nullptr)
		{
			bz_assert(condition->getType()->isPointerTy());
			auto const destruct_bb = context.add_basic_block("conditional_destruct");
			auto const end_bb = context.add_basic_block("conditional_destruct_end");
			auto const condition_val = context.create_load(context.get_bool_t(), condition);
			context.builder.CreateCondBr(condition_val, destruct_bb, end_bb);

			context.builder.SetInsertPoint(destruct_bb);
			emit_bitcode(*destruct_op.get<ast::destruct_variable>().destruct_call, context, nullptr);
			context.builder.CreateBr(end_bb);

			context.builder.SetInsertPoint(end_bb);
		}
		else
		{
			emit_bitcode(*destruct_op.get<ast::destruct_variable>().destruct_call, context, nullptr);
		}
	}
	else if (destruct_op.is<ast::destruct_self>())
	{
		bz_assert(destruct_op.get<ast::destruct_self>().destruct_call->not_null());
		bz_assert(value.val != nullptr);
		if (condition != nullptr)
		{
			bz_assert(condition->getType()->isPointerTy());
			auto const destruct_bb = context.add_basic_block("conditional_destruct");
			auto const end_bb = context.add_basic_block("conditional_destruct_end");
			auto const condition_val = context.create_load(context.get_bool_t(), condition);
			context.builder.CreateCondBr(condition_val, destruct_bb, end_bb);

			context.builder.SetInsertPoint(destruct_bb);
			auto const prev_value = context.push_value_reference(value);
			emit_bitcode(*destruct_op.get<ast::destruct_self>().destruct_call, context, nullptr);
			context.pop_value_reference(prev_value);
			context.builder.CreateBr(end_bb);

			context.builder.SetInsertPoint(end_bb);
		}
		else
		{
			auto const prev_value = context.push_value_reference(value);
			emit_bitcode(*destruct_op.get<ast::destruct_self>().destruct_call, context, nullptr);
			context.pop_value_reference(prev_value);
		}
	}
	else if (destruct_op.is<ast::trivial_destruct_self>())
	{
		// nothing
	}
	else if (destruct_op.is<ast::defer_expression>())
	{
		bz_assert(condition == nullptr);
		auto const prev_info = context.push_expression_scope();
		emit_bitcode(*destruct_op.get<ast::defer_expression>().expr, context, nullptr);
		context.pop_expression_scope(prev_info);
	}
	else if (destruct_op.is<ast::destruct_rvalue_array>())
	{
		bz_assert(rvalue_array_elem_ptr != nullptr);
		if (condition != nullptr)
		{
			bz_assert(condition->getType()->isPointerTy());
			auto const destruct_bb = context.add_basic_block("conditional_destruct");
			auto const end_bb = context.add_basic_block("conditional_destruct_end");
			auto const condition_val = context.create_load(context.get_bool_t(), condition);
			context.builder.CreateCondBr(condition_val, destruct_bb, end_bb);

			context.builder.SetInsertPoint(destruct_bb);
			emit_rvalue_array_destruct(
				*destruct_op.get<ast::destruct_rvalue_array>().elem_destruct_call,
				value,
				rvalue_array_elem_ptr,
				context
			);
			context.builder.CreateBr(end_bb);

			context.builder.SetInsertPoint(end_bb);
		}
		else
		{
			emit_rvalue_array_destruct(
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
		bz_assert(destruct_op.is_null());
		// nothing
	}

	if (move_destruct_indicator != nullptr)
	{
		context.builder.CreateStore(context.builder.getFalse(), move_destruct_indicator);
	}
}

void emit_destruct_operation(
	ast::destruct_operation const &destruct_op,
	llvm::Value *condition,
	llvm::Value *move_destruct_indicator,
	bitcode_context &context
)
{
	emit_destruct_operation_impl(
		destruct_op,
		val_ptr::get_none(),
		condition,
		move_destruct_indicator,
		nullptr,
		context
	);
}

void emit_destruct_operation(
	ast::destruct_operation const &destruct_op,
	val_ptr value,
	llvm::Value *condition,
	llvm::Value *move_destruct_indicator,
	llvm::Value *rvalue_array_elem_ptr,
	bitcode_context &context
)
{
	emit_destruct_operation_impl(
		destruct_op,
		value,
		condition,
		move_destruct_indicator,
		rvalue_array_elem_ptr,
		context
	);
}

} // namespace codegen::llvm_latest
