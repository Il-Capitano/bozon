#include "decl_set.h"

namespace ctx
{

ast::identifier const &get_symbol_id(symbol_t const &symbol)
{
	return symbol.visit(bz::overload{
		[](ast::decl_variable *var_decl) -> auto const & {
			return var_decl->get_id();
		},
		[](variadic_var_decl const &var_decl) -> auto const & {
			return var_decl.original_decl->get_id();
		},
		[](function_overload_set const &func_set) -> auto const & {
			return func_set.id;
		},
		[](ast::decl_type_alias *type_alias_decl) -> auto const & {
			return type_alias_decl->id;
		},
		[](ast::decl_struct *struct_decl) -> auto const & {
			return struct_decl->id;
		},
		[](ast::identifier const &id) -> auto const & {
			return id;
		},
	});
}

lex::src_tokens get_symbol_src_tokens(symbol_t const &symbol)
{
	return symbol.visit(bz::overload{
		[](ast::decl_variable *var_decl) {
			return var_decl->src_tokens;
		},
		[](variadic_var_decl const &var_decl) {
			return var_decl.original_decl->src_tokens;
		},
		[](function_overload_set const &func_set) {
			if (func_set.func_decls.not_empty())
			{
				return func_set.func_decls[0].get<ast::decl_function>().body.src_tokens;
			}
			else
			{
				return func_set.alias_decls[0].get<ast::decl_function_alias>().src_tokens;
			}
		},
		[](ast::decl_type_alias *type_alias_decl) {
			return type_alias_decl->src_tokens;
		},
		[](ast::decl_struct *struct_decl) {
			return struct_decl->info.src_tokens;
		},
		[](ast::identifier const &) {
			bz_unreachable;
			return lex::src_tokens{};
		},
	});
}

symbol_t *decl_set::find_by_id(ast::identifier const &id)
{
	auto const it = std::find_if(
		this->symbols.rbegin(), this->symbols.rend(),
		[&id](auto const &symbol) {
			return get_symbol_id(symbol) == id;
		}
	);
	if (it == this->symbols.rend())
	{
		return nullptr;
	}
	else
	{
		return &*it;
	}
}

static bool is_unqualified_equals(
	ast::identifier const &lhs,
	ast::identifier const &rhs,
	bz::array_view<bz::u8string_view const> current_scope
)
{
	bz_assert(lhs.is_qualified);
	bz_assert(!rhs.is_qualified);
	auto const rhs_size = rhs.values.size();
	return lhs.values.size() >= rhs_size
		&& lhs.values.size() <= rhs_size + current_scope.size()
		&& std::equal(lhs.values.end() - rhs_size, lhs.values.end(), rhs.values.begin())
		&& std::equal(lhs.values.begin(), lhs.values.end() - rhs_size, current_scope.begin());
}

bz::vector<symbol_t *> decl_set::find_by_unqualified_id(
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> current_scope
)
{
	if (id.is_qualified)
	{
		return { this->find_by_id(id) };
	}

	return this->symbols
		.filter([&id, current_scope](auto const &symbol) {
			return is_unqualified_equals(get_symbol_id(symbol), id, current_scope);
		})
		.transform([](auto &symbol) { return &symbol; })
		.collect();
}

bz::vector<symbol_t *> decl_set::find_by_unqualified_id(
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> current_scope,
	bz::array_view<bz::u8string_view const> base_scope
)
{
	if (id.is_qualified)
	{
		return { this->find_by_id(id) };
	}

	return this->symbols
		.filter([&id, current_scope, base_scope](auto const &symbol) {
			auto const &symbol_id = get_symbol_id(symbol);
			return is_unqualified_equals(symbol_id, id, current_scope)
				|| is_unqualified_equals(symbol_id, id, base_scope);
		})
		.transform([](auto &symbol) { return &symbol; })
		.collect();
}

[[nodiscard]] symbol_t *decl_set::add_symbol(symbol_t const &symbol)
{
	if (symbol.is<function_overload_set>())
	{
		return this->add_function_set(symbol.get<function_overload_set>());
	}

	auto const symbol_ptr = this->find_by_id(get_symbol_id(symbol));
	if (symbol_ptr != nullptr)
	{
		return symbol_ptr;
	}
	else
	{
		this->symbols.push_back(symbol);
		return nullptr;
	}
}

[[nodiscard]] symbol_t *decl_set::add_function(ast::statement &stmt)
{
	bz_assert(stmt.is<ast::decl_function>());
	auto &func_decl = stmt.get<ast::decl_function>();
	return this->add_function(func_decl);
}

[[nodiscard]] symbol_t *decl_set::add_function(ast::decl_function &func_decl)
{
	bz_assert(func_decl.id.is_qualified);
	auto const symbol_ptr = this->find_by_id(func_decl.id);
	if (symbol_ptr != nullptr && symbol_ptr->is<function_overload_set>())
	{
		symbol_ptr->get<function_overload_set>().func_decls.emplace_back(&func_decl);
		return nullptr;
	}
	else if (symbol_ptr != nullptr)
	{
		return symbol_ptr;
	}
	else
	{
		this->symbols.push_back(function_overload_set{ func_decl.id, { &func_decl }, {} });
		return nullptr;
	}
}

[[nodiscard]] symbol_t *decl_set::add_function_set(function_overload_set const &func_set)
{
	bz_assert(func_set.id.is_qualified);
	auto const symbol_ptr = this->find_by_id(func_set.id);
	if (symbol_ptr != nullptr && symbol_ptr->is<function_overload_set>())
	{
		symbol_ptr->get<function_overload_set>().func_decls.append(func_set.func_decls);
		symbol_ptr->get<function_overload_set>().alias_decls.append(func_set.alias_decls);
		return nullptr;
	}
	else if (symbol_ptr != nullptr)
	{
		return symbol_ptr;
	}
	else
	{
		this->symbols.push_back(func_set);
		return nullptr;
	}
}

[[nodiscard]] symbol_t *decl_set::add_operator(ast::statement &stmt)
{
	bz_assert(stmt.is<ast::decl_operator>());
	auto &op_decl = stmt.get<ast::decl_operator>();
	return this->add_operator(op_decl);
}

[[nodiscard]] symbol_t *decl_set::add_operator(ast::decl_operator &op_decl)
{
	auto const scope = op_decl.scope.as_array_view();
	auto const op = op_decl.op->kind;
	auto const set = std::find_if(
		this->op_sets.begin(), this->op_sets.end(),
		[op](auto const &set) {
			return op == set.op;
		}
	);
	if (set == this->op_sets.end())
	{
		this->op_sets.push_back({ scope, op, { &op_decl } });
	}
	else
	{
		set->op_decls.emplace_back(&op_decl);
	}
	return nullptr;
}

[[nodiscard]] symbol_t *decl_set::add_operator_set(operator_overload_set const &op_set)
{
	auto const op = op_set.op;
	auto const set = std::find_if(
		this->op_sets.begin(), this->op_sets.end(),
		[op](auto const &set) {
			return op == set.op;
		}
	);
	if (set == this->op_sets.end())
	{
		this->op_sets.push_back(op_set);
	}
	else
	{
		set->op_decls.append(op_set.op_decls);
	}
	return nullptr;
}

[[nodiscard]] symbol_t *decl_set::add_function_alias(ast::decl_function_alias &alias_decl)
{
	bz_assert(alias_decl.id.is_qualified);
	auto const symbol_ptr = this->find_by_id(alias_decl.id);
	if (symbol_ptr != nullptr && symbol_ptr->is<function_overload_set>())
	{
		symbol_ptr->get<function_overload_set>().alias_decls.push_back(&alias_decl);
		return nullptr;
	}
	else if (symbol_ptr != nullptr)
	{
		return symbol_ptr;
	}
	else
	{
		this->symbols.push_back(function_overload_set{ alias_decl.id, {}, { &alias_decl } });
		return nullptr;
	}
}

[[nodiscard]] symbol_t *decl_set::add_type_alias(ast::decl_type_alias &alias_decl)
{
	bz_assert(alias_decl.id.is_qualified);
	auto const symbol_ptr = this->find_by_id(alias_decl.id);
	if (symbol_ptr != nullptr)
	{
		return symbol_ptr;
	}
	else
	{
		this->symbols.push_back(&alias_decl);
		return nullptr;
	}
}

[[nodiscard]] symbol_t *decl_set::add_type(ast::decl_struct &struct_decl)
{
	bz_assert(struct_decl.id.is_qualified);
	auto const symbol_ptr = this->find_by_id(struct_decl.id);
	if (symbol_ptr != nullptr)
	{
		return symbol_ptr;
	}
	else
	{
		this->symbols.push_back(&struct_decl);
		return nullptr;
	}
}

[[nodiscard]] symbol_t *decl_set::add_variable(ast::decl_variable &var_decl)
{
	bz_assert(var_decl.get_id().is_qualified);
	auto const symbol_ptr = this->find_by_id(var_decl.get_id());
	if (symbol_ptr != nullptr)
	{
		return symbol_ptr;
	}
	else
	{
		this->symbols.push_back(&var_decl);
		return nullptr;
	}
}

[[nodiscard]] symbol_t *decl_set::add_variadic_variable(ast::decl_variable &original_decl, bz::vector<ast::decl_variable *> variadic_decls)
{
	bz_assert(original_decl.get_id().is_qualified);
	auto const symbol_ptr = this->find_by_id(original_decl.get_id());
	if (symbol_ptr != nullptr)
	{
		return symbol_ptr;
	}
	else
	{
		this->symbols.push_back(variadic_var_decl{ &original_decl, std::move(variadic_decls) });
		return nullptr;
	}
}

void decl_set::add_local_function(ast::statement &stmt)
{
	bz_assert(stmt.is<ast::decl_function>());
	auto &func_decl = stmt.get<ast::decl_function>();
	this->add_local_function(func_decl);
}

void decl_set::add_local_function(ast::decl_function &func_decl)
{
	auto const symbol_ptr = this->find_by_id(func_decl.id);
	if (symbol_ptr != nullptr && symbol_ptr->is<function_overload_set>())
	{
		symbol_ptr->get<function_overload_set>().func_decls.emplace_back(&func_decl);
	}
	else
	{
		this->symbols.push_back(function_overload_set{ func_decl.id, { &func_decl }, {} });
	}
}

void decl_set::add_local_function_set(function_overload_set const &func_set)
{
	auto const symbol_ptr = this->find_by_id(func_set.id);
	if (symbol_ptr != nullptr && symbol_ptr->is<function_overload_set>())
	{
		symbol_ptr->get<function_overload_set>().func_decls.append(func_set.func_decls);
		symbol_ptr->get<function_overload_set>().alias_decls.append(func_set.alias_decls);
	}
	else
	{
		this->symbols.push_back(func_set);
	}
}

void decl_set::add_local_function_alias(ast::decl_function_alias &alias_decl)
{
	auto const symbol_ptr = this->find_by_id(alias_decl.id);
	if (symbol_ptr != nullptr && symbol_ptr->is<function_overload_set>())
	{
		symbol_ptr->get<function_overload_set>().alias_decls.push_back(&alias_decl);
	}
	else
	{
		this->symbols.push_back(function_overload_set{ alias_decl.id, {}, { &alias_decl } });
	}
}

void decl_set::add_local_type_alias(ast::decl_type_alias &alias_decl)
{
	this->symbols.push_back(&alias_decl);
}

void decl_set::add_local_type(ast::decl_struct &struct_decl)
{
	this->symbols.push_back(&struct_decl);
}

void decl_set::add_local_variable(ast::decl_variable &var_decl)
{
	this->symbols.push_back(&var_decl);
}

void decl_set::add_local_variadic_variable(ast::decl_variable &original_decl, bz::vector<ast::decl_variable *> variadic_decls)
{
	this->symbols.push_back(variadic_var_decl{ &original_decl, std::move(variadic_decls) });
}

void decl_set::add_unresolved_id(ast::identifier id)
{
	this->symbols.push_back(std::move(id));
}

} // namespace ctx
