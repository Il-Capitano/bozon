#include "src_manager.h"

namespace ctx
{

[[nodiscard]] bool src_manager::tokenize(void)
{
	for (auto &file : this->_src_files)
	{
		if (!file.read_file())
		{
			bz::printf("error: unable to read file {}\n", file.get_file_name());
			bz::print("exiting...\n");
			return false;
		}

		if (!file.tokenize())
		{
			auto const error_count = this->global_ctx.get_error_count();
			bz::printf(
				"{} {} occurred while tokenizing {}\n",
				error_count, error_count == 1 ? "error" : "errors",
				file.get_file_name()
			);
			file.report_and_clear_errors();
			bz::print("exiting...\n");
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool src_manager::first_pass_parse(void)
{
	for (auto &file : this->_src_files)
	{
		if (!file.first_pass_parse())
		{
			auto const error_count = this->global_ctx.get_error_count();
			bz::printf(
				"{} {} occurred while first pass of parsing {}\n",
				error_count, error_count == 1 ? "error" : "errors",
				file.get_file_name()
			);
			file.report_and_clear_errors();
			bz::print("exiting...\n");
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool src_manager::resolve(void)
{
	for (auto &file : this->_src_files)
	{
		if (!file.resolve())
		{
			auto const error_count = this->global_ctx.get_error_count();
			bz::printf(
				"{} {} occurred while resolving {}\n",
				error_count, error_count == 1 ? "error" : "errors",
				file.get_file_name()
			);
			file.report_and_clear_errors();
			bz::print("exiting...\n");
			return false;
		}
	}

	return true;
}


} // namespace ctx
