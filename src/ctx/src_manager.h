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
	src_manager(void)
		: _src_files{},
		  _global_ctx(*this)
	{}

	src_file &add_file(bz::u8string_view file_name)
	{
		auto const it = std::find_if(
			this->_src_files.begin(), this->_src_files.end(),
			[file_name](auto const &src) {
				return file_name == src.get_file_name();
			}
		);
		if (it == this->_src_files.end())
		{
			return this->_src_files.emplace_back(file_name, this->_global_ctx);
		}
		else
		{
			return *it;
		}
	}

	bool has_files_to_compile(void) const noexcept
	{ return !this->_src_files.empty(); }

	void report_and_clear_errors_and_warnings(void);

	[[nodiscard]] bool parse_command_line(int argc, char const **argv);
	[[nodiscard]] bool initialize_llvm(void);
	[[nodiscard]] bool parse_global_symbols(void);
	[[nodiscard]] bool parse(void);
	[[nodiscard]] bool emit_bitcode(void);
	[[nodiscard]] bool emit_file(void);

	void optimize(void);
	bool emit_obj(void);
	bool emit_asm(void);
	bool emit_llvm_bc(void);
	bool emit_llvm_ir(void);

	std::list<src_file> &get_src_files(void)
	{ return this->_src_files; }

	std::list<src_file> const &get_src_files(void) const
	{ return this->_src_files; }
};

} // namespace ctx

#endif // CTX_SRC_MANAGER_H
