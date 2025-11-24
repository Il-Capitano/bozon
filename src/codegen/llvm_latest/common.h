#ifndef CODEGEN_LLVM_LATEST_COMMON_H
#define CODEGEN_LLVM_LATEST_COMMON_H

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include "ast/typespec.h"
#include "ast/statement.h"

namespace codegen::llvm_latest
{

template<typename Context>
llvm::Type *get_llvm_base_type(ast::ts_base_type const &base_t, Context &context)
{
	switch (base_t.info->kind)
	{
	case ast::type_info::i8_:
	case ast::type_info::u8_:
	case ast::type_info::i16_:
	case ast::type_info::u16_:
	case ast::type_info::i32_:
	case ast::type_info::u32_:
	case ast::type_info::i64_:
	case ast::type_info::u64_:
	case ast::type_info::f32_:
	case ast::type_info::f64_:
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
llvm::Type *get_llvm_type(ast::typespec_view ts, Context &context)
{
	static_assert(ast::typespec_types::size() == 19);
	if (ts.modifiers.empty())
	{
		switch (ts.terminator_kind())
		{
		case ast::terminator_typespec_node_t::index_of<ast::ts_base_type>:
			return get_llvm_base_type(ts.get<ast::ts_base_type>(), context);
		case ast::terminator_typespec_node_t::index_of<ast::ts_enum>:
			return get_llvm_type(ts.get<ast::ts_enum>().decl->underlying_type, context);
		case ast::terminator_typespec_node_t::index_of<ast::ts_void>:
			return llvm::Type::getVoidTy(context.get_llvm_context());
		case ast::terminator_typespec_node_t::index_of<ast::ts_function>:
			return context.get_opaque_pointer_t();
		case ast::terminator_typespec_node_t::index_of<ast::ts_array>:
		{
			auto &arr_t = ts.get<ast::ts_array>();
			auto elem_t = get_llvm_type(arr_t.elem_type, context);
			return llvm::ArrayType::get(elem_t, arr_t.size);
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_array_slice>:
			return context.get_slice_t();
		case ast::terminator_typespec_node_t::index_of<ast::ts_tuple>:
		{
			auto &tuple_t = ts.get<ast::ts_tuple>();
			auto const types = tuple_t.types
				.transform([&context](auto const &ts) { return get_llvm_type(ts, context); })
				.template collect<ast::arena_vector>();
			return context.get_tuple_t(types);
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_auto>:
			bz_unreachable;
		case ast::terminator_typespec_node_t::index_of<ast::ts_unresolved>:
			bz_unreachable;
		case ast::terminator_typespec_node_t::index_of<ast::ts_typename>:
			bz_unreachable;
		default:
			bz_unreachable;
		}
	}
	else
	{
		switch (ts.modifier_kind())
		{
		case ast::modifier_typespec_node_t::index_of<ast::ts_mut>:
			return get_llvm_type(ts.get<ast::ts_mut>(), context);
		case ast::modifier_typespec_node_t::index_of<ast::ts_consteval>:
			return get_llvm_type(ts.get<ast::ts_consteval>(), context);
		case ast::modifier_typespec_node_t::index_of<ast::ts_pointer>:
		case ast::modifier_typespec_node_t::index_of<ast::ts_lvalue_reference>:
		case ast::modifier_typespec_node_t::index_of<ast::ts_move_reference>:
			return context.get_opaque_pointer_t();
		case ast::modifier_typespec_node_t::index_of<ast::ts_optional>:
		{
			if (ts.is_optional_pointer_like() || ts.is_optional_reference())
			{
				return context.get_opaque_pointer_t();
			}
			else
			{
				auto const inner_llvm_type = get_llvm_type(ts.get<ast::ts_optional>(), context);
				return llvm::StructType::get(inner_llvm_type, context.get_bool_t());
			}
		}
		default:
			bz_unreachable;
		}
	}
}

inline bool is_non_trivial_pass_kind(ast::typespec_view ts)
{
	return !ts.is<ast::ts_void>()
		&& !ts.is<ast::ts_lvalue_reference>()
		&& !ts.is<ast::ts_move_reference>()
		&& !ast::is_trivially_relocatable(ts);
}

} // namespace codegen::llvm_latest

#endif // CODEGEN_LLVM_LATEST_COMMON_H
