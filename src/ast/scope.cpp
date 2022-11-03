#include "scope.h"
#include "statement.h"

namespace ast
{

void global_scope_t::add_variable(decl_variable &var_decl)
{
	this->variables.push_back(&var_decl);
}

void global_scope_t::add_variable(decl_variable &original_decl, arena_vector<decl_variable *> variadic_decls)
{
	this->variadic_variables.push_back({ &original_decl, std::move(variadic_decls) });
}

void global_scope_t::add_function(decl_function &func_decl)
{
	auto const it = std::find_if(
		this->function_sets.begin(), this->function_sets.end(),
		[&func_decl](auto const &set) {
			return set.id == func_decl.id;
		}
	);
	if (it != this->function_sets.end())
	{
		it->func_decls.push_back(&func_decl);
	}
	else
	{
		auto &set = this->function_sets.emplace_back();
		set.id = func_decl.id;
		set.func_decls.push_back(&func_decl);
	}
}

void global_scope_t::add_function_alias(decl_function_alias &alias_decl)
{
	auto const it = std::find_if(
		this->function_sets.begin(), this->function_sets.end(),
		[&alias_decl](auto const &set) {
			return set.id == alias_decl.id;
		}
	);
	if (it != this->function_sets.end())
	{
		it->alias_decls.push_back(&alias_decl);
	}
	else
	{
		auto &set = this->function_sets.emplace_back();
		set.id = alias_decl.id;
		set.alias_decls.push_back(&alias_decl);
	}
}

void global_scope_t::add_operator(decl_operator &op_decl)
{
	auto const it = std::find_if(
		this->operator_sets.begin(), this->operator_sets.end(),
		[&op_decl](auto const &set) {
			return set.op == op_decl.body.function_name_or_operator_kind.get<uint32_t>()
				&& set.id_scope == op_decl.scope.as_array_view();
		}
	);
	if (it != this->operator_sets.end())
	{
		it->op_decls.push_back(&op_decl);
	}
	else
	{
		auto &set = this->operator_sets.emplace_back();
		set.id_scope = op_decl.scope;
		set.op = op_decl.body.function_name_or_operator_kind.get<uint32_t>();
		set.op_decls.push_back(&op_decl);
	}
}

void global_scope_t::add_type_alias(decl_type_alias &alias_decl)
{
	this->type_aliases.push_back(&alias_decl);
}

void global_scope_t::add_struct(decl_struct &struct_decl)
{
	this->structs.push_back(&struct_decl);
}

void global_scope_t::add_enum(decl_enum &enum_decl)
{
	this->enums.push_back(&enum_decl);
}

identifier const &local_symbol_t::get_id(void) const
{
	static_assert(variant_count == 7);
	switch (this->index())
	{
	case index_of<decl_variable *>:
		return this->get<decl_variable *>()->get_id();
	case index_of<variadic_var_decl>:
		return this->get<variadic_var_decl>().original_decl->get_id();
	case index_of<decl_function *>:
		return this->get<decl_function *>()->id;
	case index_of<decl_function_alias *>:
		return this->get<decl_function_alias *>()->id;
	case index_of<decl_type_alias *>:
		return this->get<decl_type_alias *>()->id;
	case index_of<decl_struct *>:
		return this->get<decl_struct *>()->id;
	case index_of<decl_enum *>:
		return this->get<decl_enum *>()->id;
	default:
		bz_unreachable;
	}
}

lex::src_tokens const &local_symbol_t::get_src_tokens(void) const
{
	static_assert(variant_count == 7);
	switch (this->index())
	{
	case index_of<decl_variable *>:
		return this->get<decl_variable *>()->src_tokens;
	case index_of<variadic_var_decl>:
		return this->get<variadic_var_decl>().original_decl->src_tokens;
	case index_of<decl_function *>:
		return this->get<decl_function *>()->body.src_tokens;
	case index_of<decl_function_alias *>:
		return this->get<decl_function_alias *>()->src_tokens;
	case index_of<decl_type_alias *>:
		return this->get<decl_type_alias *>()->src_tokens;
	case index_of<decl_struct *>:
		return this->get<decl_struct *>()->info.src_tokens;
	case index_of<decl_enum *>:
		return this->get<decl_enum *>()->src_tokens;
	default:
		bz_unreachable;
	}
}

local_symbol_t *local_scope_t::find_by_id(identifier const &id, size_t bound) noexcept
{
	if (id.is_qualified || id.values.size() != 1)
	{
		return nullptr;
	}

	auto const symbols = this->symbols.slice(0, bound);
	auto const it = std::find_if(
		symbols.rbegin(), symbols.rend(),
		[&id](auto const &symbol) {
			return symbol.get_id() == id;
		}
	);
	if (it != symbols.rend())
	{
		return &*it;
	}
	else
	{
		return nullptr;
	}
}

void local_scope_t::add_variable(decl_variable &var_decl)
{
	this->symbols.emplace_back(&var_decl);
}

void local_scope_t::add_variable(decl_variable &original_decl, arena_vector<decl_variable *> variadic_decls)
{
	this->symbols.emplace_back(variadic_var_decl{ &original_decl, std::move(variadic_decls) });
}

void local_scope_t::add_function(decl_function &func_decl)
{
	this->symbols.emplace_back(&func_decl);
}

void local_scope_t::add_type_alias(decl_type_alias &alias_decl)
{
	this->symbols.emplace_back(&alias_decl);
}

} // namespace ast
