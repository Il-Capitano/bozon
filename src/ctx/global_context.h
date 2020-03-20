#ifndef CTX_GLOBAL_CONTEXT_H
#define CTX_GLOBAL_CONTEXT_H

#include "core.h"

#include "lex/token.h"
#include "error.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

#include "decl_set.h"

namespace ctx
{

struct file_decls
{
	uint32_t file_id;
	bz::string_view file_name;
	decl_set export_decls;
	decl_set internal_decls;
};

struct global_context
{
private:
	bz::vector<file_decls> _file_decls;
	// using a list here, so iterators aren't invalidated
	std::list<ast::type_info> _types;
	bz::vector<error> _errors;

	file_decls &get_decls(uint32_t file_id);

public:
	global_context(void);

	uint32_t get_file_id(bz::string_view file);

	void report_error(error &&err)
	{ this->_errors.emplace_back(std::move(err)); }

	bool has_errors(void) const
	{ return !this->_errors.empty(); }

	void clear_errors(void)
	{ this->_errors.clear(); }

	bz::vector<error> const &get_errors(void) const
	{ return this->_errors; }

	size_t get_error_count(void) const
	{ return this->_errors.size(); }

	void report_ambiguous_id_error(uint32_t file_id, lex::token_pos id);

	auto add_export_declaration(uint32_t file_id, ast::declaration &decl)
		-> bz::result<int, error>;

	auto add_export_variable(uint32_t file_id, ast::decl_variable &var_decl)
		-> bz::result<int, error>;
	auto add_internal_variable(uint32_t file_id, ast::decl_variable &var_decl)
		-> bz::result<int, error>;

	auto add_export_function(uint32_t file_id, ast::decl_function &func_decl)
		-> bz::result<int, error>;
	auto add_internal_function(uint32_t file_id, ast::decl_function &func_decl)
		-> bz::result<int, error>;

	auto add_export_operator(uint32_t file_id, ast::decl_operator &op_decl)
		-> bz::result<int, error>;
	auto add_internal_operator(uint32_t file_id, ast::decl_operator &op_decl)
		-> bz::result<int, error>;

	auto add_export_struct(uint32_t file_id, ast::decl_struct &struct_decl)
		-> bz::result<int, error>;
	auto add_internal_struct(uint32_t file_id, ast::decl_struct &struct_decl)
		-> bz::result<int, error>;

	ast::type_info const *get_type_info(uint32_t file_id, bz::string_view id);
};

} // namespace ctx

#endif // CTX_GLOBAL_CONTEXT_H
