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
	enum src_file_stage
	{
		constructed,
		file_read,
		tokenized,
		parsed_global_symbols,
		parsed,
	};

public:
	src_file_stage _stage;

	uint32_t               _file_id;
	fs::path               _file_path;
	bz::u8string           _file;
	bz::vector<lex::token> _tokens;

	ctx::global_context       &_global_ctx;
	bz::vector<ast::statement> _declarations;
	ctx::decl_set              _global_decls;
	ctx::decl_set              _export_decls;

public:
	src_file(fs::path file_path, uint32_t file_id, ctx::global_context &global_ctx);

	void add_to_global_decls(ctx::decl_set const &set);

private:
	[[nodiscard]] bool read_file(void);
	[[nodiscard]] bool tokenize(void);

public:
	[[nodiscard]] bool parse_global_symbols(void);
	[[nodiscard]] bool parse(void);


	fs::path const &get_file_path() const
	{ return this->_file_path; }

	auto tokens_begin(void) const
	{ bz_assert(this->_stage >= tokenized); return this->_tokens.begin(); }

	auto tokens_end(void) const
	{ bz_assert(this->_stage >= tokenized); return this->_tokens.end(); }
};

#endif // SRC_FILE_H
