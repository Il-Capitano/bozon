#ifndef BC_COMMON_H
#define BC_COMMON_H

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include "ast/typespec.h"
#include "ast/statement.h"

namespace bc
{

template<typename Context>
llvm::Type *get_llvm_base_type(ast::ts_base_type const &base_t, Context &context)
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
		return context.get_builtin_type(base_t.info->kind);

	case ast::type_info::forward_declaration:
	case ast::type_info::aggregate:
	{
		auto const result = context.get_base_type(base_t.info);
		bz_assert(result != nullptr);
		return result;
	}
	default:
		bz_unreachable;
	}
}

template<typename Context>
llvm::Type *get_llvm_type(ast::typespec_view ts, Context &context, bool is_top_level = true)
{
	static_assert(ast::typespec_types::size() == 19);
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
	case ast::typespec_node_t::index_of<ast::ts_optional_pointer>:
	{
		auto const base = get_llvm_type(ts.get<ast::ts_optional_pointer>(), context, false);
		return llvm::PointerType::get(base, 0);
	}
	case ast::typespec_node_t::index_of<ast::ts_optional>:
	{
		auto const base = get_llvm_type(ts.get<ast::ts_optional>(), context);
		auto const bool_t = context.get_bool_t();
		return llvm::StructType::get(base, bool_t);
	}
	case ast::typespec_node_t::index_of<ast::ts_lvalue_reference>:
	{
		auto const base = get_llvm_type(ts.get<ast::ts_lvalue_reference>(), context, false);
		return llvm::PointerType::get(base, 0);
	}
	case ast::typespec_node_t::index_of<ast::ts_move_reference>:
	{
		auto const base = get_llvm_type(ts.get<ast::ts_move_reference>(), context, false);
		return llvm::PointerType::get(base, 0);
	}
	case ast::typespec_node_t::index_of<ast::ts_function>:
	{
		auto &fn_t = ts.get<ast::ts_function>();
		auto const result_t = get_llvm_type(fn_t.return_type, context);
		ast::arena_vector<llvm::Type *> args = {};
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
		return llvm::ArrayType::get(elem_t, arr_t.size);
	}
	case ast::typespec_node_t::index_of<ast::ts_array_slice>:
	{
		auto &arr_slice_t = ts.get<ast::ts_array_slice>();
		auto const elem_t = get_llvm_type(arr_slice_t.elem_type, context);
		return context.get_slice_t(elem_t);
	}
	case ast::typespec_node_t::index_of<ast::ts_tuple>:
	{
		auto &tuple_t = ts.get<ast::ts_tuple>();
		auto const types = tuple_t.types
			.transform([&context](auto const &ts) { return get_llvm_type(ts, context); })
			.template collect<ast::arena_vector>();
		return context.get_tuple_t(types);
	}
	case ast::typespec_node_t::index_of<ast::ts_auto>:
		bz_unreachable;
	case ast::typespec_node_t::index_of<ast::ts_unresolved>:
		bz_unreachable;
	case ast::typespec_node_t::index_of<ast::ts_typename>:
		bz_unreachable;
	default:
		bz_unreachable;
	}
}

} // namespace bc

#endif // BC_COMMON_H
