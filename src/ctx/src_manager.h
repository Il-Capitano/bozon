#ifndef CTX_SRC_MANAGER_H
#define CTX_SRC_MANAGER_H

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

#include "src_file.h"
#include "global_context.h"

namespace ctx
{

struct src_manager
{
private:
	std::list<src_file> _src_files;
	global_context _global_ctx;

public:
	void add_file(bz::u8string_view file_name)
	{
		auto const it = std::find_if(
			this->_src_files.begin(), this->_src_files.end(),
			[file_name](auto const &src) {
				return file_name == src.get_file_name();
			}
		);
		if (it == this->_src_files.end())
		{
			this->_src_files.emplace_back(file_name, this->_global_ctx);
		}
	}

	bool has_files_to_compile(void) const noexcept
	{ return !this->_src_files.empty(); }

	[[nodiscard]] bool parse_command_line(int argc, char const **argv);
	[[nodiscard]] bool tokenize(void);
	[[nodiscard]] bool first_pass_parse(void);
	[[nodiscard]] bool resolve(void);
	[[nodiscard]] bool emit_bitcode(void);

	std::list<src_file> const &get_src_files(void) const
	{ return this->_src_files; }
};

} // namespace ctx

#endif // CTX_SRC_MANAGER_H
