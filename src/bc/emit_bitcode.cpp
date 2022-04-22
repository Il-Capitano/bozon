#include "emit_bitcode.h"
#include "global_data.h"
#include "colors.h"
#include "ctx/global_context.h"

#include <llvm/IR/Verifier.h>

namespace bc
{

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	ast::expression const &expr,
	Context &context,
	llvm::Value *result_address
);

template<abi::platform_abi abi, typename Context>
static void emit_bitcode(
	ast::statement const &stmt,
	Context &context
);

template<abi::platform_abi abi, typename Context>
static val_ptr emit_copy_constructor(
	lex::src_tokens const &src_tokens,
	val_ptr expr_val,
	ast::typespec_view expr_type,
	Context &context,
	llvm::Value *result_address
);

template<typename Context>
static constexpr bool is_comptime = bz::meta::is_same<Context, ctx::comptime_executor_context>;

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

static src_tokens_llvm_value_t get_src_tokens_llvm_value(lex::src_tokens const &src_tokens, ctx::comptime_executor_context &context)
{
	auto const begin = llvm::ConstantInt::get(context.get_uint64_t(), reinterpret_cast<uint64_t>(&*src_tokens.begin));
	auto const pivot = llvm::ConstantInt::get(context.get_uint64_t(), reinterpret_cast<uint64_t>(&*src_tokens.pivot));
	auto const end   = llvm::ConstantInt::get(context.get_uint64_t(), reinterpret_cast<uint64_t>(&*src_tokens.end));
	return { begin, pivot, end };
}

template<typename Context>
static llvm::Value *get_constant_zero(
	ast::typespec_view type,
	llvm::Type *llvm_type,
	Context &context
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
			return llvm::ConstantStruct::getNullValue(llvm_type);
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
	case ast::typespec_node_t::index_of<ast::ts_move_reference>:
	case ast::typespec_node_t::index_of<ast::ts_auto>:
	default:
		bz_unreachable;
	}
}

static llvm::Value *emit_get_error_count(ctx::comptime_executor_context &context)
{
	return context.create_call(context.get_comptime_function(ctx::comptime_function_kind::get_error_count));
}

static void emit_error_check(llvm::Value *pre_call_error_count, ctx::comptime_executor_context &context)
{
	if (context.current_function.first != nullptr && context.current_function.first->is_no_comptime_checking())
	{
		return;
	}
	bz_assert(context.error_bb != nullptr);
	auto const error_count = emit_get_error_count(context);
	auto const has_error_val = context.builder.CreateICmpNE(pre_call_error_count, error_count);
	auto const continue_bb = context.add_basic_block("error_check_continue");
	context.builder.CreateCondBr(has_error_val, context.error_bb, continue_bb);
	context.builder.SetInsertPoint(continue_bb);
}

static void emit_error_assert(llvm::Value *bool_val, ctx::comptime_executor_context &context)
{
	if (context.current_function.first != nullptr && context.current_function.first->is_no_comptime_checking())
	{
		return;
	}
	bz_assert(context.error_bb != nullptr);
	auto const continue_bb = context.add_basic_block("error_assert_continue");
	context.builder.CreateCondBr(bool_val, continue_bb, context.error_bb);
	context.builder.SetInsertPoint(continue_bb);
}

static void emit_index_bounds_check(
	lex::src_tokens const &src_tokens,
	llvm::Value *index_val,
	llvm::Value *array_size,
	bool is_index_unsigned,
	ctx::comptime_executor_context &context
)
{
	auto const error_kind_val  = llvm::ConstantInt::get(context.get_uint32_t(), static_cast<uint32_t>(ctx::warning_kind::_last));
	auto const [error_begin_val, error_pivot_val, error_end_val] = get_src_tokens_llvm_value(src_tokens, context);
	if (is_index_unsigned)
	{
		auto const index_val_u64 = context.builder.CreateIntCast(index_val, context.get_uint64_t(), false);
		auto const is_in_bounds = context.create_call(
			context.get_comptime_function(ctx::comptime_function_kind::index_check_unsigned),
			{ index_val_u64, array_size, error_kind_val, error_begin_val, error_pivot_val, error_end_val }
		);
		emit_error_assert(is_in_bounds, context);
	}
	else
	{
		auto const index_val_i64 = context.builder.CreateIntCast(index_val, context.get_int64_t(), true);
		auto const is_in_bounds = context.create_call(
			context.get_comptime_function(ctx::comptime_function_kind::index_check_signed),
			{ index_val_i64, array_size, error_kind_val, error_begin_val, error_pivot_val, error_end_val }
		);
		emit_error_assert(is_in_bounds, context);
	}
}

static void emit_error(
	lex::src_tokens const &src_tokens,
	bz::u8string message,
	ctx::comptime_executor_context &context
)
{
	bz_assert(src_tokens.begin != nullptr && src_tokens.pivot != nullptr && src_tokens.end != nullptr);
	if (context.current_function.first != nullptr && context.current_function.first->is_no_comptime_checking())
	{
		return;
	}
	auto const error_kind_val  = llvm::ConstantInt::get(context.get_uint32_t(), static_cast<uint32_t>(ctx::warning_kind::_last));
	auto const [error_begin_val, error_pivot_val, error_end_val] = get_src_tokens_llvm_value(src_tokens, context);
	auto const string_constant = context.create_string(message);
	auto const string_type = llvm::ArrayType::get(context.get_uint8_t(), message.size() + 1);
	auto const message_val = context.create_struct_gep(string_type, string_constant, 0);
	context.create_call(
		context.get_comptime_function(ctx::comptime_function_kind::add_error),
		{ error_kind_val, error_begin_val, error_pivot_val, error_end_val, message_val }
	);
	auto const continue_bb = context.add_basic_block("error_dummy_continue");
	context.builder.CreateCondBr(llvm::ConstantInt::getFalse(context.get_llvm_context()), continue_bb, context.error_bb);
	context.builder.SetInsertPoint(continue_bb);
}

[[nodiscard]] llvm::Value *emit_push_call(
	lex::src_tokens const &src_tokens,
	ast::function_body const *func_body,
	ctx::comptime_executor_context &context
)
{
	if (!context.do_error_checking())
	{
		return nullptr;
	}
	auto const call_ptr = context.insert_call(src_tokens, func_body);
	auto const call_ptr_int_val = llvm::ConstantInt::get(
		context.get_uint64_t(),
		reinterpret_cast<uint64_t>(call_ptr)
	);
	auto const error_count = emit_get_error_count(context);
	context.builder.CreateCall(context.get_comptime_function(ctx::comptime_function_kind::push_call), { call_ptr_int_val });
	return error_count;
}

void emit_pop_call(llvm::Value *pre_call_error_count, ctx::comptime_executor_context &context)
{
	if (!context.do_error_checking())
	{
		return;
	}
	context.builder.CreateCall(context.get_comptime_function(ctx::comptime_function_kind::pop_call));
	emit_error_check(pre_call_error_count, context);
}

template<abi::platform_abi abi, bool push_to_front = false, typename Context>
static void add_call_parameter(
	ast::typespec_view param_type,
	llvm::Type *param_llvm_type,
	val_ptr param,
	ast::arena_vector<llvm::Value *> &params,
	ast::arena_vector<is_byval_and_type_pair> &params_is_byval,
	Context &context
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
	// special case for *void and *const void
	else if (ast::remove_const_or_consteval(ast::remove_pointer(param_type)).is<ast::ts_void>())
	{
		auto const void_ptr_val = context.builder.CreatePointerCast(
			param.get_value(context.builder),
			llvm::PointerType::getInt8PtrTy(context.get_llvm_context())
		);
		(params.*params_push)(void_ptr_val);
		(params_is_byval.*byval_push)({ false, nullptr });
	}
	else
	{
		auto const pass_kind = context.template get_pass_kind<abi>(param_type, param_llvm_type);

		switch (pass_kind)
		{
		case abi::pass_kind::reference:
			if (
				param.kind == val_ptr::reference
				&& abi::get_pass_by_reference_attributes<abi>().contains(llvm::Attribute::ByVal)
			)
			{
				// there's no need to provide a seperate copy for a byval argument,
				// as a copy is made at the call site automatically
				// see: https://reviews.llvm.org/D79636
				(params.*params_push)(param.val);
			}
			else
			{
				auto const alloca = context.create_alloca(param_llvm_type);
				emit_copy_constructor<abi>({}, param, param_type, context, alloca);
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
				abi::get_one_register_type<abi>(param_llvm_type, context.get_data_layout(), context.get_llvm_context())
			));
			(params_is_byval.*byval_push)({ false, nullptr });
			break;
		case abi::pass_kind::two_registers:
		{
			auto const [first_type, second_type] = abi::get_two_register_types<abi>(
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

template<abi::platform_abi abi, typename Context>
static void add_byval_attributes(llvm::CallInst *call, llvm::Type *byval_type, unsigned index, Context &context)
{
	auto const attributes = abi::get_pass_by_reference_attributes<abi>();
	for (auto const attribute : attributes)
	{
		switch (attribute)
		{
		case llvm::Attribute::ByVal:
		{
			call->addParamAttr(index, llvm::Attribute::getWithByValType(context.get_llvm_context(), byval_type));
			break;
		}
		default:
			call->addParamAttr(index, attribute);
			break;
		}
	}
}

template<abi::platform_abi abi, typename Context>
static void add_byval_attributes(llvm::Argument &arg, llvm::Type *byval_type, Context &context)
{
	auto const attributes = abi::get_pass_by_reference_attributes<abi>();
	for (auto const attribute : attributes)
	{
		switch (attribute)
		{
		case llvm::Attribute::ByVal:
		{
			arg.addAttr(llvm::Attribute::getWithByValType(context.get_llvm_context(), byval_type));
			break;
		}
		default:
			arg.addAttr(attribute);
			break;
		}
	}
}

template<abi::platform_abi abi, typename Context>
static void create_function_call(
	lex::src_tokens const &src_tokens,
	ast::function_body *body,
	val_ptr lhs,
	val_ptr rhs,
	Context &context
)
{
	bz_assert(lhs.kind == val_ptr::reference);
	bz_assert(rhs.kind == val_ptr::reference);
	auto const fn = context.get_function(body);
	bz_assert(fn != nullptr);
	auto const result_pass_kind = context.template get_pass_kind<abi>(body->return_type);

	bz_assert(result_pass_kind != abi::pass_kind::reference);
	bz_assert(body->params[0].get_type().is<ast::ts_lvalue_reference>());

	ast::arena_vector<llvm::Value *> params;
	ast::arena_vector<is_byval_and_type_pair> params_is_byval;
	params.reserve(3);
	params.push_back(lhs.val);

	params_is_byval.reserve(2);

	{
		auto const &rhs_p_t = body->params[1].get_type();
		auto const rhs_llvm_type = get_llvm_type(rhs_p_t, context);
		add_call_parameter<abi>(rhs_p_t, rhs_llvm_type, rhs, params, params_is_byval, context);
	}

	auto const call = context.create_call(src_tokens, body, fn, llvm::ArrayRef(params.data(), params.size()));
	if (params_is_byval[0].is_byval)
	{
		add_byval_attributes<abi>(call, params_is_byval[0].type, 1, context);
	}
}

template<typename Context>
static void push_destructor_call(
	lex::src_tokens const &src_tokens,
	llvm::Value *ptr,
	ast::typespec_view type,
	Context &context
)
{
	type = ast::remove_const_or_consteval(type);
	if (ast::is_trivially_destructible(type))
	{
		return;
	}
	if (type.is<ast::ts_base_type>())
	{
		auto const &info = *type.get<ast::ts_base_type>().info;
		auto const llvm_type = get_llvm_type(type, context);
		for (auto const &[member, i] : info.member_variables.enumerate())
		{
			auto const member_ptr = context.create_struct_gep(llvm_type, ptr, i);
			push_destructor_call(src_tokens, member_ptr, member->get_type(), context);
		}
		if (info.destructor != nullptr)
		{
			if constexpr (is_comptime<Context>)
			{
				context.push_destructor_call(src_tokens, &info.destructor->body, ptr);
			}
			else
			{
				auto const dtor_func = context.get_function(&info.destructor->body);
				context.push_destructor_call(dtor_func, ptr);
			}
		}
	}
	else if (type.is<ast::ts_tuple>())
	{
		auto const llvm_type = get_llvm_type(type, context);
		for (auto const &[member_type, i] : type.get<ast::ts_tuple>().types.enumerate())
		{
			auto const member_ptr = context.create_struct_gep(llvm_type, ptr, i);
			push_destructor_call(src_tokens, member_ptr, member_type, context);
		}
	}
	else if (type.is<ast::ts_array>())
	{
		auto const array_size = type.get<ast::ts_array>().size;
		auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
		auto const llvm_type = get_llvm_type(type, context);
		for (auto const i : bz::iota(0, array_size))
		{
			auto const elem_ptr = context.create_struct_gep(llvm_type, ptr, i);
			push_destructor_call(src_tokens, elem_ptr, elem_type, context);
		}
	}
	else
	{
		// nothing
	}
}

template<typename Context>
static void emit_destructor_call(
	lex::src_tokens const &src_tokens,
	llvm::Value *ptr,
	ast::typespec_view type,
	Context &context
)
{
	type = ast::remove_const_or_consteval(type);
	if (ast::is_trivially_destructible(type))
	{
		return;
	}
	if (type.is<ast::ts_base_type>())
	{
		auto const &info = *type.get<ast::ts_base_type>().info;
		if (info.destructor != nullptr)
		{
			auto const dtor_func_body = &info.destructor->body;
			auto const dtor_func = context.get_function(dtor_func_body);
			context.create_call(src_tokens, dtor_func_body, dtor_func, ptr);
		}
		auto const members_count = info.member_variables.size();
		auto const llvm_type = get_llvm_type(type, context);
		for (auto const &[member, i] : info.member_variables.reversed().enumerate())
		{
			auto const member_ptr = context.create_struct_gep(llvm_type, ptr, members_count - i - 1);
			emit_destructor_call(src_tokens, member_ptr, member->get_type(), context);
		}
	}
	else if (type.is<ast::ts_tuple>())
	{
		auto const members_count = type.get<ast::ts_tuple>().types.size();
		auto const llvm_type = get_llvm_type(type, context);
		for (auto const &[member_type, i] : type.get<ast::ts_tuple>().types.reversed().enumerate())
		{
			auto const member_ptr = context.create_struct_gep(llvm_type, ptr, members_count - i - 1);
			emit_destructor_call(src_tokens, member_ptr, member_type, context);
		}
	}
	else if (type.is<ast::ts_array>())
	{
		auto const array_size = type.get<ast::ts_array>().size;
		auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
		auto const llvm_type = get_llvm_type(type, context);
		for (auto const i : bz::iota(0, array_size))
		{
			auto const elem_ptr = context.create_struct_gep(llvm_type, ptr, array_size - i - 1);
			emit_destructor_call(src_tokens, elem_ptr, elem_type, context);
		}
	}
	else
	{
		// nothing
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_copy_constructor(
	lex::src_tokens const &src_tokens,
	val_ptr expr_val,
	ast::typespec_view expr_type,
	Context &context,
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
		return val_ptr::get_reference(result_address, expr_val.get_type());
	}

	if (result_address == nullptr)
	{
		result_address = context.create_alloca(get_llvm_type(expr_type, context));
	}
	expr_type = ast::remove_const_or_consteval(expr_type);

	if (ast::is_trivially_copy_constructible(expr_type))
	{
		if (auto const size = context.get_size(expr_val.get_type()); size > 16)
		{
			auto const memcpy_fn = context.get_function(context.get_builtin_function(ast::function_body::memcpy));
			bz_assert(expr_val.kind == val_ptr::reference);
			auto const dest_ptr = context.builder.CreatePointerCast(result_address, llvm::PointerType::get(context.get_uint8_t(), 0));
			auto const src_ptr = context.builder.CreatePointerCast(expr_val.val, llvm::PointerType::get(context.get_uint8_t(), 0));
			auto const size_val = llvm::ConstantInt::get(context.get_uint64_t(), size);
			auto const false_val = llvm::ConstantInt::getFalse(context.get_llvm_context());
			context.create_call(memcpy_fn, { dest_ptr, src_ptr, size_val, false_val });
		}
		else
		{
			context.builder.CreateStore(expr_val.get_value(context.builder), result_address);
		}
		return val_ptr::get_reference(result_address, expr_val.get_type());
	}

	if (expr_type.is<ast::ts_base_type>())
	{
		auto const info = expr_type.get<ast::ts_base_type>().info;
		if (info->copy_constructor != nullptr)
		{
			auto const func_body = &info->copy_constructor->body;
			auto const fn = context.get_function(func_body);
			auto const expr_llvm_type = get_llvm_type(expr_type, context);
			auto const ret_kind = context.template get_pass_kind<abi>(expr_type, expr_llvm_type);
			switch (ret_kind)
			{
			case abi::pass_kind::value:
			{
				auto const call = context.create_call(src_tokens, func_body, fn, expr_val.val);
				context.builder.CreateStore(call, result_address);
				break;
			}
			case abi::pass_kind::reference:
			case abi::pass_kind::non_trivial:
			{
				auto const call = context.create_call(src_tokens, func_body, fn, { result_address, expr_val.val });
				call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context.get_llvm_context(), expr_llvm_type));
				break;
			}
			case abi::pass_kind::one_register:
			case abi::pass_kind::two_registers:
			{
				auto const call = context.create_call(src_tokens, func_body, fn, expr_val.val);
				auto const cast_result_address = context.builder.CreatePointerCast(
					result_address, llvm::PointerType::get(call->getType(), 0)
				);
				context.builder.CreateStore(call, cast_result_address);
				break;
			}
			}
		}
		else if (info->default_copy_constructor != nullptr)
		{
			for (auto const &[member, i] : info->member_variables.enumerate())
			{
				bz_assert(!member->get_type().template is<ast::ts_lvalue_reference>());
				bz_assert(expr_val.get_type()->isStructTy());
				auto const element_type = expr_val.get_type()->getStructElementType(i);
				emit_copy_constructor<abi>(
					src_tokens,
					val_ptr::get_reference(context.create_struct_gep(expr_val.get_type(), expr_val.val, i), element_type),
					member->get_type(),
					context,
					context.create_struct_gep(expr_val.get_type(), result_address, i)
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
		bz_assert(expr_val.get_type()->isArrayTy());
		auto const element_type = expr_val.get_type()->getArrayElementType();
		for (auto const i : bz::iota(0, expr_type.get<ast::ts_array>().size))
		{
			emit_copy_constructor<abi>(
				src_tokens,
				val_ptr::get_reference(context.create_struct_gep(expr_val.get_type(), expr_val.val, i), element_type),
				type,
				context,
				context.create_struct_gep(expr_val.get_type(), result_address, i)
			);
		}
	}
	else if (expr_type.is<ast::ts_tuple>())
	{
		for (auto const &[member_type, i] : expr_type.get<ast::ts_tuple>().types.enumerate())
		{
			bz_assert(expr_val.get_type()->isStructTy());
			auto const element_type = expr_val.get_type()->getStructElementType(i);
			emit_copy_constructor<abi>(
				src_tokens,
				val_ptr::get_reference(context.create_struct_gep(expr_val.get_type(), expr_val.val, i), element_type),
				member_type,
				context,
				context.create_struct_gep(expr_val.get_type(), result_address, i)
			);
		}
	}
	else
	{
		context.builder.CreateStore(expr_val.get_value(context.builder), result_address);
	}
	return val_ptr::get_reference(result_address, expr_val.get_type());
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_default_constructor(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	Context &context,
	llvm::Value *result_address
)
{
	type = ast::remove_const_or_consteval(type);
	if (result_address == nullptr)
	{
		result_address = context.create_alloca(get_llvm_type(type, context));
	}

	auto const llvm_type = get_llvm_type(type, context);

	if (ast::is_default_zero_initialized(type))
	{
		if (auto const size = context.get_size(llvm_type); size > 16)
		{
			auto const memset_fn = context.get_function(context.get_builtin_function(ast::function_body::memset));
			auto const dest_ptr = context.builder.CreatePointerCast(result_address, llvm::PointerType::get(context.get_uint8_t(), 0));
			auto const zero_val = llvm::ConstantInt::get(context.get_uint8_t(), 0);
			auto const size_val = llvm::ConstantInt::get(context.get_uint64_t(), size);
			auto const false_val = llvm::ConstantInt::getFalse(context.get_llvm_context());
			context.create_call(memset_fn, { dest_ptr, zero_val, size_val, false_val });
		}
		else
		{
			auto const zero_init_val = get_constant_zero(type, llvm_type, context);
			context.builder.CreateStore(zero_init_val, result_address);
		}
		return val_ptr::get_reference(result_address, llvm_type);
	}

	if (type.is<ast::ts_base_type>())
	{
		auto const info = type.get<ast::ts_base_type>().info;
		if (info->default_constructor != nullptr)
		{
			auto const func_body = &info->default_constructor->body;
			auto const fn = context.get_function(func_body);
			auto const ret_kind = context.template get_pass_kind<abi>(type, llvm_type);
			switch (ret_kind)
			{
			case abi::pass_kind::value:
			{
				auto const call = context.create_call(src_tokens, func_body, fn);
				context.builder.CreateStore(call, result_address);
				break;
			}
			case abi::pass_kind::reference:
			case abi::pass_kind::non_trivial:
			{
				auto const call = context.create_call(src_tokens, func_body, fn, result_address);
				call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context.get_llvm_context(), llvm_type));
				break;
			}
			case abi::pass_kind::one_register:
			case abi::pass_kind::two_registers:
			{
				auto const call = context.create_call(src_tokens, func_body, fn);
				auto const cast_result_address = context.builder.CreatePointerCast(
					result_address, llvm::PointerType::get(call->getType(), 0)
				);
				context.builder.CreateStore(call, cast_result_address);
				break;
			}
			}
		}
		else if (info->default_default_constructor != nullptr)
		{
			for (auto const &[member, i] : info->member_variables.enumerate())
			{
				emit_default_constructor<abi>(
					src_tokens,
					member->get_type(),
					context,
					context.create_struct_gep(llvm_type, result_address, i)
				);
			}
		}
		else
		{
			context.builder.CreateStore(get_constant_zero(type, llvm_type, context), result_address);
		}
	}
	else if (type.is<ast::ts_array>())
	{
		auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
		for (auto const i : bz::iota(0, type.get<ast::ts_array>().size))
		{
			emit_default_constructor<abi>(
				src_tokens,
				elem_type,
				context,
				context.create_struct_gep(llvm_type, result_address, i)
			);
		}
	}
	else if (type.is<ast::ts_tuple>())
	{
		for (auto const &[member_type, i] : type.get<ast::ts_tuple>().types.enumerate())
		{
			emit_default_constructor<abi>(
				src_tokens,
				member_type,
				context,
				context.create_struct_gep(llvm_type, result_address, i)
			);
		}
	}
	else
	{
		context.builder.CreateStore(get_constant_zero(type, llvm_type, context), result_address);
	}
	return val_ptr::get_reference(result_address, llvm_type);
}

template<abi::platform_abi abi, typename Context>
static void emit_copy_assign(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	val_ptr lhs,
	val_ptr rhs,
	Context &context
)
{
	bz_assert(lhs.kind == val_ptr::reference);
	if (rhs.kind == val_ptr::value)
	{
		context.builder.CreateStore(rhs.get_value(context.builder), lhs.val);
		return;
	}

	if (type.is<ast::ts_base_type>())
	{
		auto const info = type.get<ast::ts_base_type>().info;
		if (info->op_assign != nullptr && !info->op_assign->body.is_intrinsic())
		{
			create_function_call<abi>(src_tokens, &info->op_assign->body, lhs, rhs, context);
		}
		else if (info->default_op_assign != nullptr)
		{
			for (auto const &[member, i] : info->member_variables.enumerate())
			{
				bz_assert(!member->get_type().template is<ast::ts_lvalue_reference>());
				bz_assert(lhs.get_type() == rhs.get_type());
				bz_assert(lhs.get_type()->isStructTy());
				auto const element_type = lhs.get_type()->getStructElementType(i);
				emit_copy_assign<abi>(
					src_tokens,
					member->get_type(),
					val_ptr::get_reference(context.create_struct_gep(lhs.get_type(), lhs.val, i), element_type),
					val_ptr::get_reference(context.create_struct_gep(rhs.get_type(), rhs.val, i), element_type),
					context
				);
			}
		}
		else
		{
			bz_assert(info->kind != ast::type_info::aggregate);
			context.builder.CreateStore(rhs.get_value(context.builder), lhs.val);
		}
	}
	else if (type.is<ast::ts_array>())
	{
		auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
		bz_assert(lhs.get_type() == rhs.get_type());
		bz_assert(lhs.get_type()->isArrayTy());
		auto const element_type = lhs.get_type()->getArrayElementType();
		for (auto const i : bz::iota(0, type.get<ast::ts_array>().size))
		{
			emit_copy_assign<abi>(
				src_tokens,
				elem_type,
				val_ptr::get_reference(context.create_struct_gep(lhs.get_type(), lhs.val, i), element_type),
				val_ptr::get_reference(context.create_struct_gep(rhs.get_type(), rhs.val, i), element_type),
				context
			);
		}
	}
	else if (type.is<ast::ts_tuple>())
	{
		for (auto const &[member_type, i] : type.get<ast::ts_tuple>().types.enumerate())
		{
			bz_assert(!member_type.template is<ast::ts_lvalue_reference>());
			bz_assert(lhs.get_type() == rhs.get_type());
			bz_assert(lhs.get_type()->isStructTy());
			auto const element_type = lhs.get_type()->getStructElementType(i);
			emit_copy_assign<abi>(
				src_tokens,
				member_type,
				val_ptr::get_reference(context.create_struct_gep(lhs.get_type(), lhs.val, i), element_type),
				val_ptr::get_reference(context.create_struct_gep(rhs.get_type(), rhs.val, i), element_type),
				context
			);
		}
	}
	else
	{
		context.builder.CreateStore(rhs.get_value(context.builder), lhs.val);
	}
}

template<abi::platform_abi abi, typename Context>
static void emit_move_assign(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	val_ptr lhs,
	val_ptr rhs,
	Context &context
)
{
	bz_assert(lhs.kind == val_ptr::reference);
	if (rhs.kind == val_ptr::value)
	{
		context.builder.CreateStore(rhs.get_value(context.builder), lhs.val);
		return;
	}

	if (type.is<ast::ts_base_type>())
	{
		auto const info = type.get<ast::ts_base_type>().info;
		if (info->op_assign != nullptr && info->op_move_assign == nullptr)
		{
			emit_copy_assign<abi>(src_tokens, type, lhs, rhs, context);
		}
		else if (info->op_move_assign != nullptr && !info->op_move_assign->body.is_intrinsic())
		{
			create_function_call<abi>(src_tokens, &info->op_move_assign->body, lhs, rhs, context);
		}
		else if (info->default_op_move_assign != nullptr)
		{
			for (auto const &[member, i] : info->member_variables.enumerate())
			{
				bz_assert(!member->get_type().template is<ast::ts_lvalue_reference>());
				bz_assert(lhs.get_type() == rhs.get_type());
				bz_assert(lhs.get_type()->isStructTy());
				auto const element_type = lhs.get_type()->getStructElementType(i);
				emit_move_assign<abi>(
					src_tokens,
					member->get_type(),
					val_ptr::get_reference(context.create_struct_gep(lhs.get_type(), lhs.val, i), element_type),
					val_ptr::get_reference(context.create_struct_gep(rhs.get_type(), rhs.val, i), element_type),
					context
				);
			}
		}
		else
		{
			bz_assert(info->kind != ast::type_info::aggregate);
			context.builder.CreateStore(rhs.get_value(context.builder), lhs.val);
		}
	}
	else if (type.is<ast::ts_array>())
	{
		auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
		bz_assert(lhs.get_type() == rhs.get_type());
		bz_assert(lhs.get_type()->isArrayTy());
		auto const element_type = lhs.get_type()->getArrayElementType();
		for (auto const i : bz::iota(0, type.get<ast::ts_array>().size))
		{
			emit_move_assign<abi>(
				src_tokens,
				elem_type,
				val_ptr::get_reference(context.create_struct_gep(lhs.get_type(), lhs.val, i), element_type),
				val_ptr::get_reference(context.create_struct_gep(rhs.get_type(), rhs.val, i), element_type),
				context
			);
		}
	}
	else if (type.is<ast::ts_tuple>())
	{
		for (auto const &[member_type, i] : type.get<ast::ts_tuple>().types.enumerate())
		{
			bz_assert(!member_type.template is<ast::ts_lvalue_reference>());
			bz_assert(lhs.get_type() == rhs.get_type());
			bz_assert(lhs.get_type()->isStructTy());
			auto const element_type = lhs.get_type()->getStructElementType(i);
			emit_move_assign<abi>(
				src_tokens,
				member_type,
				val_ptr::get_reference(context.create_struct_gep(lhs.get_type(), lhs.val, i), element_type),
				val_ptr::get_reference(context.create_struct_gep(rhs.get_type(), rhs.val, i), element_type),
				context
			);
		}
	}
	else
	{
		context.builder.CreateStore(rhs.get_value(context.builder), lhs.val);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_default_copy_assign(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_type = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_type = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);
	auto const is_rhs_null_pointer = lhs_type.is<ast::ts_pointer>() && rhs_type.is<ast::ts_base_type>();
	bz_assert(!is_rhs_null_pointer || rhs_type.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val.kind == val_ptr::reference);

	if (is_rhs_null_pointer)
	{
		auto const lhs_llvm_type = lhs_val.get_type();
		bz_assert(lhs_llvm_type->isPointerTy());
		auto const rhs_null_val = val_ptr::get_value(llvm::ConstantPointerNull::get(static_cast<llvm::PointerType *>(lhs_llvm_type)));
		emit_copy_assign<abi>(src_tokens, lhs_type, lhs_val, rhs_null_val, context);
	}
	else
	{
		emit_copy_assign<abi>(src_tokens, lhs_type, lhs_val, rhs_val, context);
	}

	if (result_address != nullptr)
	{
		return emit_copy_constructor<abi>(src_tokens, lhs_val, lhs_type, context, result_address);
	}
	else
	{
		return lhs_val;
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_default_move_assign(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_type = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_type = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);
	auto const is_rhs_null_pointer = lhs_type.is<ast::ts_pointer>() && rhs_type.is<ast::ts_base_type>();
	bz_assert(!is_rhs_null_pointer || rhs_type.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val.kind == val_ptr::reference);

	if (is_rhs_null_pointer)
	{
		auto const lhs_llvm_type = lhs_val.get_type();
		bz_assert(lhs_llvm_type->isPointerTy());
		auto const rhs_null_val = val_ptr::get_value(llvm::ConstantPointerNull::get(static_cast<llvm::PointerType *>(lhs_llvm_type)));
		emit_move_assign<abi>(src_tokens, lhs_type, lhs_val, rhs_null_val, context);
	}
	else
	{
		emit_move_assign<abi>(src_tokens, lhs_type, lhs_val, rhs_val, context);
	}

	if (result_address != nullptr)
	{
		return emit_copy_constructor<abi>(src_tokens, lhs_val, lhs_type, context, result_address);
	}
	else
	{
		return lhs_val;
	}
}

// ================================================================
// -------------------------- expression --------------------------
// ================================================================

template<abi::platform_abi abi, typename Context>
static llvm::Constant *get_value(
	ast::constant_value const &value,
	ast::typespec_view type,
	ast::constant_expression const *const_expr,
	Context &context
);

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_identifier const &id,
	ctx::bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const [ptr, type] = context.get_variable(id.decl);
	bz_assert(ptr != nullptr);
	if (result_address == nullptr)
	{
		return val_ptr::get_reference(ptr, type);
	}
	else
	{
		emit_copy_constructor<abi>(
			src_tokens,
			val_ptr::get_reference(ptr, type),
			ast::remove_const_or_consteval(ast::remove_lvalue_reference(id.decl->get_type())),
			context,
			result_address
		);
		return val_ptr::get_reference(result_address, type);
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_identifier const &id,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	bz_assert(id.decl != nullptr);
	// we emit consteval global variables to avoid generating huge arrays every time
	// one is indexed into.  e.g. ryu has large consteval tables which would be constructed
	// in IR each time they're indexed into.
	if (id.decl->is_global() && id.decl->get_type().is<ast::ts_consteval>() && id.decl->init_expr.not_error())
	{
		context.add_global_variable(id.decl);
	}
	auto const [ptr, type] = context.get_variable(id.decl);
	if (ptr == nullptr && (!id.decl->get_type().is<ast::ts_consteval>() || id.decl->init_expr.is_error()))
	{
		emit_error(
			{ id.id.tokens.begin, id.id.tokens.begin, id.id.tokens.end },
			bz::format("variable '{}' cannot be used in a constant expression", id.id.format_as_unqualified()),
			context
		);
		if (result_address == nullptr)
		{
			auto const result_type = get_llvm_type(ast::remove_lvalue_reference(id.decl->get_type()), context);
			auto const alloca = context.create_alloca(result_type);
			return val_ptr::get_reference(alloca, result_type);
		}
		else
		{
			auto const result_type = get_llvm_type(ast::remove_lvalue_reference(id.decl->get_type()), context);
			return val_ptr::get_reference(result_address, result_type);
		}
	}
	else if (ptr == nullptr /* && consteval */)
	{
		bz_assert(id.decl->init_expr.not_error() && id.decl->init_expr.is<ast::constant_expression>());
		auto &const_expr = id.decl->init_expr.get<ast::constant_expression>();
		auto const value = get_value<abi>(const_expr.value, const_expr.type, &const_expr, context);
		if (result_address == nullptr)
		{
			return val_ptr::get_value(value);
		}
		else
		{
			context.builder.CreateStore(value, result_address);
			return val_ptr::get_reference(result_address, type, value);
		}
	}
	else
	{
		if (result_address == nullptr)
		{
			return val_ptr::get_reference(ptr, type);
		}
		else
		{
			emit_copy_constructor<abi>(
				src_tokens,
				val_ptr::get_reference(ptr, type),
				ast::remove_lvalue_reference(id.decl->get_type()),
				context,
				result_address
			);
			return val_ptr::get_reference(result_address, type);
		}
	}
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_literal const &literal_expr,
	ctx::bitcode_context &context,
	llvm::Value *
)
{
	// can only be called with unreachable
	bz_assert(literal_expr.tokens.begin->kind == lex::token::kw_unreachable);
	if (no_panic_on_unreachable)
	{
		context.builder.CreateUnreachable();
	}
	else
	{
		auto const panic_fn = context.get_function(context.get_builtin_function(ast::function_body::builtin_panic));
		context.create_call(panic_fn);
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
	return val_ptr::get_none();
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_typed_literal const &,
	Context &,
	llvm::Value *
)
{
	// this is always a constant expression
	bz_unreachable;
}

template<abi::platform_abi abi>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_literal const &literal_expr,
	ctx::comptime_executor_context &context,
	llvm::Value *
)
{
	// can only be called with unreachable
	bz_assert(literal_expr.tokens.begin->kind == lex::token::kw_unreachable);
	emit_error(src_tokens, "'unreachable' hit in compile time execution", context);
	return val_ptr::get_none();
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_tuple const &tuple_expr,
	Context &context,
	llvm::Value *result_address
)
{
	auto const types = tuple_expr.elems
		.transform([](auto const &expr) { return expr.get_expr_type_and_kind().first; })
		.transform([&context](auto const ts) { return get_llvm_type(ts, context); })
		.template collect<ast::arena_vector>();
	auto const result_type = context.get_tuple_t(types);
	if (result_address == nullptr)
	{
		result_address = context.create_alloca(result_type);
	}

	for (unsigned i = 0; i < tuple_expr.elems.size(); ++i)
	{
		auto const elem_result_address = context.create_struct_gep(result_type, result_address, i);
		emit_bitcode<abi>(tuple_expr.elems[i], context, elem_result_address);
	}
	return val_ptr::get_reference(result_address, result_type);
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_unary_address_of(
	ast::expression const &expr,
	ctx::bitcode_context &context,
	llvm::Value *result_address
)
{
	auto const val = emit_bitcode<abi>(expr, context, nullptr);
	bz_assert(val.kind == val_ptr::reference);
	if (result_address == nullptr)
	{
		return val_ptr::get_value(val.val);
	}
	else
	{
		auto const ptr_type = llvm::PointerType::get(val.get_type(), 0);
		context.builder.CreateStore(val.val, result_address);
		return val_ptr::get_reference(result_address, ptr_type);
	}
}

template<abi::platform_abi abi>
static val_ptr emit_builtin_unary_address_of(
	ast::expression const &expr,
	ctx::comptime_executor_context &context,
	llvm::Value *result_address
)
{
	auto const val = emit_bitcode<abi>(expr, context, nullptr);
	if (val.kind != val_ptr::reference)
	{
		if (auto const id_expr = expr.get_expr().get_if<ast::expr_identifier>(); id_expr && id_expr->decl != nullptr)
		{
			emit_error(
				expr.src_tokens,
				bz::format("unable to take address of variable '{}'", id_expr->decl->get_id().format_as_unqualified()),
				context
			);
		}
		else
		{
			emit_error(expr.src_tokens, "unable to take address of value", context);
		}
		// just make sure the returned value is valid
		auto const ptr_type = llvm::PointerType::get(val.get_type(), 0);
		if (result_address == nullptr)
		{
			return val_ptr::get_value(llvm::Constant::getNullValue(ptr_type));
		}
		else
		{
			return val_ptr::get_reference(result_address, ptr_type);
		}
	}
	else
	{
		if (result_address == nullptr)
		{
			return val_ptr::get_value(val.val);
		}
		else
		{
			auto const ptr_type = llvm::PointerType::get(val.get_type(), 0);
			context.builder.CreateStore(val.val, result_address);
			return val_ptr::get_reference(result_address, ptr_type);
		}
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_unary_plus(
	ast::expression const &expr,
	Context &context,
	llvm::Value *result_address
)
{
	return emit_bitcode<abi>(expr, context, result_address);
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_unary_minus(
	ast::expression const &expr,
	Context &context,
	llvm::Value *result_address
)
{
	auto const expr_t = ast::remove_const_or_consteval(expr.get_expr_type_and_kind().first);
	bz_assert(expr_t.is<ast::ts_base_type>());
	auto const expr_kind = expr_t.get<ast::ts_base_type>().info->kind;
	auto const [val, type] = emit_bitcode<abi>(expr, context, nullptr).get_value_and_type(context.builder);
	auto const res = ast::is_floating_point_kind(expr_kind)
		? context.builder.CreateFNeg(val, "unary_minus_tmp")
		: context.builder.CreateNeg(val, "unary_minus_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(res);
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return val_ptr::get_reference(result_address, type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_unary_dereference(
	lex::src_tokens const &src_tokens,
	ast::expression const &expr,
	Context &context,
	llvm::Value *result_address
)
{
	auto const val = emit_bitcode<abi>(expr, context, nullptr).get_value(context.builder);
	auto const type = ast::remove_const_or_consteval(expr.get_expr_type_and_kind().first);
	bz_assert(type.template is<ast::ts_pointer>());
	auto const result_type = get_llvm_type(type.template get<ast::ts_pointer>(), context);
	if (result_address == nullptr)
	{
		return val_ptr::get_reference(val, result_type);
	}
	else
	{
		emit_copy_constructor<abi>(
			src_tokens,
			val_ptr::get_reference(val, result_type),
			ast::remove_const_or_consteval(expr.get_expr_type_and_kind().first),
			context,
			result_address
		);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_unary_bit_not(
	ast::expression const &expr,
	Context &context,
	llvm::Value *result_address
)
{
	auto const [val, type] = emit_bitcode<abi>(expr, context, nullptr).get_value_and_type(context.builder);
	auto const res = context.builder.CreateNot(val, "unary_bit_not_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(res);
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return val_ptr::get_reference(result_address, type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_unary_bool_not(
	ast::expression const &expr,
	Context &context,
	llvm::Value *result_address
)
{
	auto const [val, type] = emit_bitcode<abi>(expr, context, nullptr).get_value_and_type(context.builder);
	auto const res = context.builder.CreateNot(val, "unary_bool_not_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(res);
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return val_ptr::get_reference(result_address, type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_unary_plus_plus(
	ast::expression const &expr,
	Context &context,
	llvm::Value *result_address
)
{
	auto const val = emit_bitcode<abi>(expr, context, nullptr);
	bz_assert(val.kind == val_ptr::reference);
	auto const [original_value, type] = val.get_value_and_type(context.builder);
	if (type->isPointerTy())
	{
		auto const expr_type = expr.get_expr_type_and_kind().first;
		bz_assert(expr_type.is<ast::ts_pointer>());
		auto const inner_type = get_llvm_type(expr_type.get<ast::ts_pointer>(), context);
		auto const incremented_value = context.create_gep(inner_type, original_value, 1);
		context.builder.CreateStore(incremented_value, val.val);
		if (result_address == nullptr)
		{
			return val;
		}
		else
		{
			context.builder.CreateStore(incremented_value, result_address);
			return val_ptr::get_reference(result_address, type);
		}
	}
	else
	{
		bz_assert(type->isIntegerTy());
		auto const incremented_value = context.builder.CreateAdd(
			original_value,
			llvm::ConstantInt::get(type, 1)
		);
		context.builder.CreateStore(incremented_value, val.val);
		if (result_address == nullptr)
		{
			return val;
		}
		else
		{
			context.builder.CreateStore(incremented_value, result_address);
			return val_ptr::get_reference(result_address, type);
		}
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_unary_minus_minus(
	ast::expression const &expr,
	Context &context,
	llvm::Value *result_address
)
{
	auto const val = emit_bitcode<abi>(expr, context, nullptr);
	bz_assert(val.kind == val_ptr::reference);
	auto const [original_value, type] = val.get_value_and_type(context.builder);
	if (type->isPointerTy())
	{
		auto const expr_type = expr.get_expr_type_and_kind().first;
		bz_assert(expr_type.is<ast::ts_pointer>());
		auto const inner_type = get_llvm_type(expr_type.get<ast::ts_pointer>(), context);
		auto const incremented_value = context.create_gep(inner_type, original_value, uint64_t(-1));
		context.builder.CreateStore(incremented_value, val.val);
		if (result_address == nullptr)
		{
			return val;
		}
		else
		{
			context.builder.CreateStore(incremented_value, result_address);
			return val_ptr::get_reference(result_address, type);
		}
	}
	else
	{
		bz_assert(type->isIntegerTy());
		auto const incremented_value = context.builder.CreateAdd(
			original_value,
			llvm::ConstantInt::get(type, uint64_t(-1))
		);
		context.builder.CreateStore(incremented_value, val.val);
		if (result_address == nullptr)
		{
			return val;
		}
		else
		{
			context.builder.CreateStore(incremented_value, result_address);
			return val_ptr::get_reference(result_address, type);
		}
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_unary_op const &unary_op,
	Context &context,
	llvm::Value *result_address
)
{
	switch (unary_op.op)
	{
	// ==== non-overloadable ====
	case lex::token::address_of:         // '&'
		return emit_builtin_unary_address_of<abi>(unary_op.expr, context, result_address);
	case lex::token::kw_sizeof:          // 'sizeof'
		// this is always a constant expression
		bz_unreachable;
	case lex::token::kw_move:
		bz_assert(result_address == nullptr);
		return emit_bitcode<abi>(unary_op.expr, context, result_address);

	// overloadables are handled as function calls
	default:
		bz_unreachable;
		return val_ptr::get_none();
	}
}


template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_assign(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	if (ast::is_lvalue(rhs.get_expr_type_and_kind().second))
	{
		return emit_default_copy_assign<abi>(src_tokens, lhs, rhs, context, result_address);
	}
	else
	{
		return emit_default_move_assign<abi>(src_tokens, lhs, rhs, context, result_address);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_plus(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind))
		{
			auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
			auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const result_val = ast::is_floating_point_kind(lhs_kind)
				? context.builder.CreateFAdd(lhs_val, rhs_val, "add_tmp")
				: context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			if (result_address == nullptr)
			{
				return val_ptr::get_value(result_val);
			}
			else
			{
				auto const result_type = result_val->getType();
				context.builder.CreateStore(result_val, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
		}
		else if (lhs_kind == ast::type_info::char_)
		{
			auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
			auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_uint32_t(),
				ast::is_signed_integer_kind(rhs_kind)
			);
			auto const result_val = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			if (result_address == nullptr)
			{
				return val_ptr::get_value(result_val);
			}
			else
			{
				auto const result_type = result_val->getType();
				context.builder.CreateStore(result_val, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
		}
		else // if (rhs_kind == ast::type_info::char_)
		{
			bz_assert(rhs_kind == ast::type_info::char_);
			auto lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
			lhs_val = context.builder.CreateIntCast(
				lhs_val,
				context.get_uint32_t(),
				ast::is_signed_integer_kind(lhs_kind)
			);
			auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const result_val = context.builder.CreateAdd(lhs_val, rhs_val, "add_tmp");
			if (result_address == nullptr)
			{
				return val_ptr::get_value(result_val);
			}
			else
			{
				auto const result_type = result_val->getType();
				context.builder.CreateStore(result_val, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
		}
	}
	else if (lhs_t.is<ast::ts_pointer>())
	{
		bz_assert(rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to usize, otherwise big values might count as a negative index
		if (ast::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_usize_t(), false);
		}
		auto const lhs_inner_type = get_llvm_type(lhs_t.get<ast::ts_pointer>(), context);
		auto const result_val = context.create_gep(lhs_inner_type, lhs_val, rhs_val, "ptr_add_tmp");
		if (result_address == nullptr)
		{
			return val_ptr::get_value(result_val);
		}
		else
		{
			auto const result_type = result_val->getType();
			context.builder.CreateStore(result_val, result_address);
			return val_ptr::get_reference(result_address, result_type);
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_pointer>());
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to usize, otherwise big values might count as a negative index
		if (ast::is_unsigned_integer_kind(lhs_kind))
		{
			lhs_val = context.builder.CreateIntCast(lhs_val, context.get_usize_t(), false);
		}
		auto const rhs_inner_type = get_llvm_type(rhs_t.get<ast::ts_pointer>(), context);
		auto const result_val = context.create_gep(rhs_inner_type, rhs_val, lhs_val, "ptr_add_tmp");
		if (result_address == nullptr)
		{
			return val_ptr::get_value(result_val);
		}
		else
		{
			auto const result_type = result_val->getType();
			context.builder.CreateStore(result_val, result_address);
			return val_ptr::get_reference(result_address, result_type);
		}
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_plus_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind))
		{
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
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
			if (result_address == nullptr)
			{
				return lhs_val_ref;
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
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
				ast::is_signed_integer_kind(rhs_kind)
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
				return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
			}
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		// we calculate the right hand side first
		auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to usize, otherwise big values might count as a negative index
		if (ast::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_usize_t(), false);
		}
		auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
		bz_assert(lhs_val_ref.kind == val_ptr::reference);
		auto const lhs_val = lhs_val_ref.get_value(context.builder);
		auto const lhs_inner_type = get_llvm_type(lhs_t.get<ast::ts_pointer>(), context);
		auto const res = context.create_gep(lhs_inner_type, lhs_val, rhs_val, "ptr_add_tmp");
		context.builder.CreateStore(res, lhs_val_ref.val);
		if (result_address == nullptr)
		{
			return lhs_val_ref;
		}
		else
		{
			context.builder.CreateStore(res, result_address);
			return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
		}
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_minus(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind))
		{
			auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
			auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const result_val = ast::is_floating_point_kind(lhs_kind)
				? context.builder.CreateFSub(lhs_val, rhs_val, "sub_tmp")
				: context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			if (result_address == nullptr)
			{
				return val_ptr::get_value(result_val);
			}
			else
			{
				auto const result_type = result_val->getType();
				context.builder.CreateStore(result_val, result_address);
				return val_ptr::get_reference(result_address, result_type);
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
				return val_ptr::get_value(result_val);
			}
			else
			{
				auto const result_type = result_val->getType();
				context.builder.CreateStore(result_val, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
		}
		else
		{
			bz_assert(
				lhs_kind == ast::type_info::char_
				&& ast::is_integer_kind(rhs_kind)
			);
			auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
			auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			rhs_val = context.builder.CreateIntCast(
				rhs_val,
				context.get_int32_t(),
				ast::is_signed_integer_kind(rhs_kind)
			);
			auto const result_val = context.builder.CreateSub(lhs_val, rhs_val, "sub_tmp");
			if (result_address == nullptr)
			{
				return val_ptr::get_value(result_val);
			}
			else
			{
				auto const result_type = result_val->getType();
				context.builder.CreateStore(result_val, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
		}
	}
	else if (rhs_t.is<ast::ts_base_type>())
	{
		bz_assert(lhs_t.is<ast::ts_pointer>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to usize, otherwise big values might count as a negative index
		if (ast::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_usize_t(), false);
		}
		// negate rhs_val
		rhs_val = context.builder.CreateNeg(rhs_val);
		auto const lhs_inner_type = get_llvm_type(lhs_t.get<ast::ts_pointer>(), context);
		auto const result_val = context.create_gep(lhs_inner_type, lhs_val, rhs_val, "ptr_sub_tmp");
		if (result_address == nullptr)
		{
			return val_ptr::get_value(result_val);
		}
		else
		{
			auto const result_type = result_val->getType();
			context.builder.CreateStore(result_val, result_address);
			return val_ptr::get_reference(result_address, result_type);
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_pointer>());
		auto const elem_type = get_llvm_type(ast::remove_const_or_consteval(lhs_t.get<ast::ts_pointer>()), context);
		auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		auto const result_val = context.builder.CreatePtrDiff(elem_type, lhs_val, rhs_val, "ptr_diff_tmp");
		if (result_address == nullptr)
		{
			return val_ptr::get_value(result_val);
		}
		else
		{
			auto const result_type = result_val->getType();
			context.builder.CreateStore(result_val, result_address);
			return val_ptr::get_reference(result_address, result_type);
		}
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_minus_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		if (ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind))
		{
			// we calculate the right hand side first
			auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
			auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
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
			if (result_address == nullptr)
			{
				return lhs_val_ref;
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
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
				ast::is_signed_integer_kind(rhs_kind)
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
				return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
			}
		}
	}
	else
	{
		bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_base_type>());
		auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
		// we calculate the right hand side first
		auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		// we need to cast unsigned integers to usize, otherwise big values might count as a negative index
		if (ast::is_unsigned_integer_kind(rhs_kind))
		{
			rhs_val = context.builder.CreateIntCast(rhs_val, context.get_usize_t(), false);
		}
		// negate rhs_val
		rhs_val = context.builder.CreateNeg(rhs_val);
		auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
		bz_assert(lhs_val_ref.kind == val_ptr::reference);
		auto const lhs_val = lhs_val_ref.get_value(context.builder);
		auto const lhs_inner_type = get_llvm_type(lhs_t.get<ast::ts_pointer>(), context);
		auto const res = context.create_gep(lhs_inner_type, lhs_val, rhs_val, "ptr_sub_tmp");
		context.builder.CreateStore(res, lhs_val_ref.val);
		if (result_address == nullptr)
		{
			return lhs_val_ref;
		}
		else
		{
			context.builder.CreateStore(res, result_address);
			return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
		}
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_multiply(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind));
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = ast::is_floating_point_kind(lhs_kind)
		? context.builder.CreateFMul(lhs_val, rhs_val, "mul_tmp")
		: context.builder.CreateMul(lhs_val, rhs_val, "mul_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(result_val);
	}
	else
	{
		auto const result_type = result_val->getType();
		context.builder.CreateStore(result_val, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_multiply_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind));
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);
	auto const res = ast::is_integer_kind(lhs_kind)
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
		return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_divide(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(lhs_kind == rhs_t.get<ast::ts_base_type>().info->kind);
	bz_assert(ast::is_arithmetic_kind(lhs_kind));
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);

	if constexpr (is_comptime<Context>)
	{
		if (context.do_error_checking() && ast::is_integer_kind(lhs_kind))
		{
			auto const check_fn_kind =
				lhs_kind == ast::type_info::int8_   ? ctx::comptime_function_kind::i8_divide_check :
				lhs_kind == ast::type_info::int16_  ? ctx::comptime_function_kind::i16_divide_check :
				lhs_kind == ast::type_info::int32_  ? ctx::comptime_function_kind::i32_divide_check :
				lhs_kind == ast::type_info::int64_  ? ctx::comptime_function_kind::i64_divide_check :
				lhs_kind == ast::type_info::uint8_  ? ctx::comptime_function_kind::u8_divide_check :
				lhs_kind == ast::type_info::uint16_ ? ctx::comptime_function_kind::u16_divide_check :
				lhs_kind == ast::type_info::uint32_ ? ctx::comptime_function_kind::u32_divide_check :
				lhs_kind == ast::type_info::uint64_ ? ctx::comptime_function_kind::u64_divide_check
				: (bz_unreachable, ctx::comptime_function_kind::u64_divide_check);
			auto const [src_begin_val, src_pivot_val, src_end_val] = get_src_tokens_llvm_value(src_tokens, context);
			auto const is_valid = context.create_call(
				context.get_comptime_function(check_fn_kind),
				{ lhs_val, rhs_val, src_begin_val, src_pivot_val, src_end_val }
			);
			emit_error_assert(is_valid, context);
		}
	}

	auto const result_val = ast::is_signed_integer_kind(lhs_kind) ? context.builder.CreateSDiv(lhs_val, rhs_val, "div_tmp")
		: ast::is_unsigned_integer_kind(lhs_kind) ? context.builder.CreateUDiv(lhs_val, rhs_val, "div_tmp")
		: context.builder.CreateFDiv(lhs_val, rhs_val, "div_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(result_val);
	}
	else
	{
		auto const result_type = result_val->getType();
		context.builder.CreateStore(result_val, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_divide_eq(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_arithmetic_kind(lhs_kind) && ast::is_arithmetic_kind(rhs_kind));
	// we calculate the right hand side first
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);

	if constexpr (is_comptime<Context>)
	{
		if (context.do_error_checking() && ast::is_integer_kind(lhs_kind))
		{
			auto const check_fn_kind =
				lhs_kind == ast::type_info::int8_   ? ctx::comptime_function_kind::i8_divide_check :
				lhs_kind == ast::type_info::int16_  ? ctx::comptime_function_kind::i16_divide_check :
				lhs_kind == ast::type_info::int32_  ? ctx::comptime_function_kind::i32_divide_check :
				lhs_kind == ast::type_info::int64_  ? ctx::comptime_function_kind::i64_divide_check :
				lhs_kind == ast::type_info::uint8_  ? ctx::comptime_function_kind::u8_divide_check :
				lhs_kind == ast::type_info::uint16_ ? ctx::comptime_function_kind::u16_divide_check :
				lhs_kind == ast::type_info::uint32_ ? ctx::comptime_function_kind::u32_divide_check :
				lhs_kind == ast::type_info::uint64_ ? ctx::comptime_function_kind::u64_divide_check
				: (bz_unreachable, ctx::comptime_function_kind::u64_divide_check);
			auto const [src_begin_val, src_pivot_val, src_end_val] = get_src_tokens_llvm_value(src_tokens, context);
			auto const is_valid = context.create_call(
				context.get_comptime_function(check_fn_kind),
				{ lhs_val, rhs_val, src_begin_val, src_pivot_val, src_end_val }
			);
			emit_error_assert(is_valid, context);
		}
	}

	auto const res = ast::is_signed_integer_kind(lhs_kind) ? context.builder.CreateSDiv(lhs_val, rhs_val, "div_tmp")
		: ast::is_unsigned_integer_kind(lhs_kind) ? context.builder.CreateUDiv(lhs_val, rhs_val, "div_tmp")
		: context.builder.CreateFDiv(lhs_val, rhs_val, "div_tmp");
	context.builder.CreateStore(res, lhs_val_ref.val);
	if (result_address == nullptr)
	{
		return lhs_val_ref;
	}
	else
	{
		context.builder.CreateStore(res, result_address);
		return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_modulo(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_integer_kind(lhs_kind) && ast::is_integer_kind(rhs_kind));
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);

	if constexpr (is_comptime<Context>)
	{
		if (context.do_error_checking() && ast::is_integer_kind(lhs_kind))
		{
			auto const check_fn_kind =
				lhs_kind == ast::type_info::int8_   ? ctx::comptime_function_kind::i8_modulo_check :
				lhs_kind == ast::type_info::int16_  ? ctx::comptime_function_kind::i16_modulo_check :
				lhs_kind == ast::type_info::int32_  ? ctx::comptime_function_kind::i32_modulo_check :
				lhs_kind == ast::type_info::int64_  ? ctx::comptime_function_kind::i64_modulo_check :
				lhs_kind == ast::type_info::uint8_  ? ctx::comptime_function_kind::u8_modulo_check :
				lhs_kind == ast::type_info::uint16_ ? ctx::comptime_function_kind::u16_modulo_check :
				lhs_kind == ast::type_info::uint32_ ? ctx::comptime_function_kind::u32_modulo_check :
				lhs_kind == ast::type_info::uint64_ ? ctx::comptime_function_kind::u64_modulo_check
				: (bz_unreachable, ctx::comptime_function_kind::u64_modulo_check);
			auto const [src_begin_val, src_pivot_val, src_end_val] = get_src_tokens_llvm_value(src_tokens, context);
			auto const is_valid = context.create_call(
				context.get_comptime_function(check_fn_kind),
				{ lhs_val, rhs_val, src_begin_val, src_pivot_val, src_end_val }
			);
			emit_error_assert(is_valid, context);
		}
	}

	auto const result_val = ast::is_signed_integer_kind(lhs_kind)
		? context.builder.CreateSRem(lhs_val, rhs_val, "mod_tmp")
		: context.builder.CreateURem(lhs_val, rhs_val, "mod_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(result_val);
	}
	else
	{
		auto const result_type = result_val->getType();
		context.builder.CreateStore(result_val, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_modulo_eq(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_integer_kind(lhs_kind) && ast::is_integer_kind(rhs_kind));
	// we calculate the right hand side first
	auto rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const lhs_val_ref = emit_bitcode<abi>(lhs, context, nullptr);
	bz_assert(lhs_val_ref.kind == val_ptr::reference);
	auto const lhs_val = lhs_val_ref.get_value(context.builder);

	if constexpr (is_comptime<Context>)
	{
		if (context.do_error_checking() && ast::is_integer_kind(lhs_kind))
		{
			auto const check_fn_kind =
				lhs_kind == ast::type_info::int8_   ? ctx::comptime_function_kind::i8_modulo_check :
				lhs_kind == ast::type_info::int16_  ? ctx::comptime_function_kind::i16_modulo_check :
				lhs_kind == ast::type_info::int32_  ? ctx::comptime_function_kind::i32_modulo_check :
				lhs_kind == ast::type_info::int64_  ? ctx::comptime_function_kind::i64_modulo_check :
				lhs_kind == ast::type_info::uint8_  ? ctx::comptime_function_kind::u8_modulo_check :
				lhs_kind == ast::type_info::uint16_ ? ctx::comptime_function_kind::u16_modulo_check :
				lhs_kind == ast::type_info::uint32_ ? ctx::comptime_function_kind::u32_modulo_check :
				lhs_kind == ast::type_info::uint64_ ? ctx::comptime_function_kind::u64_modulo_check
				: (bz_unreachable, ctx::comptime_function_kind::u64_modulo_check);
			auto const [src_begin_val, src_pivot_val, src_end_val] = get_src_tokens_llvm_value(src_tokens, context);
			auto const is_valid = context.create_call(
				context.get_comptime_function(check_fn_kind),
				{ lhs_val, rhs_val, src_begin_val, src_pivot_val, src_end_val }
			);
			emit_error_assert(is_valid, context);
		}
	}

	auto const res = ast::is_signed_integer_kind(lhs_kind)
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
		return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_cmp(
	uint32_t op,
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
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

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
		auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
		auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
		bz_assert(lhs_kind != ast::type_info::str_);
		auto const pred = ast::is_floating_point_kind(lhs_kind) ? get_cmp_predicate(2)
			: ast::is_signed_integer_kind(lhs_kind) ? get_cmp_predicate(0)
			: get_cmp_predicate(1);
		auto const result_val = ast::is_floating_point_kind(lhs_kind)
			? context.builder.CreateFCmp(pred, lhs_val, rhs_val)
			: context.builder.CreateICmp(pred, lhs_val, rhs_val);
		if (result_address == nullptr)
		{
			return val_ptr::get_value(result_val);
		}
		else
		{
			auto const result_type = result_val->getType();
			context.builder.CreateStore(result_val, result_address);
			return val_ptr::get_reference(result_address, result_type);
		}
	}
	else // if pointer
	{
		auto const [lhs_val, rhs_val] = [&]() -> std::pair<llvm::Value *, llvm::Value *> {
			if (lhs_t.is<ast::ts_base_type>())
			{
				auto const rhs_ptr_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
				auto const rhs_val = context.builder.CreatePtrToInt(rhs_ptr_val, context.get_usize_t());
				auto const lhs_val = llvm::ConstantInt::get(rhs_val->getType(), 0);
				return { lhs_val, rhs_val };
			}
			else if (rhs_t.is<ast::ts_base_type>())
			{
				auto const lhs_ptr_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
				auto const lhs_val = context.builder.CreatePtrToInt(lhs_ptr_val, context.get_usize_t());
				auto const rhs_val = llvm::ConstantInt::get(lhs_val->getType(), 0);
				return { lhs_val, rhs_val };
			}
			else
			{
				bz_assert(lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_pointer>());
				auto const lhs_ptr_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
				auto const rhs_ptr_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
				auto const lhs_val = context.builder.CreatePtrToInt(lhs_ptr_val, context.get_usize_t());
				auto const rhs_val = context.builder.CreatePtrToInt(rhs_ptr_val, context.get_usize_t());
				return { lhs_val, rhs_val };
			}
		}();
		auto const p = get_cmp_predicate(1); // unsigned compare
		auto const result_val = context.builder.CreateICmp(p, lhs_val, rhs_val, "cmp_tmp");
		if (result_address == nullptr)
		{
			return val_ptr::get_value(result_val);
		}
		else
		{
			auto const result_type = result_val->getType();
			context.builder.CreateStore(result_val, result_address);
			return val_ptr::get_reference(result_address, result_type);
		}
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_bit_and(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ast::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateAnd(lhs_val, rhs_val, "bit_and_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(result_val);
	}
	else
	{
		auto const result_type = result_val->getType();
		context.builder.CreateStore(result_val, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_bit_and_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ast::is_unsigned_integer_kind(lhs_kind)
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
		return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_bit_xor(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ast::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateXor(lhs_val, rhs_val, "bit_xor_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(result_val);
	}
	else
	{
		auto const result_type = result_val->getType();
		context.builder.CreateStore(result_val, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_bit_xor_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ast::is_unsigned_integer_kind(lhs_kind)
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
		return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_bit_or(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ast::is_unsigned_integer_kind(lhs_kind)
		|| lhs_kind == ast::type_info::bool_)
		&& lhs_kind == rhs_kind
	);
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const result_val = context.builder.CreateOr(lhs_val, rhs_val, "bit_or_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(result_val);
	}
	else
	{
		auto const result_type = result_val->getType();
		context.builder.CreateStore(result_val, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_bit_or_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(
		(ast::is_unsigned_integer_kind(lhs_kind)
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
		return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_left_shift(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_unsigned_integer_kind(lhs_kind) && ast::is_integer_kind(rhs_kind));
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const cast_rhs_val = context.builder.CreateIntCast(rhs_val, context.get_builtin_type(lhs_kind), false);
	auto const result_val = context.builder.CreateShl(lhs_val, cast_rhs_val, "lshift_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(result_val);
	}
	else
	{
		auto const result_type = result_val->getType();
		context.builder.CreateStore(result_val, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_left_shift_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_unsigned_integer_kind(lhs_kind) && ast::is_integer_kind(rhs_kind));
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
		return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_right_shift(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_unsigned_integer_kind(lhs_kind) && ast::is_integer_kind(rhs_kind));
	auto const lhs_val = emit_bitcode<abi>(lhs, context, nullptr).get_value(context.builder);
	auto const rhs_val = emit_bitcode<abi>(rhs, context, nullptr).get_value(context.builder);
	auto const cast_rhs_val = context.builder.CreateIntCast(rhs_val, context.get_builtin_type(lhs_kind), false);
	auto const result_val = context.builder.CreateLShr(lhs_val, cast_rhs_val, "rshift_tmp");
	if (result_address == nullptr)
	{
		return val_ptr::get_value(result_val);
	}
	else
	{
		auto const result_type = result_val->getType();
		context.builder.CreateStore(result_val, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_right_shift_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	Context &context,
	llvm::Value *result_address
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);

	bz_assert(lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>());
	auto const lhs_kind = lhs_t.get<ast::ts_base_type>().info->kind;
	auto const rhs_kind = rhs_t.get<ast::ts_base_type>().info->kind;
	bz_assert(ast::is_unsigned_integer_kind(lhs_kind) && ast::is_integer_kind(rhs_kind));
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
		return val_ptr::get_reference(result_address, lhs_val_ref.get_type());
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_bool_and(
	ast::expr_binary_op const &binary_op,
	Context &context,
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
		return val_ptr::get_value(phi);
	}
	else
	{
		auto const result_type = phi->getType();
		context.builder.CreateStore(phi, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_bool_xor(
	ast::expr_binary_op const &binary_op,
	Context &context,
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
		return val_ptr::get_value(result_val);
	}
	else
	{
		auto const result_type = result_val->getType();
		context.builder.CreateStore(result_val, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_builtin_binary_bool_or(
	ast::expr_binary_op const &binary_op,
	Context &context,
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
		return val_ptr::get_value(phi);
	}
	else
	{
		auto const result_type = phi->getType();
		context.builder.CreateStore(phi, result_address);
		return val_ptr::get_reference(result_address, result_type);
	}
}


template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_binary_op const &binary_op,
	Context &context,
	llvm::Value *result_address
)
{
	switch (binary_op.op)
	{
	// ==== non-overloadable ====
	case lex::token::comma:              // ','
		emit_bitcode<abi>(binary_op.lhs, context, nullptr);
		return emit_bitcode<abi>(binary_op.rhs, context, result_address);
	case lex::token::bool_and:           // '&&'
		return emit_builtin_binary_bool_and<abi>(binary_op, context, result_address);
	case lex::token::bool_xor:           // '^^'
		return emit_builtin_binary_bool_xor<abi>(binary_op, context, result_address);
	case lex::token::bool_or:            // '||'
		return emit_builtin_binary_bool_or<abi>(binary_op, context, result_address);

	// ==== overloadable ====
	// they are handled as intrinsic functions

	// these have no built-in operations
	case lex::token::dot_dot:            // '..'
	case lex::token::dot_dot_eq:         // '..='
	default:
		bz_unreachable;
		return val_ptr::get_none();
	}
}

static ctx::comptime_function_kind get_math_check_function_kind(uint32_t intrinsic_kind)
{
	switch (intrinsic_kind)
	{
	case ast::function_body::exp_f32:
		return ctx::comptime_function_kind::exp_f32_check;
	case ast::function_body::exp_f64:
		return ctx::comptime_function_kind::exp_f64_check;
	case ast::function_body::exp2_f32:
		return ctx::comptime_function_kind::exp2_f32_check;
	case ast::function_body::exp2_f64:
		return ctx::comptime_function_kind::exp2_f64_check;
	case ast::function_body::expm1_f32:
		return ctx::comptime_function_kind::expm1_f32_check;
	case ast::function_body::expm1_f64:
		return ctx::comptime_function_kind::expm1_f64_check;
	case ast::function_body::log_f32:
		return ctx::comptime_function_kind::log_f32_check;
	case ast::function_body::log_f64:
		return ctx::comptime_function_kind::log_f64_check;
	case ast::function_body::log10_f32:
		return ctx::comptime_function_kind::log10_f32_check;
	case ast::function_body::log10_f64:
		return ctx::comptime_function_kind::log10_f64_check;
	case ast::function_body::log2_f32:
		return ctx::comptime_function_kind::log2_f32_check;
	case ast::function_body::log2_f64:
		return ctx::comptime_function_kind::log2_f64_check;
	case ast::function_body::log1p_f32:
		return ctx::comptime_function_kind::log1p_f32_check;
	case ast::function_body::log1p_f64:
		return ctx::comptime_function_kind::log1p_f64_check;
	case ast::function_body::sqrt_f32:
		return ctx::comptime_function_kind::sqrt_f32_check;
	case ast::function_body::sqrt_f64:
		return ctx::comptime_function_kind::sqrt_f64_check;
	case ast::function_body::pow_f32:
		return ctx::comptime_function_kind::pow_f32_check;
	case ast::function_body::pow_f64:
		return ctx::comptime_function_kind::pow_f64_check;
	case ast::function_body::cbrt_f32:
		return ctx::comptime_function_kind::cbrt_f32_check;
	case ast::function_body::cbrt_f64:
		return ctx::comptime_function_kind::cbrt_f64_check;
	case ast::function_body::hypot_f32:
		return ctx::comptime_function_kind::hypot_f32_check;
	case ast::function_body::hypot_f64:
		return ctx::comptime_function_kind::hypot_f64_check;
	case ast::function_body::sin_f32:
		return ctx::comptime_function_kind::sin_f32_check;
	case ast::function_body::sin_f64:
		return ctx::comptime_function_kind::sin_f64_check;
	case ast::function_body::cos_f32:
		return ctx::comptime_function_kind::cos_f32_check;
	case ast::function_body::cos_f64:
		return ctx::comptime_function_kind::cos_f64_check;
	case ast::function_body::tan_f32:
		return ctx::comptime_function_kind::tan_f32_check;
	case ast::function_body::tan_f64:
		return ctx::comptime_function_kind::tan_f64_check;
	case ast::function_body::asin_f32:
		return ctx::comptime_function_kind::asin_f32_check;
	case ast::function_body::asin_f64:
		return ctx::comptime_function_kind::asin_f64_check;
	case ast::function_body::acos_f32:
		return ctx::comptime_function_kind::acos_f32_check;
	case ast::function_body::acos_f64:
		return ctx::comptime_function_kind::acos_f64_check;
	case ast::function_body::atan_f32:
		return ctx::comptime_function_kind::atan_f32_check;
	case ast::function_body::atan_f64:
		return ctx::comptime_function_kind::atan_f64_check;
	case ast::function_body::atan2_f32:
		return ctx::comptime_function_kind::atan2_f32_check;
	case ast::function_body::atan2_f64:
		return ctx::comptime_function_kind::atan2_f64_check;
	case ast::function_body::sinh_f32:
		return ctx::comptime_function_kind::sinh_f32_check;
	case ast::function_body::sinh_f64:
		return ctx::comptime_function_kind::sinh_f64_check;
	case ast::function_body::cosh_f32:
		return ctx::comptime_function_kind::cosh_f32_check;
	case ast::function_body::cosh_f64:
		return ctx::comptime_function_kind::cosh_f64_check;
	case ast::function_body::tanh_f32:
		return ctx::comptime_function_kind::tanh_f32_check;
	case ast::function_body::tanh_f64:
		return ctx::comptime_function_kind::tanh_f64_check;
	case ast::function_body::asinh_f32:
		return ctx::comptime_function_kind::asinh_f32_check;
	case ast::function_body::asinh_f64:
		return ctx::comptime_function_kind::asinh_f64_check;
	case ast::function_body::acosh_f32:
		return ctx::comptime_function_kind::acosh_f32_check;
	case ast::function_body::acosh_f64:
		return ctx::comptime_function_kind::acosh_f64_check;
	case ast::function_body::atanh_f32:
		return ctx::comptime_function_kind::atanh_f32_check;
	case ast::function_body::atanh_f64:
		return ctx::comptime_function_kind::atanh_f64_check;
	case ast::function_body::erf_f32:
		return ctx::comptime_function_kind::erf_f32_check;
	case ast::function_body::erf_f64:
		return ctx::comptime_function_kind::erf_f64_check;
	case ast::function_body::erfc_f32:
		return ctx::comptime_function_kind::erfc_f32_check;
	case ast::function_body::erfc_f64:
		return ctx::comptime_function_kind::erfc_f64_check;
	case ast::function_body::tgamma_f32:
		return ctx::comptime_function_kind::tgamma_f32_check;
	case ast::function_body::tgamma_f64:
		return ctx::comptime_function_kind::tgamma_f64_check;
	case ast::function_body::lgamma_f32:
		return ctx::comptime_function_kind::lgamma_f32_check;
	case ast::function_body::lgamma_f64:
		return ctx::comptime_function_kind::lgamma_f64_check;
	default:
		bz_unreachable;
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_function_call const &func_call,
	Context &context,
	llvm::Value *result_address
)
{
	if constexpr (!is_comptime<Context>)
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
	}

	if (func_call.func_body->is_intrinsic() && func_call.func_body->body.is_null())
	{
		switch (func_call.func_body->intrinsic_kind)
		{
		static_assert(ast::function_body::_builtin_last - ast::function_body::_builtin_first == 139);
		static_assert(ast::function_body::_builtin_default_constructor_last - ast::function_body::_builtin_default_constructor_first == 14);
		static_assert(ast::function_body::_builtin_unary_operator_last - ast::function_body::_builtin_unary_operator_first == 7);
		static_assert(ast::function_body::_builtin_binary_operator_last - ast::function_body::_builtin_binary_operator_first == 27);
		case ast::function_body::builtin_str_begin_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const arg = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const begin_ptr = context.builder.CreateExtractValue(arg, 0);
			if (result_address != nullptr)
			{
				auto const result_type = begin_ptr->getType();
				context.builder.CreateStore(begin_ptr, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
			else
			{
				return val_ptr::get_value(begin_ptr);
			}
		}
		case ast::function_body::builtin_str_end_ptr:
		{
			bz_assert(func_call.params.size() == 1);
			auto const arg = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const end_ptr = context.builder.CreateExtractValue(arg, 1);
			if (result_address != nullptr)
			{
				auto const result_type = end_ptr->getType();
				context.builder.CreateStore(end_ptr, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
			else
			{
				return val_ptr::get_value(end_ptr);
			}
		}
		case ast::function_body::builtin_str_from_ptrs:
		{
			bz_assert(func_call.params.size() == 2);
			auto const begin_ptr = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const end_ptr   = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
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
				llvm::Value *result = llvm::ConstantStruct::get(
					str_t,
					{ llvm::UndefValue::get(str_member_t), llvm::UndefValue::get(str_member_t) }
				);
				result = context.builder.CreateInsertValue(result, begin_ptr, 0);
				result = context.builder.CreateInsertValue(result, end_ptr,   1);
				return val_ptr::get_value(result);
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
				auto const result_type = begin_ptr->getType();
				context.builder.CreateStore(begin_ptr, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
			else
			{
				return val_ptr::get_value(begin_ptr);
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
				auto const result_type = end_ptr->getType();
				context.builder.CreateStore(end_ptr, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
			else
			{
				return val_ptr::get_value(end_ptr);
			}
		}
		case ast::function_body::builtin_slice_from_ptrs:
		case ast::function_body::builtin_slice_from_const_ptrs:
		{
			bz_assert(func_call.params.size() == 2);
			auto const begin_ptr = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			auto const end_ptr   = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
			auto const slice_t = get_llvm_type(func_call.func_body->return_type, context);
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
				llvm::Value *result = llvm::ConstantStruct::get(
					static_cast<llvm::StructType *>(slice_t),
					{ llvm::UndefValue::get(slice_member_t), llvm::UndefValue::get(slice_member_t) }
				);
				result = context.builder.CreateInsertValue(result, begin_ptr, 0);
				result = context.builder.CreateInsertValue(result, end_ptr,   1);
				return val_ptr::get_value(result);
			}
		}
		case ast::function_body::builtin_pointer_cast:
			if constexpr (is_comptime<Context>)
			{
				emit_error(
					func_call.src_tokens,
					bz::format("'{}' cannot be used in a constant expression", func_call.func_body->get_signature()), context
				);
				auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
				if (result_address != nullptr)
				{
					return val_ptr::get_reference(result_address, result_type);
				}
				else
				{
					return val_ptr::get_value(llvm::UndefValue::get(result_type));
				}
			}
			else
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
					auto const result_type = result->getType();
					context.builder.CreateStore(result, result_address);
					return val_ptr::get_reference(result_address, result_type);
				}
				else
				{
					return val_ptr::get_value(result);
				}
			}
		case ast::function_body::builtin_pointer_to_int:
		{
			bz_assert(func_call.params.size() == 1);
			auto const ptr = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			bz_assert(ptr->getType()->isPointerTy());
			auto const result = context.builder.CreatePtrToInt(ptr, context.get_usize_t());
			if (result_address != nullptr)
			{
				auto const result_type = result->getType();
				context.builder.CreateStore(result, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
			else
			{
				return val_ptr::get_value(result);
			}
		}
		case ast::function_body::builtin_int_to_pointer:
			if constexpr (is_comptime<Context>)
			{
				emit_error(
					func_call.src_tokens,
					bz::format("'{}' cannot be used in a constant expression", func_call.func_body->get_signature()), context
				);
				auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
				if (result_address != nullptr)
				{
					return val_ptr::get_reference(result_address, result_type);
				}
				else
				{
					return val_ptr::get_value(llvm::UndefValue::get(result_type));
				}
			}
			else
			{
				bz_assert(func_call.params.size() == 2);
				bz_assert(func_call.params[0].is_typename());
				auto const dest_type = get_llvm_type(func_call.params[0].get_typename(), context);
				auto const val = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
				bz_assert(val->getType()->isIntegerTy());
				auto const result = context.builder.CreateIntToPtr(val, dest_type);
				if (result_address != nullptr)
				{
					auto const result_type = result->getType();
					context.builder.CreateStore(result, result_address);
					return val_ptr::get_reference(result_address, result_type);
				}
				else
				{
					return val_ptr::get_value(result);
				}
			}
		case ast::function_body::builtin_call_destructor:
		{
			bz_assert(func_call.params.size() == 1);
			auto const type = func_call.params[0].get_expr_type_and_kind().first;
			auto const arg = emit_bitcode<abi>(func_call.params[0], context, nullptr);
			bz_assert(arg.kind == val_ptr::reference);
			emit_destructor_call(func_call.src_tokens, arg.val, type, context);
			return val_ptr::get_none();
		}
		case ast::function_body::builtin_inplace_construct:
		{
			bz_assert(func_call.params.size() == 2);
			auto const dest_ptr = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
			emit_bitcode<abi>(func_call.params[1], context, dest_ptr);
			return val_ptr::get_none();
		}
		case ast::function_body::builtin_is_comptime:
		{
			auto const result_val = is_comptime<Context>
				? llvm::ConstantInt::getTrue(context.get_llvm_context())
				: llvm::ConstantInt::getFalse(context.get_llvm_context());
			if (result_address != nullptr)
			{
				auto const result_type = result_val->getType();
				context.builder.CreateStore(result_val, result_address);
				return val_ptr::get_reference(result_address, result_type);
			}
			else
			{
				return val_ptr::get_value(result_val);
			}
		}
		case ast::function_body::builtin_panic:
			if constexpr (is_comptime<Context>)
			{
				emit_error(
					func_call.src_tokens,
					bz::format("'{}' called in compile time execution", func_call.func_body->get_signature()),
					context
				);
				bz_assert(func_call.func_body->return_type.is<ast::ts_void>());
				bz_assert(result_address == nullptr);
				return val_ptr::get_none();
			}
			else
			{
				break;
			}
		case ast::function_body::print_stdout:
		case ast::function_body::print_stderr:
			if constexpr (is_comptime<Context>)
			{
				emit_error(
					func_call.src_tokens,
					bz::format("'{}' cannot be used in a constant expression", func_call.func_body->get_signature()), context
				);
				bz_assert(func_call.func_body->return_type.is<ast::ts_void>());
				bz_assert(result_address == nullptr);
				return val_ptr::get_none();
			}
			else
			{
				break;
			}
		case ast::function_body::comptime_malloc_type:
			if constexpr (is_comptime<Context>)
			{
				bz_assert(func_call.params.size() == 2);
				auto const alloc_type = get_llvm_type(func_call.func_body->return_type.get<ast::ts_pointer>(), context);
				auto const result_type = llvm::PointerType::get(alloc_type, 0);
				auto const alloc_type_size = context.get_size(alloc_type);
				auto const type_size_val = llvm::ConstantInt::get(context.get_usize_t(), alloc_type_size);
				auto const count = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
				auto const alloc_size = context.builder.CreateMul(count, type_size_val);
				auto const malloc_fn = context.get_function(context.get_builtin_function(ast::function_body::comptime_malloc));
				auto const result_void_ptr = context.create_call(malloc_fn, alloc_size);
				auto const [src_begin_val, src_pivot_val, src_end_val] = get_src_tokens_llvm_value(src_tokens, context);
				// always check malloc result
				auto const non_null = context.create_call(
					context.get_comptime_function(ctx::comptime_function_kind::comptime_malloc_check),
					{ result_void_ptr, alloc_size, src_begin_val, src_pivot_val, src_end_val }
				);
				emit_error_assert(non_null, context);
				if (context.do_error_checking())
				{
					context.create_call(
						context.get_comptime_function(ctx::comptime_function_kind::register_malloc),
						{ result_void_ptr, alloc_size, src_begin_val, src_pivot_val, src_end_val }
					);
				}
				auto const result = context.builder.CreatePointerCast(result_void_ptr, result_type);
				if (result_address != nullptr)
				{
					context.builder.CreateStore(result, result_address);
					return val_ptr::get_reference(result_address, result_type);
				}
				else
				{
					return val_ptr::get_value(result);
				}
			}
			else
			{
				bz_assert(func_call.func_body->is_only_consteval());
				bz_unreachable;
			}
		case ast::function_body::comptime_free:
			if constexpr (is_comptime<Context>)
			{
				bz_assert(func_call.params.size() == 1);
				auto const ptr = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
				if (context.do_error_checking())
				{
					auto const [src_begin_val, src_pivot_val, src_end_val] = get_src_tokens_llvm_value(src_tokens, context);
					auto const is_good = context.create_call(
						context.get_comptime_function(ctx::comptime_function_kind::register_free),
						{ ptr, src_begin_val, src_pivot_val, src_end_val }
					);
					emit_error_assert(is_good, context);
				}
				auto const free_fn = context.get_function(func_call.func_body);
				context.create_call(free_fn, { ptr });
				return val_ptr::get_none();
			}
			else
			{
				bz_assert(func_call.func_body->is_only_consteval());
				bz_unreachable;
			}

		case ast::function_body::comptime_compile_error:
		case ast::function_body::comptime_compile_warning:
			if constexpr (is_comptime<Context>)
			{
				auto const fn = func_call.func_body->intrinsic_kind == ast::function_body::comptime_compile_error
					? context.get_function(context.get_builtin_function(ast::function_body::comptime_compile_error_src_tokens))
					: context.get_function(context.get_builtin_function(ast::function_body::comptime_compile_warning_src_tokens));
				auto const message_val = emit_bitcode<abi>(func_call.params[0], context, nullptr);
				auto const [src_begin_val, src_pivot_val, src_end_val] = get_src_tokens_llvm_value(src_tokens, context);
				ast::arena_vector<llvm::Value *> params;
				params.reserve(5);
				ast::arena_vector<is_byval_and_type_pair> params_is_byval;
				params_is_byval.reserve(2);
				add_call_parameter<abi>(
					func_call.params[0].get_expr_type_and_kind().first, context.get_str_t(),
					message_val, params, params_is_byval, context
				);

				params.push_back(src_begin_val);
				params.push_back(src_pivot_val);
				params.push_back(src_end_val);

				auto const call = context.create_call(fn, llvm::ArrayRef(params.data(), params.size()));
				bz_assert(!params_is_byval.empty());
				if (params_is_byval[0].is_byval)
				{
					add_byval_attributes<abi>(call, params_is_byval[0].type, 0, context);
				}
				return val_ptr::get_none();
			}
			else
			{
				bz_assert(func_call.func_body->is_only_consteval());
				bz_unreachable;
			}

		case ast::function_body::memcpy:
			if constexpr (is_comptime<Context>)
			{
				bz_assert(func_call.params.size() == 3);
				auto const dest = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
				auto const src  = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
				auto const size = emit_bitcode<abi>(func_call.params[2], context, nullptr).get_value(context.builder);
				auto const false_val = llvm::ConstantInt::getFalse(context.get_llvm_context());
				if (context.do_error_checking())
				{
					auto const [error_begin_val, error_pivot_val, error_end_val] = get_src_tokens_llvm_value(src_tokens, context);
					auto const is_valid = context.create_call(
						context.get_comptime_function(ctx::comptime_function_kind::comptime_memcpy_check),
						{ dest, src, size, error_begin_val, error_pivot_val, error_end_val }
					);
					emit_error_assert(is_valid, context);
				}
				context.create_call(context.get_function(func_call.func_body), { dest, src, size, false_val });
				return val_ptr::get_none();
			}
			else
			{
				break;
			}

		case ast::function_body::memmove:
			if constexpr (is_comptime<Context>)
			{
				bz_assert(func_call.params.size() == 3);
				auto const dest = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
				auto const src  = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
				auto const size = emit_bitcode<abi>(func_call.params[2], context, nullptr).get_value(context.builder);
				auto const false_val = llvm::ConstantInt::getFalse(context.get_llvm_context());
				if (context.do_error_checking())
				{
					auto const [error_begin_val, error_pivot_val, error_end_val] = get_src_tokens_llvm_value(src_tokens, context);
					auto const is_valid = context.create_call(
						context.get_comptime_function(ctx::comptime_function_kind::comptime_memmove_check),
						{ dest, src, size, error_begin_val, error_pivot_val, error_end_val }
					);
					emit_error_assert(is_valid, context);
				}
				context.create_call(context.get_function(func_call.func_body), { dest, src, size, false_val });
				return val_ptr::get_none();
			}
			else
			{
				break;
			}

		case ast::function_body::memset:
			if constexpr (is_comptime<Context>)
			{
				bz_assert(func_call.params.size() == 3);
				auto const dest = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
				auto const val  = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
				auto const size = emit_bitcode<abi>(func_call.params[2], context, nullptr).get_value(context.builder);
				auto const false_val = llvm::ConstantInt::getFalse(context.get_llvm_context());
				if (context.do_error_checking())
				{
					auto const [error_begin_val, error_pivot_val, error_end_val] = get_src_tokens_llvm_value(src_tokens, context);
					auto const is_valid = context.create_call(
						context.get_comptime_function(ctx::comptime_function_kind::comptime_memset_check),
						{ dest, val, size, error_begin_val, error_pivot_val, error_end_val }
					);
					emit_error_assert(is_valid, context);
				}
				context.create_call(context.get_function(func_call.func_body), { dest, val, size, false_val });
				return val_ptr::get_none();
			}
			else
			{
				break;
			}

		case ast::function_body::exp_f32:   case ast::function_body::exp_f64:
		case ast::function_body::exp2_f32:  case ast::function_body::exp2_f64:
		case ast::function_body::expm1_f32: case ast::function_body::expm1_f64:
		case ast::function_body::log_f32:   case ast::function_body::log_f64:
		case ast::function_body::log10_f32: case ast::function_body::log10_f64:
		case ast::function_body::log2_f32:  case ast::function_body::log2_f64:
		case ast::function_body::log1p_f32: case ast::function_body::log1p_f64:
			[[fallthrough]];
		case ast::function_body::sqrt_f32:  case ast::function_body::sqrt_f64:
		case ast::function_body::cbrt_f32:  case ast::function_body::cbrt_f64:
			[[fallthrough]];
		case ast::function_body::sin_f32:   case ast::function_body::sin_f64:
		case ast::function_body::cos_f32:   case ast::function_body::cos_f64:
		case ast::function_body::tan_f32:   case ast::function_body::tan_f64:
		case ast::function_body::asin_f32:  case ast::function_body::asin_f64:
		case ast::function_body::acos_f32:  case ast::function_body::acos_f64:
		case ast::function_body::atan_f32:  case ast::function_body::atan_f64:
			[[fallthrough]];
		case ast::function_body::sinh_f32:  case ast::function_body::sinh_f64:
		case ast::function_body::cosh_f32:  case ast::function_body::cosh_f64:
		case ast::function_body::tanh_f32:  case ast::function_body::tanh_f64:
		case ast::function_body::asinh_f32: case ast::function_body::asinh_f64:
		case ast::function_body::acosh_f32: case ast::function_body::acosh_f64:
		case ast::function_body::atanh_f32: case ast::function_body::atanh_f64:
			[[fallthrough]];
		case ast::function_body::erf_f32:    case ast::function_body::erf_f64:
		case ast::function_body::erfc_f32:   case ast::function_body::erfc_f64:
		case ast::function_body::tgamma_f32: case ast::function_body::tgamma_f64:
		case ast::function_body::lgamma_f32: case ast::function_body::lgamma_f64:
			if constexpr (is_comptime<Context>)
			{
				bz_assert(func_call.params.size() == 1);
				auto const val = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
				auto const fn = context.get_function(func_call.func_body);
				auto const result_val = context.create_call(fn, { val });
				if (context.do_error_checking())
				{
					auto const [src_begin, src_pivot, src_end] = get_src_tokens_llvm_value(src_tokens, context);
					auto const check_fn_kind = get_math_check_function_kind(func_call.func_body->intrinsic_kind);
					auto const check_fn = context.get_comptime_function(check_fn_kind);
					auto const is_valid = context.create_call(check_fn, { val, result_val, src_begin, src_pivot, src_end });
					emit_error_assert(is_valid, context);
				}

				if (result_address == nullptr)
				{
					return val_ptr::get_value(result_val);
				}
				else
				{
					auto const result_type = result_val->getType();
					context.builder.CreateStore(result_val, result_address);
					return val_ptr::get_reference(result_address, result_type);
				}
			}
			else
			{
				break;
			}

		case ast::function_body::pow_f32:   case ast::function_body::pow_f64:
		case ast::function_body::hypot_f32: case ast::function_body::hypot_f64:
		case ast::function_body::atan2_f32: case ast::function_body::atan2_f64:
			if constexpr (is_comptime<Context>)
			{
				bz_assert(func_call.params.size() == 2);
				auto const val1 = emit_bitcode<abi>(func_call.params[0], context, nullptr).get_value(context.builder);
				auto const val2 = emit_bitcode<abi>(func_call.params[1], context, nullptr).get_value(context.builder);
				auto const fn = context.get_function(func_call.func_body);
				auto const result_val = context.create_call(fn, { val1, val2 });
				if (context.do_error_checking())
				{
					auto const [src_begin, src_pivot, src_end] = get_src_tokens_llvm_value(src_tokens, context);
					auto const check_fn_kind = get_math_check_function_kind(func_call.func_body->intrinsic_kind);
					auto const check_fn = context.get_comptime_function(check_fn_kind);
					auto const is_valid = context.create_call(check_fn, { val1, val2, result_val, src_begin, src_pivot, src_end });
					emit_error_assert(is_valid, context);
				}

				if (result_address == nullptr)
				{
					return val_ptr::get_value(result_val);
				}
				else
				{
					auto const result_type = result_val->getType();
					context.builder.CreateStore(result_val, result_address);
					return val_ptr::get_reference(result_address, result_type);
				}
			}
			else
			{
				break;
			}

		case ast::function_body::comptime_concatenate_strs:
		case ast::function_body::typename_as_str:
		case ast::function_body::is_const:
		case ast::function_body::is_consteval:
		case ast::function_body::is_pointer:
		case ast::function_body::is_reference:
		case ast::function_body::is_move_reference:
		case ast::function_body::remove_const:
		case ast::function_body::remove_consteval:
		case ast::function_body::remove_pointer:
		case ast::function_body::remove_reference:
		case ast::function_body::remove_move_reference:
		case ast::function_body::is_default_constructible:
		case ast::function_body::is_copy_constructible:
		case ast::function_body::is_trivially_copy_constructible:
		case ast::function_body::is_trivially_destructible:
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
			return emit_builtin_unary_plus<abi>(func_call.params[0], context, result_address);
		case ast::function_body::builtin_unary_minus:
			return emit_builtin_unary_minus<abi>(func_call.params[0], context, result_address);
		case ast::function_body::builtin_unary_dereference:
			return emit_builtin_unary_dereference<abi>(src_tokens, func_call.params[0], context, result_address);
		case ast::function_body::builtin_unary_bit_not:
			return emit_builtin_unary_bit_not<abi>(func_call.params[0], context, result_address);
		case ast::function_body::builtin_unary_bool_not:
			return emit_builtin_unary_bool_not<abi>(func_call.params[0], context, result_address);
		case ast::function_body::builtin_unary_plus_plus:
			return emit_builtin_unary_plus_plus<abi>(func_call.params[0], context, result_address);
		case ast::function_body::builtin_unary_minus_minus:
			return emit_builtin_unary_minus_minus<abi>(func_call.params[0], context, result_address);

		case ast::function_body::builtin_binary_assign:
			return emit_builtin_binary_assign<abi>(src_tokens, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_plus:
			return emit_builtin_binary_plus<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_plus_eq:
			return emit_builtin_binary_plus_eq<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_minus:
			return emit_builtin_binary_minus<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_minus_eq:
			return emit_builtin_binary_minus_eq<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_multiply:
			return emit_builtin_binary_multiply<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_multiply_eq:
			return emit_builtin_binary_multiply_eq<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_divide:
			return emit_builtin_binary_divide<abi>(src_tokens, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_divide_eq:
			return emit_builtin_binary_divide_eq<abi>(src_tokens, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_modulo:
			return emit_builtin_binary_modulo<abi>(src_tokens, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_modulo_eq:
			return emit_builtin_binary_modulo_eq<abi>(src_tokens, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_equals:
			return emit_builtin_binary_cmp<abi>(lex::token::equals, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_not_equals:
			return emit_builtin_binary_cmp<abi>(lex::token::not_equals, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_less_than:
			return emit_builtin_binary_cmp<abi>(lex::token::less_than, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_less_than_eq:
			return emit_builtin_binary_cmp<abi>(lex::token::less_than_eq, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_greater_than:
			return emit_builtin_binary_cmp<abi>(lex::token::greater_than, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_greater_than_eq:
			return emit_builtin_binary_cmp<abi>(lex::token::greater_than_eq, func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_and:
			return emit_builtin_binary_bit_and<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_and_eq:
			return emit_builtin_binary_bit_and_eq<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_xor:
			return emit_builtin_binary_bit_xor<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_xor_eq:
			return emit_builtin_binary_bit_xor_eq<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_or:
			return emit_builtin_binary_bit_or<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_or_eq:
			return emit_builtin_binary_bit_or_eq<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_left_shift:
			return emit_builtin_binary_left_shift<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_left_shift_eq:
			return emit_builtin_binary_left_shift_eq<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_right_shift:
			return emit_builtin_binary_right_shift<abi>(func_call.params[0], func_call.params[1], context, result_address);
		case ast::function_body::builtin_binary_bit_right_shift_eq:
			return emit_builtin_binary_right_shift_eq<abi>(func_call.params[0], func_call.params[1], context, result_address);

		default:
			break;
		}
	}
	else if (func_call.func_body->is_default_op_assign())
	{
		return emit_default_copy_assign<abi>(func_call.src_tokens, func_call.params[0], func_call.params[1], context, result_address);
	}
	else if (func_call.func_body->is_default_op_move_assign())
	{
		return emit_default_move_assign<abi>(func_call.src_tokens, func_call.params[0], func_call.params[1], context, result_address);
	}
	else if (func_call.func_body->is_default_copy_constructor())
	{
		auto const expr_val = emit_bitcode<abi>(func_call.params[0], context, nullptr);
		auto const expr_type = func_call.func_body->return_type.as_typespec_view();
		return emit_copy_constructor<abi>(func_call.src_tokens, expr_val, expr_type, context, result_address);
	}
	else if (func_call.func_body->is_default_default_constructor())
	{
		return emit_default_constructor<abi>(func_call.src_tokens, func_call.func_body->return_type, context, result_address);
	}

	bz_assert(func_call.func_body != nullptr);
	if constexpr (is_comptime<Context>)
	{
		if (!func_call.func_body->is_intrinsic() && func_call.func_body->body.is_null())
		{
			emit_error(
				func_call.src_tokens,
				bz::format("unable to call external function '{}' in compile time execution", func_call.func_body->get_signature()),
				context
			);
			auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
			if (result_address != nullptr)
			{
				return val_ptr::get_reference(result_address, result_type);
			}
			else if (result_type->isVoidTy())
			{
				return val_ptr::get_none();
			}
			else
			{
				return val_ptr::get_value(llvm::UndefValue::get(result_type));
			}
		}
	}

	auto const fn = context.get_function(func_call.func_body);
	bz_assert(fn != nullptr);

	auto const result_type = get_llvm_type(func_call.func_body->return_type, context);
	auto const result_kind = context.template get_pass_kind<abi>(func_call.func_body->return_type, result_type);

	ast::arena_vector<llvm::Value *> params = {};
	ast::arena_vector<is_byval_and_type_pair> params_is_byval = {};
	params.reserve(
		func_call.params.size()
		+ (result_kind == abi::pass_kind::reference || result_kind == abi::pass_kind::non_trivial ? 1 : 0)
	);
	params_is_byval.reserve(
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
		else
		{
			auto const param_llvm_type = get_llvm_type(param_type, context);
			if (param_type.is<ast::ts_move_reference>())
			{
				auto const result_address = ast::is_rvalue_or_literal(p.get_expr_type_and_kind().second)
					? context.create_alloca(get_llvm_type(param_type.get<ast::ts_move_reference>(), context))
					: nullptr;
				auto const param_val = emit_bitcode<abi>(p, context, result_address);
				if (result_address != nullptr)
				{
					push_destructor_call(src_tokens, result_address, param_type.get<ast::ts_move_reference>(), context);
				}
				add_call_parameter<abi, push_to_front>(param_type, param_llvm_type, param_val, params, params_is_byval, context);
			}
			else
			{
				auto const param_val = ast::is_non_trivial(param_type)
					? emit_bitcode<abi>(p, context, context.create_alloca(param_llvm_type))
					: emit_bitcode<abi>(p, context, nullptr);
				add_call_parameter<abi, push_to_front>(param_type, param_llvm_type, param_val, params, params_is_byval, context);
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

	if (result_kind == abi::pass_kind::reference || result_kind == abi::pass_kind::non_trivial)
	{
		auto const output_ptr = result_address != nullptr
			? result_address
			: context.create_alloca(result_type);
		params.push_front(output_ptr);
		params_is_byval.push_front({ false, nullptr });
	}

	if (
		func_call.func_body->is_intrinsic()
		&& (
			func_call.func_body->intrinsic_kind == ast::function_body::memcpy
			|| func_call.func_body->intrinsic_kind == ast::function_body::memmove
			|| func_call.func_body->intrinsic_kind == ast::function_body::memset
			|| func_call.func_body->intrinsic_kind == ast::function_body::clz_u8
			|| func_call.func_body->intrinsic_kind == ast::function_body::clz_u16
			|| func_call.func_body->intrinsic_kind == ast::function_body::clz_u32
			|| func_call.func_body->intrinsic_kind == ast::function_body::clz_u64
			|| func_call.func_body->intrinsic_kind == ast::function_body::ctz_u8
			|| func_call.func_body->intrinsic_kind == ast::function_body::ctz_u16
			|| func_call.func_body->intrinsic_kind == ast::function_body::ctz_u32
			|| func_call.func_body->intrinsic_kind == ast::function_body::ctz_u64
		)
	)
	{
		params.push_back(llvm::ConstantInt::getFalse(context.get_llvm_context()));
		params_is_byval.push_back({ false, nullptr });
	}

	auto const call = [&]() {
		if constexpr (is_comptime<Context>)
		{
			llvm::Value *pre_call_error_count = nullptr;
			if (!func_call.func_body->is_no_comptime_checking())
			{
				pre_call_error_count = emit_push_call(func_call.src_tokens, func_call.func_body, context);
			}

			if (false)
			{
				auto const fn_type = llvm::FunctionType::get(
					llvm::Type::getVoidTy(context.get_llvm_context()),
					{ llvm::Type::getInt8PtrTy(context.get_llvm_context()) },
					false
				);
				static auto debug_print_func = llvm::Function::Create(
					fn_type,
					llvm::Function::ExternalLinkage,
					"__bozon_builtin_debug_print",
					context.get_module()
				);

				auto const file = context.global_ctx.get_file_name(src_tokens.pivot->src_pos.file_id);
				// if (!file.ends_with("__comptime_checking.bz"))
				{
					auto const line = src_tokens.pivot->src_pos.line;
					auto const message = bz::format("{}:{}: {}", file, line, func_call.func_body->get_signature());
					auto const string_constant = context.create_string(message);
					context.create_call(debug_print_func, { string_constant });
				}
			}

			auto const call = context.create_call(fn, llvm::ArrayRef(params.data(), params.size()));
			auto is_byval_it = params_is_byval.begin();
			auto const is_byval_end = params_is_byval.end();
			unsigned i = 0;
			bz_assert(fn->arg_size() == call->arg_size());
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
					add_byval_attributes<abi>(call, is_byval_it->type, i, context);
				}
			}

			if (!func_call.func_body->is_no_comptime_checking())
			{
				emit_pop_call(pre_call_error_count, context);
			}
			return call;
		}
		else
		{
			auto const call = context.create_call(fn, llvm::ArrayRef(params.data(), params.size()));
			auto is_byval_it = params_is_byval.begin();
			auto const is_byval_end = params_is_byval.end();
			unsigned i = 0;
			bz_assert(fn->arg_size() == call->arg_size());
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
					add_byval_attributes<abi>(call, is_byval_it->type, i, context);
				}
			}
			return call;
		}
	}();

	switch (result_kind)
	{
	case abi::pass_kind::reference:
	case abi::pass_kind::non_trivial:
		bz_assert(result_address == nullptr || params.front() == result_address);
		return val_ptr::get_reference(params.front(), result_type);
	case abi::pass_kind::value:
		if (call->getType()->isVoidTy())
		{
			return val_ptr::get_none();
		}
		else if (func_call.func_body->return_type.is<ast::ts_lvalue_reference>())
		{
			auto const inner_result_type = func_call.func_body->return_type.get<ast::ts_lvalue_reference>();
			auto const inner_result_llvm_type = get_llvm_type(inner_result_type, context);
			if (result_address == nullptr)
			{
				return val_ptr::get_reference(call, inner_result_llvm_type);
			}
			else
			{
				emit_copy_constructor<abi>(
					src_tokens,
					val_ptr::get_reference(call, inner_result_llvm_type),
					inner_result_type,
					context,
					result_address
				);
				return val_ptr::get_reference(result_address, inner_result_llvm_type);
			}
		}
		if (result_address == nullptr)
		{
			return val_ptr::get_value(call);
		}
		else
		{
			context.builder.CreateStore(call, result_address);
			return val_ptr::get_reference(result_address, call->getType());
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
			return val_ptr::get_reference(result_address, result_type);
		}
		else if (result_type == call_result_type)
		{
			return val_ptr::get_value(call);
		}
		else
		{
			auto const result_ptr = context.create_alloca(result_type);
			auto const result_ptr_cast = context.builder.CreateBitCast(
				result_ptr,
				llvm::PointerType::get(call_result_type, 0)
			);
			context.builder.CreateStore(call, result_ptr_cast);
			return val_ptr::get_reference(result_ptr, result_type);
		}
	}
	default:
		bz_unreachable;
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_subscript const &subscript,
	Context &context,
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
		if (ast::is_unsigned_integer_kind(kind))
		{
			index_val = context.builder.CreateIntCast(index_val, context.get_usize_t(), false);
		}

		// array bounds check
		if constexpr (is_comptime<Context>)
		{
			if (context.do_error_checking())
			{
				auto const array_size = base_type.get<ast::ts_array>().size;
				auto const array_size_val = llvm::ConstantInt::get(context.get_usize_t(), array_size);
				emit_index_bounds_check(
					src_tokens,
					context.builder.CreateIntCast(index_val, array_size_val->getType(), ast::is_signed_integer_kind(kind)),
					array_size_val,
					ast::is_unsigned_integer_kind(kind),
					context
				);
			}
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

		if (result_address == nullptr)
		{
			return val_ptr::get_reference(result_ptr, elem_llvm_type);
		}
		else
		{
			emit_copy_constructor<abi>(
				src_tokens,
				val_ptr::get_reference(result_ptr, elem_llvm_type),
				elem_type,
				context,
				result_address
			);
			return val_ptr::get_reference(result_address, elem_llvm_type);
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
		if (ast::is_unsigned_integer_kind(kind))
		{
			index_val = context.builder.CreateIntCast(index_val, context.get_usize_t(), false);
		}

		// array bounds check
		if constexpr (is_comptime<Context>)
		{
			if (context.do_error_checking())
			{
				auto const end_ptr = context.builder.CreateExtractValue(array_val, 1);
				auto const elem_type = get_llvm_type(base_type.get<ast::ts_array_slice>().elem_type, context);
				auto const array_size_val = context.builder.CreatePtrDiff(elem_type, end_ptr, begin_ptr);
				emit_index_bounds_check(
					src_tokens,
					context.builder.CreateIntCast(index_val, array_size_val->getType(), ast::is_signed_integer_kind(kind)),
					array_size_val,
					ast::is_unsigned_integer_kind(kind),
					context
				);
			}
		}

		auto const elem_type = base_type.get<ast::ts_array_slice>().elem_type.as_typespec_view();
		auto const elem_llvm_type = get_llvm_type(elem_type, context);

		auto const result_ptr = context.create_gep(elem_llvm_type, begin_ptr, index_val);

		if (result_address == nullptr)
		{
			return val_ptr::get_reference(result_ptr, elem_llvm_type);
		}
		else
		{
			emit_copy_constructor<abi>(
				src_tokens,
				val_ptr::get_reference(result_ptr, elem_llvm_type),
				elem_type,
				context,
				result_address
			);
			return val_ptr::get_reference(result_address, elem_llvm_type);
		}
	}
	else
	{
		bz_assert(base_type.is<ast::ts_tuple>() || subscript.base.is_tuple());
		auto const tuple = emit_bitcode<abi>(subscript.base, context, nullptr);
		bz_assert(subscript.index.is<ast::constant_expression>());
		auto const &index_value = subscript.index.get<ast::constant_expression>().value;
		bz_assert(index_value.is<ast::constant_value::uint>() || index_value.is<ast::constant_value::sint>());
		auto const index_int_value = index_value.is<ast::constant_value::uint>()
			? index_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(index_value.get<ast::constant_value::sint>());

		auto const accessed_type = base_type.is<ast::ts_tuple>()
			? base_type.get<ast::ts_tuple>().types[index_int_value].as_typespec_view()
			: subscript.base.get_tuple().elems[index_int_value].get_expr_type_and_kind().first;

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
			auto const result_type = get_llvm_type(ast::remove_lvalue_reference(accessed_type), context);
			if (result_address == nullptr)
			{
				return val_ptr::get_reference(result_ptr, result_type);
			}
			else
			{
				emit_copy_constructor<abi>(
					src_tokens,
					val_ptr::get_reference(result_ptr, result_type),
					ast::remove_lvalue_reference(accessed_type),
					context,
					result_address
				);
				return val_ptr::get_reference(result_address, result_type);
			}
		}
		else
		{
			auto const result_val = context.builder.CreateExtractValue(tuple.get_value(context.builder), index_int_value);
			if (result_address == nullptr)
			{
				return val_ptr::get_value(result_val);
			}
			else
			{
				context.builder.CreateStore(result_val, result_address);
				return val_ptr::get_reference(result_address, result_val->getType());
			}
		}
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_cast const &cast,
	Context &context,
	llvm::Value *result_address
)
{
	auto const expr_t = ast::remove_const_or_consteval(cast.expr.get_expr_type_and_kind().first);
	auto const dest_t = ast::remove_const_or_consteval(cast.type);

	if (expr_t.is<ast::ts_base_type>() && dest_t.is<ast::ts_base_type>())
	{
		auto const llvm_dest_t = get_llvm_type(dest_t, context);
		auto const expr = emit_bitcode<abi>(cast.expr, context, nullptr).get_value(context.builder);
		auto const expr_kind = expr_t.get<ast::ts_base_type>().info->kind;
		auto const dest_kind = dest_t.get<ast::ts_base_type>().info->kind;

		if (ast::is_integer_kind(expr_kind) && ast::is_integer_kind(dest_kind))
		{
			auto const res = context.builder.CreateIntCast(
				expr,
				llvm_dest_t,
				ast::is_signed_integer_kind(expr_kind),
				"cast_tmp"
			);
			if (result_address == nullptr)
			{
				return val_ptr::get_value(res);
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return val_ptr::get_reference(result_address, res->getType());
			}
		}
		else if (ast::is_floating_point_kind(expr_kind) && ast::is_floating_point_kind(dest_kind))
		{
			auto const res = context.builder.CreateFPCast(expr, llvm_dest_t, "cast_tmp");
			if (result_address == nullptr)
			{
				return val_ptr::get_value(res);
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return val_ptr::get_reference(result_address, res->getType());
			}
		}
		else if (ast::is_floating_point_kind(expr_kind))
		{
			bz_assert(ast::is_integer_kind(dest_kind));
			auto const res = ast::is_signed_integer_kind(dest_kind)
				? context.builder.CreateFPToSI(expr, llvm_dest_t, "cast_tmp")
				: context.builder.CreateFPToUI(expr, llvm_dest_t, "cast_tmp");
			if (result_address == nullptr)
			{
				return val_ptr::get_value(res);
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return val_ptr::get_reference(result_address, res->getType());
			}
		}
		else if (ast::is_integer_kind(expr_kind) && ast::is_floating_point_kind(dest_kind))
		{
			auto const res = ast::is_signed_integer_kind(expr_kind)
				? context.builder.CreateSIToFP(expr, llvm_dest_t, "cast_tmp")
				: context.builder.CreateUIToFP(expr, llvm_dest_t, "cast_tmp");
			if (result_address == nullptr)
			{
				return val_ptr::get_value(res);
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return val_ptr::get_reference(result_address, res->getType());
			}
		}
		else if (expr_kind == ast::type_info::bool_ && ast::is_integer_kind(dest_kind))
		{
			auto const res = context.builder.CreateIntCast(expr, llvm_dest_t, false, "cast_tmp");
			if (result_address == nullptr)
			{
				return val_ptr::get_value(res);
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return val_ptr::get_reference(result_address, res->getType());
			}
		}
		else
		{
			// this is a cast from i32 or to i32 in IR, so we emit an integer cast
			bz_assert(
				(expr_kind == ast::type_info::char_ && ast::is_integer_kind(dest_kind))
				|| ( ast::is_integer_kind(expr_kind) && dest_kind == ast::type_info::char_)
			);
			auto const res = context.builder.CreateIntCast(expr, llvm_dest_t, ast::is_signed_integer_kind(expr_kind), "cast_tmp");
			if (result_address == nullptr)
			{
				return val_ptr::get_value(res);
			}
			else
			{
				context.builder.CreateStore(res, result_address);
				return val_ptr::get_reference(result_address, res->getType());
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
			return val_ptr::get_value(cast_result);
		}
		else
		{
			context.builder.CreateStore(cast_result, result_address);
			return val_ptr::get_reference(result_address, cast_result->getType());
		}
	}
	else if (expr_t.is<ast::ts_array>() && dest_t.is<ast::ts_array_slice>())
	{
		auto const expr_val = emit_bitcode<abi>(cast.expr, context, nullptr);
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
			llvm::Value *result = llvm::ConstantStruct::get(
				slice_struct_t,
				{ llvm::UndefValue::get(slice_member_t), llvm::UndefValue::get(slice_member_t) }
			);
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

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_take_reference const &take_ref,
	Context &context,
	llvm::Value *result_address
)
{
	auto const result = emit_bitcode<abi>(take_ref.expr, context, nullptr);
	if constexpr (is_comptime<Context>)
	{
		if (result.kind != val_ptr::reference)
		{
			if (auto const id_expr = take_ref.expr.get_expr().get_if<ast::expr_identifier>(); id_expr && id_expr->decl != nullptr)
			{
				emit_error(
					take_ref.expr.src_tokens,
					bz::format("unable to take reference to variable '{}'", id_expr->decl->get_id().format_as_unqualified()),
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
			return val_ptr::get_reference(alloca, result.get_type());
		}
	}
	if (result_address != nullptr)
	{
		auto const result_type = ast::remove_const_or_consteval(take_ref.expr.get_expr_type_and_kind().first);
		bz_assert(result.kind == val_ptr::reference);
		emit_copy_constructor<abi>(src_tokens, result, result_type, context, result_address);
		return val_ptr::get_reference(result_address, result.get_type());
	}
	else
	{
		return result;
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_struct_init const &struct_init,
	Context &context,
	llvm::Value *result_address
)
{
	auto const type = get_llvm_type(struct_init.type, context);
	auto const result_ptr = result_address != nullptr ? result_address : context.create_alloca(type);
	for (auto const i : bz::iota(0, struct_init.exprs.size()))
	{
		auto const member_ptr = context.create_struct_gep(type, result_ptr, i);
		emit_bitcode<abi>(struct_init.exprs[i], context, member_ptr);
	}
	return val_ptr::get_reference(result_ptr, type);
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_array_default_construct const &array_default_construct,
	Context &context,
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
	if (size <= 16)
	{
		for (auto const i : bz::iota(0, size))
		{
			auto const elem_result_address = context.create_struct_gep(llvm_type, result_address, i);
			emit_bitcode<abi>(src_tokens, array_default_construct.elem_ctor_call, context, elem_result_address);
		}
		return val_ptr::get_reference(result_address, llvm_type);
	}
	else
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

		context.builder.SetInsertPoint(loop_bb);
		auto const elem_result_address = context.create_array_gep(llvm_type, result_address, iter_val);
		emit_bitcode<abi>(src_tokens, array_default_construct.elem_ctor_call, context, elem_result_address);
		auto const one_value = llvm::ConstantInt::get(iter_val->getType(), 1);
		auto const next_iter_val = context.builder.CreateAdd(iter_val, one_value);
		context.builder.CreateBr(condition_check_bb);
		auto const loop_end_bb = context.builder.GetInsertBlock();

		context.builder.SetInsertPoint(condition_check_bb);
		iter_val->addIncoming(next_iter_val, loop_end_bb);
		auto const size_value = llvm::ConstantInt::get(iter_val->getType(), size);
		auto const is_at_end = context.builder.CreateICmpEQ(iter_val, size_value);
		context.builder.CreateCondBr(is_at_end, end_bb, loop_bb);
		context.builder.SetInsertPoint(end_bb);
		return val_ptr::get_reference(result_address, llvm_type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_builtin_default_construct const &builtin_default_construct,
	Context &context,
	llvm::Value *result_address
)
{
	auto const type = builtin_default_construct.type.as_typespec_view();
	if (type.is<ast::ts_pointer>())
	{
		auto const llvm_type = get_llvm_type(type, context);
		bz_assert(llvm_type->isPointerTy());
		auto const null_value = llvm::ConstantPointerNull::get(static_cast<llvm::PointerType *>(llvm_type));
		if (result_address != nullptr)
		{
			context.builder.CreateStore(null_value, result_address);
			return val_ptr::get_reference(result_address, llvm_type);
		}
		else
		{
			return val_ptr::get_value(null_value);
		}
	}
	else if (type.is<ast::ts_array_slice>())
	{
		auto const ptr_type = llvm::PointerType::get(get_llvm_type(type.get<ast::ts_array_slice>().elem_type, context), 0);
		auto const result_type = llvm::StructType::get(ptr_type, ptr_type);
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

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_member_access const &member_access,
	Context &context,
	llvm::Value *result_address
)
{
	auto const base = emit_bitcode<abi>(member_access.base, context, nullptr);
	auto const base_type = ast::remove_const_or_consteval(member_access.base.get_expr_type_and_kind().first);
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
		auto const result_type = get_llvm_type(ast::remove_lvalue_reference(accessed_type), context);
		if (result_address == nullptr)
		{
			return val_ptr::get_reference(result_ptr, result_type);
		}
		else
		{
			emit_copy_constructor<abi>(
				src_tokens,
				val_ptr::get_reference(result_ptr, result_type),
				ast::remove_lvalue_reference(accessed_type),
				context,
				result_address
			);
			return val_ptr::get_reference(result_address, result_type);
		}
	}
	else
	{
		auto const val = context.builder.CreateExtractValue(
			base.get_value(context.builder), member_access.index
		);
		if (result_address == nullptr)
		{
			return val_ptr::get_value(val);
		}
		else
		{
			context.builder.CreateStore(val, result_address);
			return val_ptr::get_reference(result_address, val->getType());
		}
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_type_member_access const &member_access,
	Context &context,
	llvm::Value *result_address
)
{
	bz_assert(member_access.var_decl != nullptr);
	auto const decl = member_access.var_decl;
	if constexpr (is_comptime<Context>)
	{
		if (decl->get_type().is<ast::ts_consteval>() && decl->init_expr.not_error())
		{
			context.add_global_variable(decl);
		}
	}
	auto const [ptr, type] = context.get_variable(decl);
	if constexpr (is_comptime<Context>)
	{
		if (ptr == nullptr)
		{
			emit_error(
				lex::src_tokens::from_single_token(member_access.member),
				bz::format("member '{}' cannot be used in a constant expression", member_access.member->value),
				context
			);
			auto const result_type = get_llvm_type(ast::remove_lvalue_reference(decl->get_type()), context);
			if (result_address == nullptr)
			{
				auto const alloca = context.create_alloca(result_type);
				return val_ptr::get_reference(alloca, result_type);
			}
			else
			{
				return val_ptr::get_reference(result_address, result_type);
			}
		}
	}

	if (result_address == nullptr)
	{
		return val_ptr::get_reference(ptr, type);
	}
	else
	{
		emit_copy_constructor<abi>(
			src_tokens,
			val_ptr::get_reference(ptr, type),
			ast::remove_lvalue_reference(decl->get_type()),
			context,
			result_address
		);
		return val_ptr::get_reference(result_address, type);
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_compound const &compound_expr,
	Context &context,
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
		return val_ptr::get_none();
	}
	else
	{
		auto const result = emit_bitcode<abi>(compound_expr.final_expr, context, result_address);
		context.pop_expression_scope();
		return result;
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_if const &if_expr,
	Context &context,
	llvm::Value *result_address
)
{
	context.push_expression_scope();
	auto condition = emit_bitcode<abi>(if_expr.condition, context, nullptr).get_value(context.builder);
	if (condition == nullptr)
	{
		condition = llvm::UndefValue::get(context.get_bool_t());
	}
	context.pop_expression_scope();
	// assert that the condition is an i1 (bool)
	bz_assert(condition->getType()->isIntegerTy() && condition->getType()->getIntegerBitWidth() == 1);
	// the original block
	auto const entry_bb = context.builder.GetInsertBlock();

	if (auto const constant_condition = llvm::dyn_cast<llvm::ConstantInt>(condition))
	{
		if (constant_condition->equalsInt(1))
		{
			return emit_bitcode<abi>(if_expr.then_block, context, result_address);
		}
		else if (if_expr.else_block.not_null())
		{
			return emit_bitcode<abi>(if_expr.else_block, context, result_address);
		}
	}

	// emit code for the then block
	auto const then_bb = context.add_basic_block("then");
	context.builder.SetInsertPoint(then_bb);
	auto const then_val = emit_bitcode<abi>(if_expr.then_block, context, result_address);
	auto const then_bb_end = context.builder.GetInsertBlock();

	// emit code for the else block if there's any
	auto const else_bb = if_expr.else_block.is_null() ? nullptr : context.add_basic_block("else");
	val_ptr else_val = val_ptr::get_none();
	if (else_bb != nullptr)
	{
		context.builder.SetInsertPoint(else_bb);
		else_val = emit_bitcode<abi>(if_expr.else_block, context, result_address);
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
	if (!then_val.has_value() || !else_val.has_value())
	{
		return val_ptr::get_none();
	}

	auto const result_type = then_val.get_type();
	if (result_address != nullptr)
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

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_if_consteval const &if_expr,
	Context &context,
	llvm::Value *result_address
)
{
	bz_assert(if_expr.condition.is<ast::constant_expression>());
	auto const &condition_value = if_expr.condition.get<ast::constant_expression>().value;
	bz_assert(condition_value.is<ast::constant_value::boolean>());
	if (condition_value.get<ast::constant_value::boolean>())
	{
		return emit_bitcode<abi>(if_expr.then_block, context, result_address);
	}
	else if (if_expr.else_block.not_null())
	{
		return emit_bitcode<abi>(if_expr.else_block, context, result_address);
	}
	else
	{
		bz_assert(result_address == nullptr);
		return val_ptr::get_none();
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_switch const &switch_expr,
	Context &context,
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
			bz_assert(expr.template is<ast::constant_expression>());
			auto const &const_expr = expr.template get<ast::constant_expression>();
			auto const val = get_value<abi>(const_expr.value, const_expr.type, &const_expr, context);
			bz_assert(val != nullptr && llvm::dyn_cast<llvm::ConstantInt>(val) != nullptr);
			auto const const_int_val = static_cast<llvm::ConstantInt *>(val);
			switch_inst->addCase(const_int_val, bb);
		}
		context.builder.SetInsertPoint(bb);
		auto const case_val = emit_bitcode<abi>(case_expr, context, result_address);
		case_result_vals.push_back({ context.builder.GetInsertBlock(), case_val });
	}
	auto const end_bb = has_default ? context.add_basic_block("switch_end") : default_bb;
	auto const has_value = case_result_vals.is_all([&](auto const &pair) {
		return context.has_terminator(pair.first) || pair.second.val != nullptr || pair.second.consteval_val != nullptr;
	});
	if (result_address == nullptr && has_default && has_value)
	{
		auto const is_all_ref = case_result_vals.is_all([&](auto const &pair) {
			return context.has_terminator(pair.first) || (pair.second.val != nullptr && pair.second.kind == val_ptr::reference);
		});
		context.builder.SetInsertPoint(end_bb);
		bz_assert(case_result_vals.not_empty());
		auto const result_type = case_result_vals.front().second.get_type();
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
			? val_ptr::get_reference(phi, result_type)
			: val_ptr::get_value(phi);
	}
	else if (has_default && has_value)
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

		bz_assert(result_address != nullptr);
		bz_assert(case_result_vals.not_empty());
		auto const result_type = case_result_vals.front().second.get_type();
		return val_ptr::get_reference(result_address, result_type);
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
		bz_assert(result_address == nullptr);
		return val_ptr::get_none();
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_break const &,
	Context &context,
	llvm::Value *
)
{
	if constexpr (is_comptime<Context>)
	{
		if (context.loop_info.break_bb == nullptr)
		{
			emit_error(src_tokens, "'break' hit in compile time execution without an outer loop", context);
			return val_ptr::get_none();
		}
	}

	bz_assert(context.loop_info.break_bb != nullptr);
	context.emit_loop_destructor_calls();
	context.emit_loop_end_lifetime_calls();
	bz_assert(!context.has_terminator());
	context.builder.CreateBr(context.loop_info.break_bb);
	return val_ptr::get_none();
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::expr_continue const &,
	Context &context,
	llvm::Value *
)
{
	if constexpr (is_comptime<Context>)
	{
		if (context.loop_info.continue_bb == nullptr)
		{
			emit_error(src_tokens, "'continue' hit in compile time execution without an outer loop", context);
			return val_ptr::get_none();
		}
	}

	bz_assert(context.loop_info.continue_bb != nullptr);
	context.emit_loop_destructor_calls();
	context.emit_loop_end_lifetime_calls();
	bz_assert(!context.has_terminator());
	context.builder.CreateBr(context.loop_info.continue_bb);
	return val_ptr::get_none();
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &,
	ast::expr_generic_type_instantiation const &,
	Context &,
	llvm::Value *
)
{
	bz_unreachable;
}

template<abi::platform_abi abi, typename Context>
static llvm::Constant *get_value(
	ast::constant_value const &value,
	ast::typespec_view type,
	ast::constant_expression const *const_expr,
	Context &context
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
		auto const string_type = llvm::ArrayType::get(context.get_uint8_t(), str.size() + 1);

		auto const begin_ptr = context.create_struct_gep(string_type, string_constant, 0);
		auto const const_begin_ptr = llvm::dyn_cast<llvm::Constant>(begin_ptr);
		bz_assert(const_begin_ptr != nullptr);

		auto const end_ptr = context.create_struct_gep(string_type, string_constant, str.size());
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
		return context.get_function(&decl->body);
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
				return get_value<abi>(pair.first, pair.second->get_type(), nullptr, context);
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

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::constant_expression const &const_expr,
	Context &context,
	llvm::Value *result_address
)
{
	if (
		const_expr.kind == ast::expression_type_kind::type_name
		|| const_expr.kind == ast::expression_type_kind::none
	)
	{
		return val_ptr::get_none();
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
	val_ptr result = val_ptr::get_none();

	// consteval variable
	if (const_expr.kind == ast::expression_type_kind::lvalue)
	{
		result = const_expr.expr.visit([&](auto const &expr) {
			return emit_bitcode<abi>(src_tokens, expr, context, nullptr);
		});
	}
	else
	{
		result.kind = val_ptr::value;
	}

	if (result.val != nullptr && llvm::dyn_cast<llvm::GlobalVariable>(result.val) != nullptr)
	{
		auto const global_var = static_cast<llvm::GlobalVariable *>(result.val);
		bz_assert(global_var->hasInitializer());
		result.consteval_val = global_var->getInitializer();
	}
	else
	{
		auto const const_val = get_value<abi>(const_expr.value, const_expr.type, &const_expr, context);
		result.consteval_val = const_val;
		result.type = const_val->getType();
	}

	if (result_address == nullptr)
	{
		return result;
	}
	else
	{
		auto const result_val = result.get_value(context.builder);
		context.builder.CreateStore(result_val, result_address);
		return val_ptr::get_reference(result_address, result_val->getType());
	}
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	lex::src_tokens const &src_tokens,
	ast::dynamic_expression const &dyn_expr,
	Context &context,
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
		return emit_bitcode<abi>(src_tokens, expr, context, result_address);
	});
}

template<abi::platform_abi abi, typename Context>
static val_ptr emit_bitcode(
	ast::expression const &expr,
	Context &context,
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
		return emit_bitcode<abi>(expr.src_tokens, expr.get<ast::constant_expression>(), context, result_address);
	case ast::expression::index_of<ast::dynamic_expression>:
		return emit_bitcode<abi>(expr.src_tokens, expr.get<ast::dynamic_expression>(), context, result_address);
	case ast::expression::index_of<ast::error_expression>:
		if constexpr (is_comptime<Context>)
		{
			emit_error(expr.src_tokens, "failed to resolve expression", context);
		}
		else
		{
			bz_unreachable;
		}
		return val_ptr::get_none();

	default:
		if constexpr (is_comptime<Context>)
		{
			emit_error(expr.src_tokens, "failed to resolve expression", context);
		}
		else
		{
			bz_unreachable;
		}
		// we can safely return none here, because errors should have been propagated enough while parsing for this to not matter
		return val_ptr::get_none();
	}
}


// ================================================================
// -------------------------- statement ---------------------------
// ================================================================

template<abi::platform_abi abi, typename Context>
static void emit_bitcode(
	ast::stmt_while const &while_stmt,
	Context &context
)
{
	auto const condition_check_bb = context.add_basic_block("while_condition_check");
	auto const end_bb = context.add_basic_block("endwhile");
	auto const prev_loop_info = context.push_loop(end_bb, condition_check_bb);
	context.builder.CreateBr(condition_check_bb);
	context.builder.SetInsertPoint(condition_check_bb);
	context.push_expression_scope();
	auto const condition = emit_bitcode<abi>(while_stmt.condition, context, nullptr).get_value(context.builder);
	context.pop_expression_scope();
	auto const condition_check_end = context.builder.GetInsertBlock();

	auto const while_bb = context.add_basic_block("while");
	context.builder.SetInsertPoint(while_bb);
	context.push_expression_scope();
	emit_bitcode<abi>(while_stmt.while_block, context, nullptr);
	context.pop_expression_scope();
	if (!context.has_terminator())
	{
		context.builder.CreateBr(condition_check_bb);
	}

	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition == nullptr ? llvm::ConstantInt::getFalse(context.get_llvm_context()) : condition, while_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
	context.pop_loop(prev_loop_info);
}

template<abi::platform_abi abi, typename Context>
static void emit_bitcode(
	ast::stmt_for const &for_stmt,
	Context &context
)
{
	context.push_expression_scope();
	if (for_stmt.init.not_null())
	{
		emit_bitcode<abi>(for_stmt.init, context);
	}
	auto const condition_check_bb = context.add_basic_block("for_condition_check");
	auto const iteration_bb = context.add_basic_block("for_iteration");
	auto const end_bb = context.add_basic_block("endfor");
	auto const prev_loop_info = context.push_loop(end_bb, iteration_bb);

	context.builder.CreateBr(condition_check_bb);
	context.builder.SetInsertPoint(condition_check_bb);
	context.push_expression_scope();
	auto const condition = for_stmt.condition.not_null()
		? emit_bitcode<abi>(for_stmt.condition, context, nullptr).get_value(context.builder)
		: llvm::ConstantInt::getTrue(context.get_llvm_context());
	context.pop_expression_scope();
	auto const condition_check_end = context.builder.GetInsertBlock();

	context.builder.SetInsertPoint(iteration_bb);
	if (for_stmt.iteration.not_null())
	{
		context.push_expression_scope();
		emit_bitcode<abi>(for_stmt.iteration, context, nullptr);
		context.pop_expression_scope();
	}
	if (!context.has_terminator())
	{
		context.builder.CreateBr(condition_check_bb);
	}

	auto const for_bb = context.add_basic_block("for");
	context.builder.SetInsertPoint(for_bb);
	context.push_expression_scope();
	emit_bitcode<abi>(for_stmt.for_block, context, nullptr);
	context.pop_expression_scope();
	if (!context.has_terminator())
	{
		context.builder.CreateBr(iteration_bb);
	}

	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition == nullptr ? llvm::ConstantInt::getFalse(context.get_llvm_context()) : condition, for_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
	context.pop_loop(prev_loop_info);
	context.pop_expression_scope();
}

template<abi::platform_abi abi, typename Context>
static void emit_bitcode(
	ast::stmt_foreach const &foreach_stmt,
	Context &context
)
{
	context.push_expression_scope();
	emit_bitcode<abi>(foreach_stmt.range_var_decl, context);
	emit_bitcode<abi>(foreach_stmt.iter_var_decl, context);
	emit_bitcode<abi>(foreach_stmt.end_var_decl, context);

	auto const condition_check_bb = context.add_basic_block("foreach_condition_check");
	auto const iteration_bb = context.add_basic_block("foreach_iteration");
	auto const end_bb = context.add_basic_block("endforeach");
	auto const prev_loop_info = context.push_loop(end_bb, iteration_bb);

	context.builder.CreateBr(condition_check_bb);
	context.builder.SetInsertPoint(condition_check_bb);
	auto const condition = emit_bitcode<abi>(foreach_stmt.condition, context, nullptr).get_value(context.builder);
	auto const condition_check_end = context.builder.GetInsertBlock();

	context.builder.SetInsertPoint(iteration_bb);
	emit_bitcode<abi>(foreach_stmt.iteration, context, nullptr);
	bz_assert(!context.has_terminator());
	context.builder.CreateBr(condition_check_bb);

	auto const foreach_bb = context.add_basic_block("foreach");
	context.builder.SetInsertPoint(foreach_bb);
	context.push_expression_scope();
	emit_bitcode<abi>(foreach_stmt.iter_deref_var_decl, context);
	context.push_expression_scope();
	emit_bitcode<abi>(foreach_stmt.for_block, context, nullptr);
	context.pop_expression_scope();
	if (!context.has_terminator())
	{
		context.builder.CreateBr(iteration_bb);
	}
	context.pop_expression_scope();

	context.builder.SetInsertPoint(condition_check_end);
	context.builder.CreateCondBr(condition, foreach_bb, end_bb);
	context.builder.SetInsertPoint(end_bb);
	context.pop_loop(prev_loop_info);
	context.pop_expression_scope();
}

template<abi::platform_abi abi, typename Context>
static void emit_bitcode(
	ast::stmt_return const &ret_stmt,
	Context &context
)
{
	if constexpr (is_comptime<Context>)
	{
		if (context.current_function.first == nullptr)
		{
			bz_assert(ret_stmt.return_pos != nullptr);
			auto const src_tokens = ret_stmt.expr.is_null()
				? lex::src_tokens::from_single_token(ret_stmt.return_pos)
				: ret_stmt.expr.src_tokens;
			// we are in a comptime compound expression here
			emit_error(
				src_tokens,
				"return statement is not allowed in compile time evaluation of compound expression",
				context
			);
			auto const ret_type = context.current_function.second->getReturnType();
			if (ret_type->isVoidTy())
			{
				context.builder.CreateRetVoid();
			}
			else
			{
				context.builder.CreateRet(llvm::UndefValue::get(ret_type));
			}
			return;
		}
	}

	if (ret_stmt.expr.is_null())
	{
		context.emit_all_destructor_calls();
		context.emit_all_end_lifetime_calls();
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
		if constexpr (is_comptime<Context>)
		{
			emit_error(ret_stmt.expr.src_tokens, "failed to evaluate expression", context);
		}
		else
		{
			bz_unreachable;
		}
	}
	else
	{
		if (context.current_function.first->return_type.template is<ast::ts_lvalue_reference>())
		{
			auto const ret_val = emit_bitcode<abi>(ret_stmt.expr, context, context.output_pointer);
			context.emit_all_destructor_calls();
			context.emit_all_end_lifetime_calls();
			bz_assert(ret_val.kind == val_ptr::reference);
			context.builder.CreateRet(ret_val.val);
		}
		else if (context.output_pointer != nullptr)
		{
			emit_bitcode<abi>(ret_stmt.expr, context, context.output_pointer);
			context.emit_all_destructor_calls();
			context.emit_all_end_lifetime_calls();
			context.builder.CreateRetVoid();
		}
		else
		{
			auto const result_type = get_llvm_type(context.current_function.first->return_type, context);
			auto const ret_kind = context.template get_pass_kind<abi>(context.current_function.first->return_type, result_type);
			switch (ret_kind)
			{
			case abi::pass_kind::reference:
			case abi::pass_kind::non_trivial:
				bz_unreachable;
			case abi::pass_kind::value:
			{
				auto const ret_val = emit_bitcode<abi>(ret_stmt.expr, context, context.output_pointer).get_value(context.builder);
				context.emit_all_destructor_calls();
				context.emit_all_end_lifetime_calls();
				context.builder.CreateRet(ret_val);
				break;
			}
			case abi::pass_kind::one_register:
			case abi::pass_kind::two_registers:
			{
				auto const ret_type = context.current_function.second->getReturnType();
				auto const alloca = context.create_alloca(result_type);
				auto const result_ptr = context.builder.CreatePointerCast(alloca, llvm::PointerType::get(ret_type, 0));
				emit_bitcode<abi>(ret_stmt.expr, context, alloca);
				auto const result = context.create_load(ret_type, result_ptr);
				context.emit_all_destructor_calls();
				context.emit_all_end_lifetime_calls();
				context.builder.CreateRet(result);
				break;
			}
			}
		}
	}
}

template<abi::platform_abi abi, typename Context>
static void emit_bitcode(
	ast::stmt_no_op const &,
	Context &
)
{
	// we do nothing
	return;
}

template<abi::platform_abi abi, typename Context>
static void emit_bitcode(
	ast::stmt_expression const &expr_stmt,
	Context &context
)
{
	context.push_expression_scope();
	emit_bitcode<abi>(expr_stmt.expr, context, nullptr);
	context.pop_expression_scope();
}

template<typename Context>
static void add_variable_helper(
	ast::decl_variable const &var_decl,
	llvm::Value *ptr,
	llvm::Type *type,
	Context &context
)
{
	if (var_decl.tuple_decls.empty())
	{
		if (var_decl.get_type().is<ast::ts_lvalue_reference>())
		{
			context.add_variable(&var_decl, context.create_load(context.get_opaque_pointer_t(), ptr), type);
		}
		else
		{
			context.add_variable(&var_decl, ptr, type);
		}
	}
	else
	{
		bz_assert(type->isStructTy());
		for (auto const &[decl, i] : var_decl.tuple_decls.enumerate())
		{
			auto const gep_ptr = context.create_struct_gep(type, ptr, i);
			auto const elem_type = type->getStructElementType(i);
			add_variable_helper(decl, gep_ptr, elem_type, context);
		}
	}
}

template<abi::platform_abi abi, typename Context>
static void emit_bitcode(
	ast::decl_variable const &var_decl,
	Context &context
)
{
	if constexpr (is_comptime<Context>)
	{
		if (var_decl.get_type().is_empty())
		{
			emit_error(var_decl.src_tokens, "failed to resolve variable declaration", context);
			return;
		}
	}
	if (var_decl.get_type().is<ast::ts_lvalue_reference>())
	{
		bz_assert(var_decl.init_expr.not_null());
		auto const init_val = emit_bitcode<abi>(var_decl.init_expr, context, nullptr);
		bz_assert(init_val.kind == val_ptr::reference);
		if (var_decl.tuple_decls.empty())
		{
			context.add_variable(&var_decl, init_val.val, init_val.get_type());
		}
		else
		{
			add_variable_helper(var_decl, init_val.val, init_val.get_type(), context);
		}
	}
	else
	{
		auto const type = get_llvm_type(var_decl.get_type(), context);
		auto const alloca = context.create_alloca_without_lifetime_start(type);
		auto const size = context.get_size(type);
		context.start_lifetime(alloca, size);
		context.push_end_lifetime_call(alloca, size);
		push_destructor_call(var_decl.src_tokens, alloca, var_decl.get_type(), context);
		if (var_decl.init_expr.not_null())
		{
			context.push_expression_scope();
			emit_bitcode<abi>(var_decl.init_expr, context, alloca);
			context.pop_expression_scope();
		}
		else
		{
			emit_default_constructor<abi>(var_decl.src_tokens, var_decl.get_type(), context, alloca);
		}
		add_variable_helper(var_decl, alloca, type, context);
	}
}


template<abi::platform_abi abi, typename Context>
static void emit_bitcode(
	ast::statement const &stmt,
	Context &context
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

template<abi::platform_abi abi, typename Context>
static llvm::Function *create_function_from_symbol_impl(
	ast::function_body &func_body,
	Context &context
)
{
	if (func_body.is_bitcode_emitted())
	{
		return context.get_function(&func_body);
	}

	auto const result_t = get_llvm_type(func_body.return_type, context);
	auto const return_kind = context.template get_pass_kind<abi>(func_body.return_type, result_t);

	bz::vector<is_byval_and_type_pair> is_arg_byval = {};
	bz::vector<llvm::Type *> args = {};
	is_arg_byval.reserve(func_body.params.size());
	args.reserve(
		func_body.params.size()
		+ (return_kind == abi::pass_kind::reference || return_kind == abi::pass_kind::non_trivial ? 1 : 0)
	);

	if (return_kind == abi::pass_kind::reference || return_kind == abi::pass_kind::non_trivial)
	{
		args.push_back(llvm::PointerType::get(result_t, 0));
	}
	if (func_body.is_main())
	{
		auto const str_slice = context.get_slice_t(context.get_str_t());
		// str_slice is known to be not non_trivial
		auto const pass_kind = abi::get_pass_kind<abi>(str_slice, context.get_data_layout(), context.get_llvm_context());

		switch (pass_kind)
		{
		case abi::pass_kind::reference:
			is_arg_byval.push_back({ true, str_slice });
			args.push_back(llvm::PointerType::get(str_slice, 0));
			break;
		case abi::pass_kind::value:
			is_arg_byval.push_back({ false, nullptr });
			args.push_back(str_slice);
			break;
		case abi::pass_kind::one_register:
			is_arg_byval.push_back({ false, nullptr });
			args.push_back(abi::get_one_register_type<abi>(str_slice, context.get_data_layout(), context.get_llvm_context()));
			break;
		case abi::pass_kind::two_registers:
		{
			auto const [first_type, second_type] = abi::get_two_register_types<abi>(
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
			auto const pass_kind = context.template get_pass_kind<abi>(p.get_type(), t);

			switch (pass_kind)
			{
			case abi::pass_kind::reference:
				is_arg_byval.push_back({ true, t });
				args.push_back(llvm::PointerType::get(t, 0));
				break;
			case abi::pass_kind::value:
				is_arg_byval.push_back({ false, nullptr });
				args.push_back(t);
				break;
			case abi::pass_kind::one_register:
				is_arg_byval.push_back({ false, nullptr });
				args.push_back(abi::get_one_register_type<abi>(t, context.get_data_layout(), context.get_llvm_context()));
				break;
			case abi::pass_kind::two_registers:
			{
				auto const [first_type, second_type] = abi::get_two_register_types<abi>(
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
				args.push_back(llvm::PointerType::get(t, 0));
				break;
			}
		}
	}
	if (
		func_body.is_intrinsic()
		&& (
			func_body.intrinsic_kind == ast::function_body::memcpy
			|| func_body.intrinsic_kind == ast::function_body::memmove
			|| func_body.intrinsic_kind == ast::function_body::memset
			|| func_body.intrinsic_kind == ast::function_body::clz_u8
			|| func_body.intrinsic_kind == ast::function_body::clz_u16
			|| func_body.intrinsic_kind == ast::function_body::clz_u32
			|| func_body.intrinsic_kind == ast::function_body::clz_u64
			|| func_body.intrinsic_kind == ast::function_body::ctz_u8
			|| func_body.intrinsic_kind == ast::function_body::ctz_u16
			|| func_body.intrinsic_kind == ast::function_body::ctz_u32
			|| func_body.intrinsic_kind == ast::function_body::ctz_u64
		)
	)
	{
		args.push_back(context.get_bool_t());
		is_arg_byval.push_back({ false, nullptr });
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
				auto const [first_type, second_type] = abi::get_two_register_types<abi>(
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
	auto const name = !is_comptime<Context> && func_body.is_main()
		? llvm::StringRef("__bozon_main")
		: llvm::StringRef(func_body.symbol_name.data_as_char_ptr(), func_body.symbol_name.size());

	auto const linkage = is_comptime<Context> || func_body.is_external_linkage()
		? llvm::Function::ExternalLinkage
		: llvm::Function::InternalLinkage;

	auto const fn = llvm::Function::Create(
		func_t, linkage,
		name, context.get_module()
	);

	switch (func_body.cc)
	{
	static_assert(static_cast<size_t>(abi::calling_convention::_last) == 3);
	case abi::calling_convention::c:
		fn->setCallingConv(llvm::CallingConv::C);
		break;
	case abi::calling_convention::fast:
		fn->setCallingConv(llvm::CallingConv::Fast);
		break;
	case abi::calling_convention::std:
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
		arg_it->addAttr(llvm::Attribute::NoCapture);
		arg_it->addAttr(llvm::Attribute::NonNull);
		++arg_it;
	}

	for (; is_byval_it != is_byval_end; ++is_byval_it, ++arg_it)
	{
		if (is_byval_it->is_byval)
		{
			add_byval_attributes<abi>(*arg_it, is_byval_it->type, context);
		}
	}
	return fn;
}

template<typename Context>
static llvm::Function *create_function_from_symbol(
	ast::function_body &func_body,
	Context &context
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

void add_function_to_module(
	ast::function_body *func_body,
	ctx::bitcode_context &context
)
{
	auto const fn = create_function_from_symbol(*func_body, context);
	context.funcs_[func_body] = fn;
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
	ctx::bitcode_context &context
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

	context.push_expression_scope();
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
				bz_assert(p.init_expr.is<ast::constant_expression>());
				auto const &const_expr = p.init_expr.get<ast::constant_expression>();
				auto const val = get_value<abi>(const_expr.value, const_expr.type, &const_expr, context);
				auto const alloca = context.create_alloca_without_lifetime_start(val->getType());
				auto const size = context.get_size(val->getType());
				context.start_lifetime(alloca, size);
				context.push_end_lifetime_call(alloca, size);
				context.builder.CreateStore(val, alloca);
				add_variable_helper(p, alloca, val->getType(), context);
				++p_it;
				continue;
			}
			if (p.get_type().is<ast::ts_lvalue_reference>() || p.get_type().is<ast::ts_move_reference>())
			{
				bz_assert(fn_it->getType()->isPointerTy());
				if (p.tuple_decls.empty())
				{
					auto const type = ast::remove_lvalue_or_move_reference(p.get_type());
					context.add_variable(&p, fn_it, get_llvm_type(type, context));
				}
				else
				{
					auto const type = ast::remove_lvalue_or_move_reference(p.get_type());
					add_variable_helper(p, fn_it, get_llvm_type(type, context), context);
				}
			}
			else
			{
				auto const t = get_llvm_type(p.get_type(), context);
				auto const pass_kind = context.template get_pass_kind<abi>(p.get_type(), t);
				switch (pass_kind)
				{
				case abi::pass_kind::reference:
				case abi::pass_kind::non_trivial:
					push_destructor_call(p.src_tokens, fn_it, p.get_type(), context);
					add_variable_helper(p, fn_it, t, context);
					break;
				case abi::pass_kind::value:
				{
					auto const alloca = context.create_alloca_without_lifetime_start(t);
					auto const size = context.get_size(t);
					context.start_lifetime(alloca, size);
					context.builder.CreateStore(fn_it, alloca);
					context.push_end_lifetime_call(alloca, size);
					push_destructor_call(p.src_tokens, alloca, p.get_type(), context);
					add_variable_helper(p, alloca, t, context);
					break;
				}
				case abi::pass_kind::one_register:
				{
					auto const alloca = context.create_alloca_without_lifetime_start(t);
					auto const size = context.get_size(t);
					context.start_lifetime(alloca, size);
					auto const alloca_cast = context.builder.CreatePointerCast(alloca, llvm::PointerType::get(fn_it->getType(), 0));
					context.builder.CreateStore(fn_it, alloca_cast);
					context.push_end_lifetime_call(alloca, size);
					push_destructor_call(p.src_tokens, alloca, p.get_type(), context);
					add_variable_helper(p, alloca, t, context);
					break;
				}
				case abi::pass_kind::two_registers:
				{
					auto const alloca = context.create_alloca_without_lifetime_start(t);
					auto const size = context.get_size(t);
					context.start_lifetime(alloca, size);
					auto const first_val = fn_it;
					auto const first_type = fn_it->getType();
					++fn_it;
					auto const second_val = fn_it;
					auto const second_type = fn_it->getType();
					auto const struct_type = llvm::StructType::get(first_type, second_type);
					auto const alloca_cast = context.builder.CreatePointerCast(
						alloca, llvm::PointerType::get(struct_type, 0)
					);
					auto const first_address  = context.create_struct_gep(struct_type, alloca_cast, 0);
					auto const second_address = context.create_struct_gep(struct_type, alloca_cast, 1);
					context.builder.CreateStore(first_val, first_address);
					context.builder.CreateStore(second_val, second_address);
					context.push_end_lifetime_call(alloca, size);
					push_destructor_call(p.src_tokens, alloca, p.get_type(), context);
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
		emit_bitcode<abi>(stmt, context);
	}
	context.pop_expression_scope();

	if (!context.has_terminator())
	{
		if (context.current_function.first->is_main())
		{
			context.builder.CreateRet(llvm::ConstantInt::get(context.get_int32_t(), 0));
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
}

template<abi::platform_abi abi>
static void emit_function_bitcode_impl(
	ast::function_body &func_body,
	ctx::comptime_executor_context &context
)
{
	bz_assert(!func_body.is_comptime_bitcode_emitted());
	func_body.flags |= ast::function_body::comptime_bitcode_emitted;

	auto [module, fn] = context.get_module_and_function(&func_body);
	auto const prev_module = context.push_module(module.get());
	bz_assert(fn != nullptr);
	bz_assert(fn->size() == 0);

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

	context.push_expression_scope();
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
				bz_assert(p.init_expr.is<ast::constant_expression>());
				auto const &const_expr = p.init_expr.get<ast::constant_expression>();
				auto const val = get_value<abi>(const_expr.value, const_expr.type, &const_expr, context);
				auto const alloca = context.create_alloca_without_lifetime_start(val->getType());
				auto const size = context.get_size(val->getType());
				context.start_lifetime(alloca, size);
				context.push_end_lifetime_call(alloca, size);
				context.builder.CreateStore(val, alloca);
				add_variable_helper(p, alloca, val->getType(), context);
				++p_it;
				continue;
			}
			if (p.get_type().is<ast::ts_lvalue_reference>() || p.get_type().is<ast::ts_move_reference>())
			{
				bz_assert(fn_it->getType()->isPointerTy());
				if (p.tuple_decls.empty())
				{
					auto const type = ast::remove_lvalue_or_move_reference(p.get_type());
					context.add_variable(&p, fn_it, get_llvm_type(type, context));
				}
				else
				{
					auto const type = ast::remove_lvalue_or_move_reference(p.get_type());
					add_variable_helper(p, fn_it, get_llvm_type(type, context), context);
				}
			}
			else
			{
				auto const t = get_llvm_type(p.get_type(), context);
				auto const pass_kind = context.template get_pass_kind<abi>(p.get_type(), t);
				switch (pass_kind)
				{
				case abi::pass_kind::reference:
				case abi::pass_kind::non_trivial:
					push_destructor_call(p.src_tokens, fn_it, p.get_type(), context);
					add_variable_helper(p, fn_it, t, context);
					break;
				case abi::pass_kind::value:
				{
					auto const alloca = context.create_alloca_without_lifetime_start(t);
					auto const size = context.get_size(t);
					context.start_lifetime(alloca, size);
					context.builder.CreateStore(fn_it, alloca);
					context.push_end_lifetime_call(alloca, size);
					push_destructor_call(p.src_tokens, alloca, p.get_type(), context);
					add_variable_helper(p, alloca, t, context);
					break;
				}
				case abi::pass_kind::one_register:
				{
					auto const alloca = context.create_alloca_without_lifetime_start(t);
					auto const size = context.get_size(t);
					context.start_lifetime(alloca, size);
					auto const alloca_cast = context.builder.CreatePointerCast(alloca, llvm::PointerType::get(fn_it->getType(), 0));
					context.builder.CreateStore(fn_it, alloca_cast);
					context.push_end_lifetime_call(alloca, size);
					push_destructor_call(p.src_tokens, alloca, p.get_type(), context);
					add_variable_helper(p, alloca, t, context);
					break;
				}
				case abi::pass_kind::two_registers:
				{
					auto const alloca = context.create_alloca_without_lifetime_start(t);
					auto const size = context.get_size(t);
					context.start_lifetime(alloca, size);
					auto const first_val = fn_it;
					auto const first_type = fn_it->getType();
					++fn_it;
					auto const second_val = fn_it;
					auto const second_type = fn_it->getType();
					auto const struct_type = llvm::StructType::get(first_type, second_type);
					auto const alloca_cast = context.builder.CreatePointerCast(
						alloca,
						llvm::PointerType::get(struct_type, 0)
					);
					auto const first_address  = context.create_struct_gep(struct_type, alloca_cast, 0);
					auto const second_address = context.create_struct_gep(struct_type, alloca_cast, 1);
					context.builder.CreateStore(first_val, first_address);
					context.builder.CreateStore(second_val, second_address);
					context.push_end_lifetime_call(alloca, size);
					push_destructor_call(p.src_tokens, alloca, p.get_type(), context);
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
		emit_bitcode<abi>(stmt, context);
	}
	context.pop_expression_scope();

	if (!context.has_terminator())
	{
		if (context.current_function.first->is_main())
		{
			context.builder.CreateRet(llvm::ConstantInt::get(context.get_int32_t(), 0));
		}
		else if (auto const ret_t = context.current_function.second->getReturnType(); ret_t->isVoidTy())
		{
			context.builder.CreateRetVoid();
		}
		else
		{
			emit_error(func_body.src_tokens, "end of function reached without a return statement", context);
			bz_assert(!context.has_terminator());
			context.builder.CreateRet(llvm::UndefValue::get(ret_t));
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
	context.pop_module(prev_module);
	context.add_module(std::move(module));
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
	case abi::platform_abi::systemv_amd64:
		emit_function_bitcode_impl<abi::platform_abi::systemv_amd64>(
			func_body, context
		);
		return;
	}
	bz_unreachable;
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

template<abi::platform_abi abi, typename Context>
static void emit_global_variable_impl(ast::decl_variable const &var_decl, Context &context)
{
	auto const name = var_decl.get_id().format_for_symbol();
	auto const name_ref = llvm::StringRef(name.data_as_char_ptr(), name.size());
	auto const type = get_llvm_type(var_decl.get_type(), context);
	auto const val = context.get_module().getOrInsertGlobal(name_ref, type);
	bz_assert(llvm::dyn_cast<llvm::GlobalVariable>(val) != nullptr);
	auto const global_var = static_cast<llvm::GlobalVariable *>(val);
	if (is_comptime<Context> || var_decl.is_external_linkage())
	{
		global_var->setLinkage(llvm::GlobalValue::ExternalLinkage);
	}
	else
	{
		global_var->setLinkage(llvm::GlobalValue::InternalLinkage);
	}
	bz_assert(var_decl.init_expr.is<ast::constant_expression>());
	auto const &const_expr = var_decl.init_expr.get<ast::constant_expression>();
	auto const init_val = get_value<abi>(const_expr.value, const_expr.type, &const_expr, context);
	global_var->setInitializer(init_val);
	context.add_variable(&var_decl, global_var, type);
}

void emit_global_variable(ast::decl_variable const &var_decl, ctx::bitcode_context &context)
{
	if (context.vars_.find(&var_decl) != context.vars_.end() || var_decl.is_no_runtime_emit())
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

void emit_global_type_symbol(ast::type_info const &info, ctx::bitcode_context &context)
{
	if (context.types_.find(&info) != context.types_.end())
	{
		return;
	}

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
	case ast::type_info::forward_declaration:
	case ast::type_info::aggregate:
	{
		auto const name = llvm::StringRef(info.symbol_name.data_as_char_ptr(), info.symbol_name.size());
		context.add_base_type(&info, llvm::StructType::create(context.get_llvm_context(), name));
		break;
	}
	default:
		bz_unreachable;
	}

	if (info.body.is<bz::vector<ast::statement>>())
	{
		for (
			auto const &inner_struct_decl :
			info.body.get<bz::vector<ast::statement>>()
				.filter([](auto const &stmt) { return stmt.template is<ast::decl_struct>(); })
				.transform([](auto const &stmt) -> auto const & { return stmt.template get<ast::decl_struct>(); })
		)
		{
			emit_global_type_symbol(inner_struct_decl.info, context);
		}
	}
}

void emit_global_type(ast::type_info const &info, ctx::bitcode_context &context)
{
	if (info.is_generic())
	{
		for (auto const &instantiation : info.generic_instantiations)
		{
			emit_global_type(*instantiation, context);
		}
		return;
	}

	auto const type = context.get_base_type(&info);
	bz_assert(type != nullptr);
	bz_assert(type->isStructTy());
	auto const struct_type = static_cast<llvm::StructType *>(type);
	switch (info.kind)
	{
	case ast::type_info::forward_declaration:
		// there's nothing to do
		return;
	case ast::type_info::aggregate:
	{
		auto const types = info.member_variables
			.transform([&](auto const &member) { return get_llvm_type(member->get_type(), context); })
			.collect<ast::arena_vector>();
		struct_type->setBody(llvm::ArrayRef<llvm::Type *>(types.data(), types.size()));
		break;
	}
	default:
		bz_unreachable;
	}

	if (info.body.is<bz::vector<ast::statement>>())
	{
		for (
			auto const &inner_struct_decl :
			info.body.get<bz::vector<ast::statement>>()
				.filter([](auto const &stmt) { return stmt.template is<ast::decl_struct>(); })
				.transform([](auto const &stmt) -> auto const & { return stmt.template get<ast::decl_struct>(); })
		)
		{
			emit_global_type(inner_struct_decl.info, context);
		}
	}
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
			.transform([&](auto const &member) { return get_llvm_type(member->get_type(), context); })
			.collect();
		struct_type->setBody(llvm::ArrayRef<llvm::Type *>(types.data(), types.size()));
		break;
	}
	default:
		bz_unreachable;
	}
}

void emit_necessary_functions(ctx::bitcode_context &context)
{
	auto const abi = context.get_platform_abi();
	switch (abi)
	{
	case abi::platform_abi::generic:
		for (size_t i = 0; i < context.functions_to_compile.size(); ++i)
		{
			auto const func_body = context.functions_to_compile[i];
			if (func_body->is_bitcode_emitted())
			{
				continue;
			}
			emit_function_bitcode_impl<abi::platform_abi::generic>(*func_body, context);
		}
		return;
	case abi::platform_abi::microsoft_x64:
		for (size_t i = 0; i < context.functions_to_compile.size(); ++i)
		{
			auto const func_body = context.functions_to_compile[i];
			if (func_body->is_bitcode_emitted())
			{
				continue;
			}
			emit_function_bitcode_impl<abi::platform_abi::microsoft_x64>(*func_body, context);
		}
		return;
	case abi::platform_abi::systemv_amd64:
		for (size_t i = 0; i < context.functions_to_compile.size(); ++i)
		{
			auto const func_body = context.functions_to_compile[i];
			if (func_body->is_bitcode_emitted())
			{
				continue;
			}
			emit_function_bitcode_impl<abi::platform_abi::systemv_amd64>(*func_body, context);
		}
		return;
	}
	bz_unreachable;
}

bool emit_necessary_functions(size_t start_index, ctx::comptime_executor_context &context)
{
	auto const abi = context.get_platform_abi();
	switch (abi)
	{
	case abi::platform_abi::generic:
		for (size_t i = start_index; i < context.functions_to_compile.size(); ++i)
		{
			auto const body = context.functions_to_compile[i];
			if (body->is_comptime_bitcode_emitted())
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
	{
		for (size_t i = start_index; i < context.functions_to_compile.size(); ++i)
		{
			auto const body = context.functions_to_compile[i];
			if (body->is_comptime_bitcode_emitted())
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
	}
	case abi::platform_abi::systemv_amd64:
		for (size_t i = start_index; i < context.functions_to_compile.size(); ++i)
		{
			auto const body = context.functions_to_compile[i];
			if (body->is_comptime_bitcode_emitted())
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
	llvm::Type *global_value_type,
	llvm::Type *type,
	bz::vector<std::uint32_t> &gep_indices,
	ctx::comptime_executor_context &context
)
{
	switch (type->getTypeID())
	{
	case llvm::Type::StructTyID:
	{
		auto const struct_type = static_cast<llvm::StructType *>(type);
		gep_indices.push_back(0);
		for (auto const elem_type : struct_type->elements())
		{
			add_global_result_getters<abi>(getters, global_value_ptr, global_value_type, elem_type, gep_indices, context);
			gep_indices.back() += 1;
		}
		gep_indices.pop_back();
		break;
	}
	case llvm::Type::ArrayTyID:
	{
		auto const array_type = static_cast<llvm::ArrayType *>(type);
		gep_indices.push_back(0);
		auto const elem_type = array_type->getElementType();
		for ([[maybe_unused]] auto const _ : bz::iota(0, array_type->getNumElements()))
		{
			add_global_result_getters<abi>(getters, global_value_ptr, global_value_type, elem_type, gep_indices, context);
			gep_indices.back() += 1;
		}
		gep_indices.pop_back();
		break;
	}
	default:
	{
		auto const func_type = llvm::FunctionType::get(type, false);
		auto const symbol_name = bz::format("__global_result_getter.{}", get_unique_id());
		auto const symbol_name_ref = llvm::StringRef(symbol_name.data_as_char_ptr(), symbol_name.size());
		auto const func = llvm::Function::Create(
			func_type, llvm::Function::InternalLinkage,
			symbol_name_ref,
			&context.get_module()
		);
		getters.push_back(func);
		auto const bb = llvm::BasicBlock::Create(
			context.get_llvm_context(),
			"entry",
			func
		);
		auto const indices = gep_indices
			.transform([&](auto const i) -> llvm::Value * { return llvm::ConstantInt::get(context.get_uint32_t(), i); })
			.collect();
		auto const prev_bb = context.builder.GetInsertBlock();

		context.builder.SetInsertPoint(bb);
		auto const ptr = context.create_gep(global_value_type, global_value_ptr, indices);
		auto const result_val = context.create_load(type, ptr);
		context.builder.CreateRet(result_val);

		context.builder.SetInsertPoint(prev_bb);
		break;
	}
	}
}

template<abi::platform_abi abi>
static std::pair<llvm::Function *, bz::vector<llvm::Function *>> create_function_for_comptime_execution_impl(
	ast::function_body *body,
	bz::array_view<ast::expression const> params,
	ctx::comptime_executor_context &context
)
{
	bz_assert(!body->has_builtin_implementation());
	auto const called_fn = context.get_function(body);
	bz_assert(called_fn != nullptr);

	auto const result_type = get_llvm_type(body->return_type, context);
	auto const void_type = llvm::Type::getVoidTy(context.get_llvm_context());
	auto const return_result_as_global = result_type->isAggregateType();

	auto const result_func_type = llvm::FunctionType::get(return_result_as_global ? void_type : result_type, false);
	std::pair<llvm::Function *, bz::vector<llvm::Function *>> result;
	auto const symbol_name = bz::format("__anon_comptime_func_call.{}", get_unique_id());
	auto const symbol_name_ref = llvm::StringRef(symbol_name.data_as_char_ptr(), symbol_name.size());
	result.first = llvm::Function::Create(
		result_func_type,
		llvm::Function::InternalLinkage,
		symbol_name_ref,
		&context.get_module()
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

	auto const result_kind = abi::get_pass_kind<abi>(result_type, context.get_data_layout(), context.get_llvm_context());

	ast::arena_vector<llvm::Value *> args;
	ast::arena_vector<is_byval_and_type_pair> args_is_byval;
	args.reserve(params.size() + (result_kind == abi::pass_kind::reference ? 1 : 0));
	args_is_byval.reserve(params.size() + (result_kind == abi::pass_kind::reference ? 1 : 0));

	if (result_kind == abi::pass_kind::reference)
	{
		auto const output_ptr = context.create_alloca_without_lifetime_start(result_type);
		args.push_back(output_ptr);
		args_is_byval.push_back({ false, nullptr });
	}

	context.push_expression_scope();

	for (auto const &[value, i] : params.enumerate())
	{
		if (ast::is_generic_parameter(body->params[i]))
		{
			continue;
		}
		auto const param_t = body->params[i].get_type().as_typespec_view();
		auto const param_type = get_llvm_type(param_t, context);
		auto const param_val = emit_bitcode<abi>(value, context, nullptr);

		add_call_parameter<abi>(param_t, param_type, param_val, args, args_is_byval, context);
	}

	auto const call = context.builder.CreateCall(called_fn, llvm::ArrayRef(args.data(), args.size()));
	call->setCallingConv(called_fn->getCallingConv());

	auto is_byval_it = args_is_byval.begin();
	auto const is_byval_end = args_is_byval.end();
	unsigned i = 0;

	bz_assert(called_fn->arg_size() == call->arg_size());
	if (result_kind == abi::pass_kind::reference)
	{
		call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context.get_llvm_context(), result_type));
		bz_assert(is_byval_it != is_byval_end);
		++is_byval_it, ++i;
	}
	for (; is_byval_it != is_byval_end; ++is_byval_it, ++i)
	{
		if (is_byval_it->is_byval)
		{
			add_byval_attributes<abi>(call, is_byval_it->type, i, context);
		}
	}

	auto const no_leaks = context.builder.CreateCall(
		context.get_comptime_function(ctx::comptime_function_kind::check_leaks),
		{}
	);
	emit_error_assert(no_leaks, context);

	if (body->return_type.is<ast::ts_array_slice>())
	{
		emit_error(body->src_tokens, "an array slice cannot be returned from compile time execution", context);
		bz_assert(!context.has_terminator());
		context.builder.CreateRet(llvm::UndefValue::get(result.first->getReturnType()));
	}
	else if (return_result_as_global && !result_type->isVoidTy())
	{
		auto const symbol_name = bz::format("__anon_func_call_result.{}", get_unique_id());
		auto const symbol_name_ref = llvm::StringRef(symbol_name.data_as_char_ptr(), symbol_name.size());
		auto const global_result = context.current_module->getOrInsertGlobal(symbol_name_ref, result_type);
		{
			bz_assert(llvm::dyn_cast<llvm::GlobalVariable>(global_result) != nullptr);
			static_cast<llvm::GlobalVariable *>(global_result)->setInitializer(llvm::UndefValue::get(result_type));
		}

		switch (result_kind)
		{
		case abi::pass_kind::reference:
			context.builder.CreateStore(context.create_load(result_type, args.front()), global_result);
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
		case abi::pass_kind::non_trivial:
			bz_unreachable;
		}
		context.builder.CreateRetVoid();
		bz::vector<uint32_t> gep_indices = { 0 };
		add_global_result_getters<abi>(result.second, global_result, result_type, result_type, gep_indices, context);
	}
	else
	{
		switch (result_kind)
		{
		case abi::pass_kind::reference:
			bz_unreachable;
		case abi::pass_kind::value:
			if (call->getType()->isVoidTy())
			{
				context.builder.CreateRetVoid();
			}
			else if (body->return_type.is<ast::ts_lvalue_reference>())
			{
				emit_error(body->src_tokens, "a reference cannot be returned from compile time execution", context);
				bz_assert(!context.has_terminator());
				context.builder.CreateRet(llvm::UndefValue::get(result.first->getReturnType()));
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
				auto const result_ptr = context.create_alloca_without_lifetime_start(result_type);
				auto const result_ptr_cast = context.builder.CreatePointerCast(
					result_ptr,
					llvm::PointerType::get(call_result_type, 0)
				);
				context.builder.CreateStore(call, result_ptr_cast);
				context.builder.CreateRet(context.create_load(call_result_type, result_ptr));
			}
			break;
		}
		case abi::pass_kind::non_trivial:
		{
			emit_error(body->src_tokens, "a non-trivial type cannot be returned from compile time execution", context);
			bz_assert(!context.has_terminator());
			context.builder.CreateRet(llvm::UndefValue::get(result.first->getReturnType()));
			break;
		}
		}
	}

	context.pop_expression_scope();

	context.builder.SetInsertPoint(alloca_bb);
	context.builder.CreateBr(entry_bb);

	context.current_function = {};
	context.alloca_bb = nullptr;
	context.error_bb = nullptr;
	context.output_pointer = nullptr;

	return result;
}

std::pair<llvm::Function *, bz::vector<llvm::Function *>> create_function_for_comptime_execution(
	ast::function_body *body,
	bz::array_view<ast::expression const> params,
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
	auto const result_type = expr.final_expr.is_null() || expr.final_expr.is_typename()
		? llvm::Type::getVoidTy(context.get_llvm_context())
		: get_llvm_type(expr.final_expr.get_expr_type_and_kind().first, context);
	auto const void_type = llvm::Type::getVoidTy(context.get_llvm_context());
	auto const return_result_as_global = result_type->isAggregateType();

	auto const func_t = llvm::FunctionType::get(return_result_as_global ? void_type : result_type, false);
	std::pair<llvm::Function *, bz::vector<llvm::Function *>> result;
	auto const symbol_name = expr.final_expr.src_tokens.pivot == nullptr
		? bz::format("__anon_comptime_compound_expr.null.{}", get_unique_id())
		: bz::format("__anon_comptime_compound_expr.{}.{}", expr.final_expr.src_tokens.pivot->src_pos.line, get_unique_id());
	auto const symbol_name_ref = llvm::StringRef(symbol_name.data_as_char_ptr(), symbol_name.size());
	result.first = llvm::Function::Create(
		func_t, llvm::Function::InternalLinkage,
		symbol_name_ref, &context.get_module()
	);
	context.current_function = { nullptr, result.first };
	auto const alloca_bb = context.add_basic_block("alloca");
	context.alloca_bb = alloca_bb;

	auto const error_bb = context.add_basic_block("error");
	context.error_bb = error_bb;
	context.builder.SetInsertPoint(error_bb);
	context.builder.CreateCall(
		context.get_comptime_function(ctx::comptime_function_kind::check_leaks),
		{}
	);
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

	// we push two scopes here, one for the compound expression, and one for the enclosing function
	context.push_expression_scope();
	context.push_expression_scope();
	for (auto &stmt : expr.statements)
	{
		emit_bitcode<abi>(stmt, context);
	}

	llvm::Value *ret_val = nullptr;
	if (expr.final_expr.is_null())
	{
		// nothing, return void
	}
	else if (!context.has_terminator())
	{
		if (return_result_as_global && !result_type->isVoidTy())
		{
			auto const symbol_name = bz::format("__anon_compound_expr_result.{}", get_unique_id());
			auto const symbol_name_ref = llvm::StringRef(symbol_name.data_as_char_ptr(), symbol_name.size());
			auto const global_result = context.current_module->getOrInsertGlobal(symbol_name_ref, result_type);
			{
				bz_assert(llvm::dyn_cast<llvm::GlobalVariable>(global_result) != nullptr);
				static_cast<llvm::GlobalVariable *>(global_result)->setInitializer(llvm::UndefValue::get(result_type));
			}

			emit_bitcode<abi>(expr.final_expr, context, global_result);
			// context.builder.CreateRetVoid();
			bz::vector<uint32_t> gep_indices = { 0 };
			add_global_result_getters<abi>(result.second, global_result, result_type, result_type, gep_indices, context);
		}
		else
		{
			auto const result_val = emit_bitcode<abi>(expr.final_expr, context, nullptr).get_value(context.builder);
			context.pop_expression_scope();
			// context.builder.CreateRet(result_val);
			ret_val = result_val;
		}
	}
	context.pop_expression_scope();

	auto const no_leaks = context.builder.CreateCall(
		context.get_comptime_function(ctx::comptime_function_kind::check_leaks),
		{}
	);
	emit_error_assert(no_leaks, context);

	if (ret_val == nullptr)
	{
		context.builder.CreateRetVoid();
	}
	else
	{
		context.builder.CreateRet(ret_val);
	}
	context.pop_expression_scope();

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

} // namespace bc
