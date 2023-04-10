#include "scope.h"
#include "statement.h"

namespace ast
{

std::pair<global_scope_symbol_list_t::id_map_t::iterator, bool> global_scope_symbol_list_t::insert(
	bz::array_view<bz::u8string_view const> id,
	global_scope_symbol_index_t index
)
{
	auto const it = this->id_map.find(id);
	if (it != this->id_map.end())
	{
		return { it, false };
	}
	else
	{
		auto const &new_id = this->id_storage.push_back(id);
		return this->id_map.insert({ new_id, index });
	}
}

static_assert(statement::variant_count == 17);

void global_scope_symbol_list_t::add_variable(bz::array_view<bz::u8string_view const> id, decl_variable &var_decl)
{
	auto const index = this->variables.size();
	this->variables.push_back(&var_decl);

	if (id.empty())
	{
		id = var_decl.get_id().values;
	}

	auto const [it, inserted] = this->insert(id, {
		.symbol_kind = global_scope_symbol_kind::variable,
		.index = static_cast<uint32_t>(index)
	});
	id = it->first;

	if (!inserted)
	{
		auto &ambiguous_ids = this->ambiguous_id_map[id];
		if (it->second.symbol_kind != global_scope_symbol_kind::ambiguous)
		{
			ambiguous_ids.push_back(it->second);
			it->second.symbol_kind = global_scope_symbol_kind::ambiguous;
		}
		ambiguous_ids.push_back({
			.symbol_kind = global_scope_symbol_kind::variable,
			.index = static_cast<uint32_t>(index),
		});
	}
}

void global_scope_symbol_list_t::add_variable(bz::array_view<bz::u8string_view const> id, decl_variable &original_decl, arena_vector<decl_variable *> variadic_decls)
{
	auto const index = this->variadic_variables.size();
	this->variadic_variables.push_back({ &original_decl, std::move(variadic_decls) });

	if (id.empty())
	{
		id = original_decl.get_id().values;
	}

	auto const [it, inserted] = this->insert(id, {
		.symbol_kind = global_scope_symbol_kind::variadic_variable,
		.index = static_cast<uint32_t>(index)
	});
	id = it->first;

	if (!inserted)
	{
		auto &ambiguous_ids = this->ambiguous_id_map[id];
		if (it->second.symbol_kind != global_scope_symbol_kind::ambiguous)
		{
			ambiguous_ids.push_back(it->second);
			it->second.symbol_kind = global_scope_symbol_kind::ambiguous;
		}
		ambiguous_ids.push_back({
			.symbol_kind = global_scope_symbol_kind::variadic_variable,
			.index = static_cast<uint32_t>(index),
		});
	}
}

void global_scope_symbol_list_t::add_function(bz::array_view<bz::u8string_view const> id, decl_function &func_decl)
{
	if (id.empty())
	{
		id = func_decl.id.values;
	}

	auto const potential_index = this->function_sets.size();
	auto const [it, inserted] = this->insert(id, {
		.symbol_kind = global_scope_symbol_kind::function_set,
		.index = static_cast<uint32_t>(potential_index)
	});
	id = it->first;

	if (inserted)
	{
		auto &set = this->function_sets.emplace_back();
		set.func_decls.push_back(&func_decl);
	}
	else if (it->second.symbol_kind == global_scope_symbol_kind::function_set)
	{
		this->function_sets[it->second.index].func_decls.push_back(&func_decl);
	}
	// only handle this case if the id is not already ambiguous.
	// this means that the error messages are not ideal in the case where
	// the function set would have been added as a third or later ambiguous id,
	// but that's probably rare, so it's not necessary to handle it
	else if (it->second.symbol_kind != global_scope_symbol_kind::ambiguous)
	{
		auto &ambiguous_ids = this->ambiguous_id_map[id];
		ambiguous_ids.push_back(it->second);
		it->second.symbol_kind = global_scope_symbol_kind::ambiguous;

		auto const index = this->function_sets.size();
		auto &set = this->function_sets.emplace_back();
		set.func_decls.push_back(&func_decl);

		ambiguous_ids.push_back({
			.symbol_kind = global_scope_symbol_kind::function_set,
			.index = static_cast<uint32_t>(index),
		});
	}
}

void global_scope_symbol_list_t::add_function_alias(bz::array_view<bz::u8string_view const> id, decl_function_alias &alias_decl)
{
	if (id.empty())
	{
		id = alias_decl.id.values;
	}

	auto const potential_index = this->function_sets.size();
	auto const [it, inserted] = this->insert(id, {
		.symbol_kind = global_scope_symbol_kind::function_set,
		.index = static_cast<uint32_t>(potential_index)
	});
	id = it->first;

	if (inserted)
	{
		auto &set = this->function_sets.emplace_back();
		set.alias_decls.push_back(&alias_decl);
	}
	else if (it->second.symbol_kind == global_scope_symbol_kind::function_set)
	{
		this->function_sets[it->second.index].alias_decls.push_back(&alias_decl);
	}
	else if (it->second.symbol_kind != global_scope_symbol_kind::ambiguous)
	{
		auto &ambiguous_ids = this->ambiguous_id_map[id];
		ambiguous_ids.push_back(it->second);
		it->second.symbol_kind = global_scope_symbol_kind::ambiguous;

		auto const index = this->function_sets.size();
		auto &set = this->function_sets.emplace_back();
		set.alias_decls.push_back(&alias_decl);

		ambiguous_ids.push_back({
			.symbol_kind = global_scope_symbol_kind::function_set,
			.index = static_cast<uint32_t>(index),
		});
	}
}

void global_scope_symbol_list_t::add_operator(decl_operator &op_decl)
{
	auto const it = std::find_if(
		this->operator_sets.begin(), this->operator_sets.end(),
		[&op_decl](auto const &set) {
			return set.op == op_decl.body.function_name_or_operator_kind.get<uint32_t>();
		}
	);
	if (it != this->operator_sets.end())
	{
		it->op_decls.push_back(&op_decl);
	}
	else
	{
		auto &set = this->operator_sets.emplace_back();
		set.op = op_decl.body.function_name_or_operator_kind.get<uint32_t>();
		set.op_decls.push_back(&op_decl);
	}
}

void global_scope_symbol_list_t::add_operator_alias(decl_operator_alias &alias_decl)
{
	auto const it = std::find_if(
		this->operator_sets.begin(), this->operator_sets.end(),
		[&alias_decl](auto const &set) {
			return set.op == alias_decl.op;
		}
	);
	if (it != this->operator_sets.end())
	{
		it->alias_decls.push_back(&alias_decl);
	}
	else
	{
		auto &set = this->operator_sets.emplace_back();
		set.op = alias_decl.op;
		set.alias_decls.push_back(&alias_decl);
	}
}

void global_scope_symbol_list_t::add_type_alias(bz::array_view<bz::u8string_view const> id, decl_type_alias &alias_decl)
{
	auto const index = this->type_aliases.size();
	this->type_aliases.push_back(&alias_decl);

	if (id.empty())
	{
		id = alias_decl.id.values;
	}

	auto const [it, inserted] = this->insert(id, {
		.symbol_kind = global_scope_symbol_kind::type_alias,
		.index = static_cast<uint32_t>(index)
	});
	id = it->first;

	if (!inserted)
	{
		auto &ambiguous_ids = this->ambiguous_id_map[id];
		if (it->second.symbol_kind != global_scope_symbol_kind::ambiguous)
		{
			ambiguous_ids.push_back(it->second);
			it->second.symbol_kind = global_scope_symbol_kind::ambiguous;
		}
		ambiguous_ids.push_back({
			.symbol_kind = global_scope_symbol_kind::type_alias,
			.index = static_cast<uint32_t>(index),
		});
	}
}

void global_scope_symbol_list_t::add_struct(bz::array_view<bz::u8string_view const> id, decl_struct &struct_decl)
{
	auto const index = this->structs.size();
	this->structs.push_back(&struct_decl);

	if (id.empty())
	{
		id = struct_decl.id.values;
	}

	auto const [it, inserted] = this->insert(id, {
		.symbol_kind = global_scope_symbol_kind::struct_,
		.index = static_cast<uint32_t>(index)
	});
	id = it->first;

	if (!inserted)
	{
		auto &ambiguous_ids = this->ambiguous_id_map[id];
		if (it->second.symbol_kind != global_scope_symbol_kind::ambiguous)
		{
			ambiguous_ids.push_back(it->second);
			it->second.symbol_kind = global_scope_symbol_kind::ambiguous;
		}
		ambiguous_ids.push_back({
			.symbol_kind = global_scope_symbol_kind::struct_,
			.index = static_cast<uint32_t>(index),
		});
	}
}

void global_scope_symbol_list_t::add_enum(bz::array_view<bz::u8string_view const> id, decl_enum &enum_decl)
{
	auto const index = this->enums.size();
	this->enums.push_back(&enum_decl);

	if (id.empty())
	{
		id = enum_decl.id.values;
	}

	auto const [it, inserted] = this->insert(id, {
		.symbol_kind = global_scope_symbol_kind::enum_,
		.index = static_cast<uint32_t>(index)
	});
	id = it->first;

	if (!inserted)
	{
		auto &ambiguous_ids = this->ambiguous_id_map[id];
		if (it->second.symbol_kind != global_scope_symbol_kind::ambiguous)
		{
			ambiguous_ids.push_back(it->second);
			it->second.symbol_kind = global_scope_symbol_kind::ambiguous;
		}
		ambiguous_ids.push_back({
			.symbol_kind = global_scope_symbol_kind::enum_,
			.index = static_cast<uint32_t>(index),
		});
	}
}

global_scope_symbol_index_t global_scope_symbol_list_t::get_symbol_index_by_id(identifier const &id) const
{
	auto const it = this->id_map.find(id.values);
	if (it != this->id_map.end())
	{
		return it->second;
	}
	else
	{
		return { global_scope_symbol_kind::none, 0 };
	}
}

bz::array_view<global_scope_symbol_index_t const> global_scope_symbol_list_t::get_ambiguous_symbols_by_id(identifier const &id) const
{
	auto const it = this->ambiguous_id_map.find(id.values);
	if (it != this->ambiguous_id_map.end())
	{
		return it->second;
	}
	else
	{
		return {};
	}
}

void global_scope_t::add_variable(bz::array_view<bz::u8string_view const> id, decl_variable &var_decl)
{
	this->all_symbols.add_variable(id, var_decl);
	if (var_decl.is_module_export())
	{
		this->export_symbols.add_variable(id, var_decl);
	}
}

void global_scope_t::add_variable(bz::array_view<bz::u8string_view const> id, decl_variable &original_decl, arena_vector<decl_variable *> variadic_decls)
{
	if (original_decl.is_module_export())
	{
		this->all_symbols.add_variable(id, original_decl, variadic_decls);
		this->export_symbols.add_variable(id, original_decl, std::move(variadic_decls));
	}
	else
	{
		this->all_symbols.add_variable(id, original_decl, std::move(variadic_decls));
	}
}

void global_scope_t::add_function(bz::array_view<bz::u8string_view const> id, decl_function &func_decl)
{
	this->all_symbols.add_function(id, func_decl);
	if (func_decl.body.is_export())
	{
		this->export_symbols.add_function(id, func_decl);
	}
}

void global_scope_t::add_function_alias(bz::array_view<bz::u8string_view const> id, decl_function_alias &alias_decl)
{
	this->all_symbols.add_function_alias(id, alias_decl);
	if (alias_decl.is_export)
	{
		this->export_symbols.add_function_alias(id, alias_decl);
	}
}

void global_scope_t::add_operator(decl_operator &op_decl)
{
	this->all_symbols.add_operator(op_decl);
	if (op_decl.body.is_export())
	{
		this->export_symbols.add_operator(op_decl);
	}
}

void global_scope_t::add_operator_alias(decl_operator_alias &alias_decl)
{
	this->all_symbols.add_operator_alias(alias_decl);
	if (alias_decl.is_export)
	{
		this->export_symbols.add_operator_alias(alias_decl);
	}
}

void global_scope_t::add_type_alias(bz::array_view<bz::u8string_view const> id, decl_type_alias &alias_decl)
{
	this->all_symbols.add_type_alias(id, alias_decl);
	if (alias_decl.is_module_export())
	{
		this->export_symbols.add_type_alias(id, alias_decl);
	}
}

void global_scope_t::add_struct(bz::array_view<bz::u8string_view const> id, decl_struct &struct_decl)
{
	this->all_symbols.add_struct(id, struct_decl);
	if (struct_decl.info.is_module_export())
	{
		this->export_symbols.add_struct(id, struct_decl);
	}
}

void global_scope_t::add_enum(bz::array_view<bz::u8string_view const> id, decl_enum &enum_decl)
{
	this->all_symbols.add_enum(id, enum_decl);
	if (enum_decl.is_module_export())
	{
		this->export_symbols.add_enum(id, enum_decl);
	}
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
