#include "comptime_executor.h"
#include "global_context.h"
#include "parse_context.h"
#include "bc/comptime/comptime_emit_bitcode.h"
#include "parse/statement_parser.h"
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

namespace ctx
{

static bz::vector<comptime_function> create_empty_comptime_functions(void)
{
	bz::vector<comptime_function> result;
	result.resize(static_cast<uint32_t>(comptime_function_kind::_last) - static_cast<uint32_t>(comptime_function_kind::_first));
	for (auto const kind : bz::iota(0, static_cast<uint32_t>(comptime_function_kind::_last)))
	{
		result[kind] = { static_cast<comptime_function_kind>(kind), nullptr, nullptr };
	}
	return result;
}

comptime_executor_context::comptime_executor_context(global_context &_global_ctx)
	: global_ctx(_global_ctx),
	  builder(_global_ctx._llvm_context),
	  comptime_functions(create_empty_comptime_functions())
{}

comptime_executor_context::~comptime_executor_context(void)
{
	if (this->engine != nullptr)
	{
		this->engine->runFunction(this->get_comptime_function(comptime_function_kind::cleanup), {});
	}
}

ast::type_info *comptime_executor_context::get_builtin_type_info(uint32_t kind)
{
	return this->global_ctx.get_builtin_type_info(kind);
}

ast::typespec_view comptime_executor_context::get_builtin_type(bz::u8string_view name)
{
	return this->global_ctx.get_builtin_type(name);
}

ast::function_body *comptime_executor_context::get_builtin_function(uint32_t kind)
{
	return this->global_ctx.get_builtin_function(kind);
}

llvm::Value *comptime_executor_context::get_variable(ast::decl_variable const *var_decl) const
{
	auto const it = this->vars_.find(var_decl);
	return it == this->vars_.end() ? nullptr : it->second;
}

void comptime_executor_context::add_variable(ast::decl_variable const *var_decl, llvm::Value *val)
{
	this->vars_.insert_or_assign(var_decl, val);
}

llvm::Type *comptime_executor_context::get_base_type(ast::type_info *info)
{
	if (info->state != ast::resolve_state::all)
	{
		bz_assert(this->current_parse_ctx != nullptr);
		this->current_parse_ctx->add_to_resolve_queue({}, *info);
		parse::resolve_type_info(*info, *this->current_parse_ctx);
		this->current_parse_ctx->pop_resolve_queue();
	}
	auto const it = this->types_.find(info);
	if (it == this->types_.end())
	{
		auto const name = llvm::StringRef(info->symbol_name.data_as_char_ptr(), info->symbol_name.size());
		auto const type = llvm::StructType::create(this->get_llvm_context(), name);
		this->add_base_type(info, type);
		bc::comptime::resolve_global_type(info, type, *this);
		return type;
	}
	else
	{
		return it->second;
	}
}

void comptime_executor_context::add_base_type(ast::type_info const *info, llvm::Type *type)
{
	this->types_.insert_or_assign(info, type);
}

llvm::Function *comptime_executor_context::get_function(ast::function_body *func_body)
{
	this->ensure_function_emission(func_body);
	auto it = this->funcs_.find(func_body);
	if (it == this->funcs_.end())
	{
		bz_assert(func_body->state != ast::resolve_state::error);
		bz_assert(func_body->state >= ast::resolve_state::symbol);
		return bc::comptime::add_function_to_module(func_body, *this);
	}
	else
	{
		return it->second;
	}
}

llvm::LLVMContext &comptime_executor_context::get_llvm_context(void) const noexcept
{
	return this->global_ctx._llvm_context;
}

llvm::DataLayout &comptime_executor_context::get_data_layout(void) const noexcept
{
	return this->global_ctx._data_layout.get();
}

llvm::Module &comptime_executor_context::get_module(void) const noexcept
{
	return *this->current_module;
}

abi::platform_abi comptime_executor_context::get_platform_abi(void) const noexcept
{
	return this->global_ctx._platform_abi;
}

size_t comptime_executor_context::get_size(ast::typespec_view ts)
{
	auto const llvm_t = bc::comptime::get_llvm_type(ts, *this);
	return this->get_size(llvm_t);
}

size_t comptime_executor_context::get_align(ast::typespec_view ts)
{
	auto const llvm_t = bc::comptime::get_llvm_type(ts, *this);
	return this->get_align(llvm_t);
}

size_t comptime_executor_context::get_size(llvm::Type *t) const
{
	return this->global_ctx._data_layout->getTypeAllocSize(t);
}

size_t comptime_executor_context::get_align(llvm::Type *t) const
{
	return this->global_ctx._data_layout->getPrefTypeAlign(t).value();
#if LLVM_VERSION_MAJOR < 11
#error LLVM 11 is required
#endif // llvm < 11
}

size_t comptime_executor_context::get_offset(llvm::Type *t, size_t elem) const
{
	bz_assert(t->isStructTy());
	return this->global_ctx._data_layout->getStructLayout(static_cast<llvm::StructType *>(t))->getElementOffset(elem);
}


size_t comptime_executor_context::get_register_size(void) const
{
	switch (this->global_ctx._platform_abi)
	{
	case abi::platform_abi::generic:
	{
		static size_t register_size = this->global_ctx._data_layout->getLargestLegalIntTypeSizeInBits() / 8;
		return register_size;
	}
	case abi::platform_abi::microsoft_x64:
		bz_assert(this->global_ctx._data_layout->getLargestLegalIntTypeSizeInBits() == 64);
		return 8;
	case abi::platform_abi::systemv_amd64:
		bz_assert(this->global_ctx._data_layout->getLargestLegalIntTypeSizeInBits() == 64);
		return 8;
	}
	bz_unreachable;
}

llvm::BasicBlock *comptime_executor_context::add_basic_block(bz::u8string_view name)
{
	return llvm::BasicBlock::Create(
		this->global_ctx._llvm_context,
		llvm::StringRef(name.data(), name.length()),
		this->current_function.second
	);
}

llvm::Value *comptime_executor_context::create_alloca(llvm::Type *t)
{
	auto const bb = this->builder.GetInsertBlock();
	this->builder.SetInsertPoint(this->alloca_bb);
	auto const result = this->builder.CreateAlloca(t);
	this->builder.SetInsertPoint(bb);
	return result;
}

llvm::Value *comptime_executor_context::create_alloca(llvm::Type *t, size_t align)
{
	auto const bb = this->builder.GetInsertBlock();
	this->builder.SetInsertPoint(this->alloca_bb);
	auto const result = this->builder.CreateAlloca(t);
	result->setAlignment(llvm::Align(align));
	this->builder.SetInsertPoint(bb);
	return result;
}

llvm::Value *comptime_executor_context::create_string(bz::u8string_view str)
{
	auto const str_ref = llvm::StringRef(str.data(), str.size());
	return this->builder.CreateGlobalString(str_ref, ".str", 0, &this->get_module());
}

llvm::Value *comptime_executor_context::create_bitcast(bc::val_ptr val, llvm::Type *dest_type)
{
	if (val.kind == bc::val_ptr::reference)
	{
		auto const dest_ptr = this->builder.CreateBitCast(
			val.val, llvm::PointerType::get(dest_type, 0)
		);
		return this->builder.CreateLoad(dest_ptr);
	}
	else
	{
		auto const src_value = val.get_value(this->builder);
		auto const dest_ptr = this->create_alloca(dest_type);
		auto const cast_ptr = this->builder.CreateBitCast(
			dest_ptr, llvm::PointerType::get(val.get_type(), 0)
		);
		this->builder.CreateStore(src_value, cast_ptr);
		return this->builder.CreateLoad(dest_ptr);
	}
}

llvm::Value *comptime_executor_context::create_cast_to_int(bc::val_ptr val)
{
	auto const dest_type = [&]() -> llvm::Type * {
		auto const val_t = val.get_type();
		switch (this->get_size(val_t))
		{
		case 1:
			return this->get_int8_t();
		case 2:
			return this->get_int16_t();
		case 3:
			return llvm::IntegerType::getIntNTy(this->get_llvm_context(), 24);
		case 4:
			return this->get_int32_t();
		case 5:
			return llvm::IntegerType::getIntNTy(this->get_llvm_context(), 40);
		case 6:
			return llvm::IntegerType::getIntNTy(this->get_llvm_context(), 48);
		case 7:
			return llvm::IntegerType::getIntNTy(this->get_llvm_context(), 56);
		case 8:
			return this->get_int64_t();
		default:
			bz_unreachable;
		}
	}();
	return this->create_bitcast(val, dest_type);
}

llvm::Type *comptime_executor_context::get_builtin_type(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::null_t_);
	return this->global_ctx._llvm_builtin_types[kind];
}

llvm::Type *comptime_executor_context::get_int8_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int8_)]; }

llvm::Type *comptime_executor_context::get_int16_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int16_)]; }

llvm::Type *comptime_executor_context::get_int32_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int32_)]; }

llvm::Type *comptime_executor_context::get_int64_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int64_)]; }

llvm::Type *comptime_executor_context::get_uint8_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint8_)]; }

llvm::Type *comptime_executor_context::get_uint16_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint16_)]; }

llvm::Type *comptime_executor_context::get_uint32_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint32_)]; }

llvm::Type *comptime_executor_context::get_uint64_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint64_)]; }

llvm::Type *comptime_executor_context::get_float32_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::float32_)]; }

llvm::Type *comptime_executor_context::get_float64_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::float64_)]; }

llvm::Type *comptime_executor_context::get_str_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::str_)]; }

llvm::Type *comptime_executor_context::get_char_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::char_)]; }

llvm::Type *comptime_executor_context::get_bool_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::bool_)]; }

llvm::Type *comptime_executor_context::get_null_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::null_t_)]; }

llvm::StructType *comptime_executor_context::get_slice_t(llvm::Type *elem_type) const
{
	auto const elem_ptr_type = llvm::PointerType::get(elem_type, 0);
	return llvm::StructType::get(elem_ptr_type, elem_ptr_type);
}

llvm::Type *comptime_executor_context::get_usize_t(void) const
{
	switch (this->global_ctx.get_data_layout().getPointerSize())
	{
	case 8:
		return this->get_uint64_t();
	case 4:
		return this->get_uint32_t();
	case 2:
		return this->get_uint16_t();
	case 1:
		return this->get_uint8_t();
	default:
		bz_unreachable;
	}
}

llvm::Type *comptime_executor_context::get_isize_t(void) const
{
	switch (this->global_ctx.get_data_layout().getPointerSize())
	{
	case 8:
		return this->get_int64_t();
	case 4:
		return this->get_int32_t();
	case 2:
		return this->get_int16_t();
	case 1:
		return this->get_int8_t();
	default:
		bz_unreachable;
	}
}


bool comptime_executor_context::has_terminator(void) const
{
	auto const current_bb = this->builder.GetInsertBlock();
	return current_bb->size() != 0 && current_bb->back().isTerminator();
}

bool comptime_executor_context::has_terminator(llvm::BasicBlock *bb)
{
	return bb->size() != 0 && bb->back().isTerminator();
}

void comptime_executor_context::ensure_function_emission(ast::function_body *body)
{
	if (!body->is_intrinsic() && !this->functions_to_compile.contains(body))
	{
		this->functions_to_compile.push_back(body);
	}
}

bool comptime_executor_context::resolve_function(ast::function_body *body)
{
	if (body->body.is_null())
	{
		bz_assert(body->is_intrinsic());
		return true;
	}
	bz_assert(this->current_parse_ctx != nullptr);
	this->current_parse_ctx->add_to_resolve_queue({}, *body);
	parse::resolve_function({}, *body, *this->current_parse_ctx);
	this->current_parse_ctx->pop_resolve_queue();
	if (body->state == ast::resolve_state::error)
	{
		return false;
	}
	else
	{
		return true;
	}
}

llvm::Function *comptime_executor_context::get_comptime_function(comptime_function_kind kind)
{
	bz_assert(this->comptime_functions[static_cast<uint32_t>(kind)].llvm_func != nullptr);
	return this->comptime_functions[static_cast<uint32_t>(kind)].llvm_func;
}

void comptime_executor_context::set_comptime_function(comptime_function_kind kind, ast::function_body *func_body)
{
	bz_assert(this->comptime_functions[static_cast<uint32_t>(kind)].func_body == nullptr);
	bz_assert(this->comptime_functions[static_cast<uint32_t>(kind)].llvm_func == nullptr);
	this->comptime_functions[static_cast<uint32_t>(kind)].func_body = func_body;
}

void comptime_executor_context::set_comptime_function(comptime_function_kind kind, llvm::Function *llvm_func)
{
	bz_assert(this->comptime_functions[static_cast<uint32_t>(kind)].func_body != nullptr);
	bz_assert(this->comptime_functions[static_cast<uint32_t>(kind)].llvm_func == nullptr);
	this->comptime_functions[static_cast<uint32_t>(kind)].llvm_func = llvm_func;
}

static ast::constant_value constant_value_from_generic_value(llvm::GenericValue const &value, ast::typespec_view result_type)
{
	ast::constant_value result;
	ast::remove_const_or_consteval(result_type).visit(bz::overload{
		[&](ast::ts_base_type const &base_t) {
			switch (base_t.info->kind)
			{
			case ast::type_info::int8_:
			case ast::type_info::int16_:
			case ast::type_info::int32_:
			case ast::type_info::int64_:
				result.emplace<ast::constant_value::sint>(value.IntVal.getSExtValue());
				break;
			case ast::type_info::uint8_:
			case ast::type_info::uint16_:
			case ast::type_info::uint32_:
			case ast::type_info::uint64_:
				result.emplace<ast::constant_value::uint>(value.IntVal.getLimitedValue());
				break;
			case ast::type_info::float32_:
				result.emplace<ast::constant_value::float32>(value.FloatVal);
				break;
			case ast::type_info::float64_:
				result.emplace<ast::constant_value::float64>(value.DoubleVal);
				break;
			case ast::type_info::char_:
				result.emplace<ast::constant_value::u8char>(static_cast<bz::u8char>(value.IntVal.getLimitedValue()));
				break;
			case ast::type_info::str_:
				bz_unreachable;
			case ast::type_info::bool_:
				result.emplace<ast::constant_value::boolean>(value.IntVal.getBoolValue());
				break;
			case ast::type_info::null_t_:
				result.emplace<ast::constant_value::null>();
				break;
			case ast::type_info::aggregate:
			{
				result.emplace<ast::constant_value::aggregate>(
					bz::zip(value.AggregateVal, base_t.info->member_variables)
					.transform([](auto const &pair) { return constant_value_from_generic_value(pair.first, pair.second.type); })
					.collect()
				);
				break;
			}
			case ast::type_info::forward_declaration:
				bz_unreachable;
			}
		},
		[](ast::ts_void const &) {
			// nothing
		},
		[](ast::ts_function const &) {
			bz_unreachable;
		},
		[&](ast::ts_array const &array_t) {
			result.emplace<ast::constant_value::array>(
				bz::basic_range(value.AggregateVal.begin(), value.AggregateVal.end())
				.transform([&](auto const &val) { return constant_value_from_generic_value(val, array_t.elem_type); })
				.collect()
			);
		},
		[](ast::ts_array_slice const &) {
			bz_unreachable;
		},
		[&](ast::ts_tuple const &tuple_t) {
			result.emplace<ast::constant_value::tuple>(
				bz::zip(value.AggregateVal, tuple_t.types)
				.transform([](auto const &pair) { return constant_value_from_generic_value(pair.first, pair.second); })
				.collect()
			);
		},
		[](ast::ts_pointer const &) {
			bz_unreachable;
		},
		[](ast::ts_lvalue_reference const &) {
			bz_unreachable;
		},
		[](ast::ts_auto_reference const &) {
			bz_unreachable;
		},
		[](ast::ts_auto_reference_const const &) {
			bz_unreachable;
		},
		[](ast::ts_unresolved const &) {
			bz_unreachable;
		},
		[](ast::ts_const const &) {
			bz_unreachable;
		},
		[](ast::ts_consteval const &) {
			bz_unreachable;
		},
		[](ast::ts_auto const &) {
			bz_unreachable;
		},
		[](ast::ts_typename const &) {
			bz_unreachable;
		},
	});
	return result;
}

static ast::constant_value constant_value_from_global_getters(
	ast::typespec_view result_type,
	bz::vector<llvm::Function *>::const_iterator &getter_it,
	comptime_executor_context &context
)
{
	return ast::remove_const_or_consteval(result_type).visit(bz::overload{
		[&](ast::ts_base_type const &base_t) -> ast::constant_value {
			if (base_t.info->kind == ast::type_info::aggregate)
			{
				ast::constant_value result;
				result.emplace<ast::constant_value::aggregate>();
				auto &agg = result.get<ast::constant_value::aggregate>();
				agg.reserve(base_t.info->member_variables.size());
				for (auto const &[_, __, type] : base_t.info->member_variables)
				{
					agg.push_back(constant_value_from_global_getters(type, getter_it, context));
				}
				return result;
			}
			else
			{
				auto const call_result = context.engine->runFunction(*getter_it, {});
				++getter_it;
				return constant_value_from_generic_value(call_result, result_type);
			}
		},
		[&](ast::ts_array const &array_t) -> ast::constant_value {
			ast::constant_value result;
			result.emplace<ast::constant_value::array>();
			auto &arr = result.get<ast::constant_value::array>();
			arr.reserve(array_t.size);
			for ([[maybe_unused]] auto const _ : bz::iota(0, array_t.size))
			{
				arr.push_back(constant_value_from_global_getters(array_t.elem_type, getter_it, context));
			}
			return result;
		},
		[&](ast::ts_tuple const &tuple_t) -> ast::constant_value {
			ast::constant_value result;
			result.emplace<ast::constant_value::tuple>();
			auto &tuple = result.get<ast::constant_value::tuple>();
			tuple.reserve(tuple_t.types.size());
			for (auto const &type : tuple_t.types)
			{
				tuple.push_back(constant_value_from_global_getters(type, getter_it, context));
			}
			return result;
		},
		[&](auto const &) -> ast::constant_value {
			auto const call_result = context.engine->runFunction(*getter_it, {});
			++getter_it;
			return constant_value_from_generic_value(call_result, result_type);
		}
	});
}

std::pair<ast::constant_value, bz::vector<error>> comptime_executor_context::execute_function(
	lex::src_tokens src_tokens,
	ast::function_body *body,
	bz::array_view<ast::constant_value const> params
)
{
	auto const original_module = this->current_module;
	auto module = std::make_unique<llvm::Module>("comptime_module", this->get_llvm_context());
	module->setDataLayout(this->get_data_layout());
	auto const is_native_target = target == "" || target == "native";
	auto const target_triple = is_native_target
		? llvm::sys::getDefaultTargetTriple()
		: std::string(target.data_as_char_ptr(), target.size());
	module->setTargetTriple(target_triple);
	this->current_module = module.get();

	(void)this->resolve_function(body);
	std::pair<ast::constant_value, bz::vector<error>> result;
	if (body->state == ast::resolve_state::error)
	{
		this->current_module = original_module;
		return result;
	}
	else if (body->state != ast::resolve_state::all)
	{
		result.second.push_back(error{
			warning_kind::_last,
			source_highlight{
				src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
				src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
				suggestion_range{}, suggestion_range{},
				bz::format("cannot call external function '{}' in a constant expression", body->get_signature())
			},
			{}, {}
		});
		this->current_module = original_module;
		return result;
	}
	else
	{
		this->initialize_engine();
		this->ensure_function_emission(body);
		if (!bc::comptime::emit_necessary_functions(*this))
		{
			return result;
		}

		auto const [fn, global_result_getters] = bc::comptime::create_function_for_comptime_execution(body, params, *this);
		// bz_assert(!llvm::verifyFunction(*fn, &llvm::dbgs()));
		this->add_module(std::move(module));
		auto const call_result = this->engine->runFunction(fn, {});
		if (this->has_error())
		{
			auto errors = this->consume_errors();
			result.second.append_move(errors);
		}
		else if (global_result_getters.empty())
		{
			result.first = constant_value_from_generic_value(call_result, body->return_type);
		}
		else
		{
			auto getter_it = global_result_getters.cbegin();
			result.first = constant_value_from_global_getters(body->return_type, getter_it, *this);
		}
		this->current_module = original_module;
		return result;
	}
}

std::pair<ast::constant_value, bz::vector<error>> comptime_executor_context::execute_compound_expression(ast::expr_compound &expr)
{
	auto const original_module = this->current_module;
	auto module = std::make_unique<llvm::Module>("comptime_module", this->get_llvm_context());
	module->setDataLayout(this->get_data_layout());
	auto const is_native_target = target == "" || target == "native";
	auto const target_triple = is_native_target
		? llvm::sys::getDefaultTargetTriple()
		: std::string(target.data_as_char_ptr(), target.size());
	module->setTargetTriple(target_triple);
	this->current_module = module.get();

	std::pair<ast::constant_value, bz::vector<error>> result;
	this->initialize_engine();

	auto const [fn, global_result_getters] = bc::comptime::create_function_for_comptime_execution(expr, *this);
	if (!bc::comptime::emit_necessary_functions(*this))
	{
		return result;
	}

	// bz_assert(!llvm::verifyFunction(*fn, &llvm::dbgs()));
	this->add_module(std::move(module));
	auto const call_result = this->engine->runFunction(fn, {});
	if (this->has_error())
	{
		auto errors = this->consume_errors();
		result.second.append_move(errors);
	}
	else if (global_result_getters.empty())
	{
		bz_assert(expr.final_expr.not_null());
		auto const result_type = expr.final_expr.get_expr_type_and_kind().first;
		result.first = constant_value_from_generic_value(call_result, ast::remove_const_or_consteval(result_type));
	}
	else
	{
		auto const result_type = expr.final_expr.get_expr_type_and_kind().first;
		auto getter_it = global_result_getters.cbegin();
		result.first = constant_value_from_global_getters(result_type, getter_it, *this);
	}
	this->current_module = original_module;
	return result;
}

void comptime_executor_context::initialize_engine(void)
{
	if (this->engine == nullptr)
	{
		auto module = std::make_unique<llvm::Module>("comptime_module", this->get_llvm_context());
		module->setDataLayout(this->get_data_layout());
		auto const is_native_target = target == "" || target == "native";
		auto const target_triple = is_native_target
			? llvm::sys::getDefaultTargetTriple()
			: std::string(target.data_as_char_ptr(), target.size());
		module->setTargetTriple(target_triple);
		this->add_base_functions_to_module(*module);
		this->pass_manager.add(llvm::createInstructionCombiningPass());
		this->pass_manager.add(llvm::createPromoteMemoryToRegisterPass());
		this->pass_manager.add(llvm::createReassociatePass());
		this->pass_manager.add(llvm::createCFGSimplificationPass());
		// this->pass_manager.add(llvm::createGVNPass());

		this->pass_manager.run(*module);
		this->pass_manager.run(*module);

		this->engine = this->create_engine(std::move(module));
	}
}

std::unique_ptr<llvm::ExecutionEngine> comptime_executor_context::create_engine(std::unique_ptr<llvm::Module> module)
{
	if (debug_comptime_ir_output)
	{
		std::error_code ec;
		auto output_file = llvm::raw_fd_ostream("comptime_output.ll", ec, llvm::sys::fs::OF_Text);
		if (!ec)
		{
			module->print(output_file, nullptr);
		}
	}
	std::string err;
	llvm::EngineBuilder builder(std::move(module));
	builder
		.setEngineKind(use_interpreter ? llvm::EngineKind::Interpreter : llvm::EngineKind::Either)
		.setErrorStr(&err)
		.setOptLevel(llvm::CodeGenOpt::None);
	auto const result = builder.create();
	if (result == nullptr)
	{
		bz::log("execution engine creation failed: {}\n", err.c_str());
		exit(1);
	}
	return std::unique_ptr<llvm::ExecutionEngine>(result);
}

void comptime_executor_context::add_base_functions_to_module(llvm::Module &module)
{
	static_assert(sizeof (void *) == sizeof (uint64_t));

	auto const original_module = this->current_module;
	this->current_module = &module;
	bz_assert(this->functions_to_compile.empty());

	bc::comptime::add_builtin_functions(*this);

	bz_assert(this->errors_array != nullptr);
	bc::comptime::emit_global_variable(*this->errors_array, *this);
	bz_assert(this->call_stack != nullptr);
	bc::comptime::emit_global_variable(*this->call_stack, *this);

	for (auto &func : this->comptime_functions)
	{
		bz_assert(func.func_body != nullptr);
		bz_assert(func.llvm_func == nullptr);
		func.llvm_func = bc::comptime::add_function_to_module(func.func_body, *this);
		this->functions_to_compile.push_back(func.func_body);
	}

	[[maybe_unused]] auto const emit_result = bc::comptime::emit_necessary_functions(*this);
	bz_assert(emit_result);
	this->functions_to_compile.clear();

	this->current_module = original_module;
}

void comptime_executor_context::add_module(std::unique_ptr<llvm::Module> module)
{
	bz_assert(this->engine != nullptr);
	this->pass_manager.run(*module);
	this->pass_manager.run(*module);
	if (debug_comptime_ir_output)
	{
		std::error_code ec;
		auto output_file = llvm::raw_fd_ostream("comptime_output.ll", ec, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
		if (!ec)
		{
			module->print(output_file, nullptr);
		}
	}
	this->engine->addModule(std::move(module));
}

bool comptime_executor_context::has_error(void)
{
	if (this->engine == nullptr)
	{
		return false;
	}
	else
	{
		return this->engine->runFunction(this->get_comptime_function(comptime_function_kind::has_errors), {}).IntVal.getBoolValue();
	}
}

bz::vector<error> comptime_executor_context::consume_errors(void)
{
	bz_assert(this->engine != nullptr);
	auto const error_count = this->engine->runFunction(this->get_comptime_function(comptime_function_kind::get_error_count), {})
		.IntVal.getLimitedValue();
	bz::vector<error> result;
	result.reserve(error_count);
	llvm::GenericValue index;
	for (size_t i = 0; i < error_count; ++i)
	{
		index.IntVal = i;
		auto const error_kind = [&]() -> uint32_t {
			auto const llvm_func = this->get_comptime_function(comptime_function_kind::get_error_kind_by_index);
			auto const func_ptr = this->engine->getFunctionAddress(llvm_func->getName().str());
			if (func_ptr != 0)
			{
				return reinterpret_cast<uint32_t (*)(uint64_t)>(func_ptr)(i);
			}
			else
			{
				return this->engine->runFunction(llvm_func, index).IntVal.getLimitedValue();
			}
		}();
		auto const error_ptr = [&]() -> uint64_t {
			auto const llvm_func = this->get_comptime_function(comptime_function_kind::get_error_ptr_by_index);
			auto const func_ptr = this->engine->getFunctionAddress(llvm_func->getName().str());
			if (func_ptr != 0)
			{
				return reinterpret_cast<uint64_t (*)(uint64_t)>(func_ptr)(i);
			}
			else
			{
				return this->engine->runFunction(llvm_func, index).IntVal.getLimitedValue();
			}
		}();
		auto const call_stack_notes = [&]() {
			bz::vector<source_highlight> notes;
			auto const call_stack_size = [&]() -> uint64_t {
				auto const llvm_func = this->get_comptime_function(comptime_function_kind::get_error_call_stack_size_by_index);
				auto const func_ptr = this->engine->getFunctionAddress(llvm_func->getName().str());
				if (func_ptr != 0)
				{
					return reinterpret_cast<uint64_t (*)(uint64_t)>(func_ptr)(i);
				}
				else
				{
					return this->engine->runFunction(llvm_func, index).IntVal.getLimitedValue();
				}
			}();
			notes.reserve(call_stack_size);
			llvm::GenericValue call_stack_index;
			for (uint64_t j = call_stack_size; j != 0;)
			{
				--j;
				auto const call_ptr_int_val = [&]() -> uint64_t {
					auto const llvm_func = this->get_comptime_function(comptime_function_kind::get_error_call_stack_element_by_index);
					auto const func_ptr = this->engine->getFunctionAddress(llvm_func->getName().str());
					if (func_ptr != 0)
					{
						return reinterpret_cast<uint64_t (*)(uint64_t, uint64_t)>(func_ptr)(i, j);
					}
					else
					{
						call_stack_index.IntVal = j;
						return this->engine->runFunction(llvm_func, { index, call_stack_index }).IntVal.getLimitedValue();
					}
				}();
				bz_assert(call_ptr_int_val != 0);
				auto const call_ptr = reinterpret_cast<comptime_func_call const *>(call_ptr_int_val);
				notes.emplace_back(this->make_note(
					call_ptr->src_tokens,
					bz::format("in call to '{}'", call_ptr->func_body->get_signature())
				));
			}
			return notes;
		}();
		bz_assert(error_ptr != 0);
		result.push_back({
			static_cast<warning_kind>(error_kind),
			*reinterpret_cast<source_highlight const *>(error_ptr),
			std::move(call_stack_notes), {}
		});
	}
	this->engine->runFunction(this->get_comptime_function(comptime_function_kind::clear_errors), {});
	return result;
}

source_highlight const *comptime_executor_context::insert_error(lex::src_tokens src_tokens, bz::u8string message)
{
	auto const &result = this->execution_errors.emplace_back(source_highlight{
		src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
		src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
		suggestion_range{}, suggestion_range{},
		std::move(message)
	});
	return &result;
}

comptime_func_call const *comptime_executor_context::insert_call(lex::src_tokens src_tokens, ast::function_body const *body)
{
	auto const &result = this->execution_calls.emplace_back(comptime_func_call{ src_tokens, body });
	return &result;
}

error comptime_executor_context::make_error(
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
)
{
	return error{
		warning_kind::_last,
		{
			global_context::compiler_file_id, 0,
			char_pos(), char_pos(), char_pos(),
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	};
}

error comptime_executor_context::make_error(
	lex::src_tokens src_tokens, bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
)
{
	return error{
		warning_kind::_last,
		{
			src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
			src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	};
}

source_highlight comptime_executor_context::make_note(lex::src_tokens src_tokens, bz::u8string message)
{
	return source_highlight{
		src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
		src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
		suggestion_range{}, suggestion_range{},
		std::move(message),
	};
}

} // namespace ctx
