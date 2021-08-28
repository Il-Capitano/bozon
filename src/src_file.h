#ifndef SRC_FILE_H
#define SRC_FILE_H

#include "core.h"

#include "lex/token.h"
#include "ctx/decl_set.h"

namespace ctx
{
	struct global_context;
} // namespace ctx

struct src_file
{
	enum src_file_stage : uint8_t
	{
		constructed,
		file_read,
		tokenized,
		parsed_global_symbols,
		parsed,
	};

public:
	src_file_stage _stage;
	bool           _is_library_file;

	uint32_t               _file_id;
	fs::path               _file_path;
	bz::u8string           _file;
	bz::vector<lex::token> _tokens;

	bz::vector<ast::statement> _declarations;
	ctx::decl_set              _global_decls;
	ctx::decl_set              _export_decls;

	bz::vector<bz::u8string_view> _scope;

public:
	src_file(fs::path file_path, uint32_t file_id, bz::vector<bz::u8string_view> scope, bool is_library_file);

private:
	[[nodiscard]] bool read_file(ctx::global_context &global_ctx);
	[[nodiscard]] bool tokenize(ctx::global_context &global_ctx);

public:
	[[nodiscard]] bool parse_global_symbols(ctx::global_context &global_ctx);
	[[nodiscard]] bool parse(ctx::global_context &global_ctx);
	void aggressive_consteval(ctx::global_context &global_ctx);


	fs::path const &get_file_path() const
	{ return this->_file_path; }

	auto tokens_begin(void) const
	{ bz_assert(this->_stage >= tokenized); return this->_tokens.begin(); }

	auto tokens_end(void) const
	{ bz_assert(this->_stage >= tokenized); return this->_tokens.end(); }
};

#endif // SRC_FILE_H
