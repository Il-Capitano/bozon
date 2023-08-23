#include <bz/process.h>
#include <bz/format.h>
#include <bz/u8string.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <future>
#include <thread>
#include <semaphore>
#include "colors.h"

namespace fs = std::filesystem;

#ifdef _WIN32
constexpr bz::u8string_view bozon_default = "bin/windows-debug/bozon.exe";
constexpr bz::u8string_view clang_default = "clang";
constexpr bz::u8string_view os_exe_extension = ".exe";
#else
constexpr bz::u8string_view bozon_default = "bin/linux-debug/bozon";
constexpr bz::u8string_view clang_default = "clang-16";
constexpr bz::u8string_view os_exe_extension = ".out";
#endif

struct thread_pool
{
	thread_pool(size_t thread_count)
		: _task_wait_semaphore(1),
		  _tasks_mutex(),
		  _threads(),
		  _tasks()
	{
		// acquire, because we don't have any tasks yet
		this->_task_wait_semaphore.acquire();

		this->_threads.reserve(thread_count);
		for ([[maybe_unused]] auto const _ : bz::iota(0, thread_count))
		{
			this->_threads.push_back(std::jthread([this]() {
				while (true)
				{
					auto task = this->get_next_task();
					if (!task.has_value())
					{
						return;
					}

					(*task)();
				}
			}));
		}
	}

	~thread_pool(void)
	{
		auto const tasks_guard = std::lock_guard(this->_tasks_mutex);
		if (this->_tasks.empty())
		{
			// all queued tasks have finished, so we need to notify the worker threads to shut down
			this->_task_wait_semaphore.release();
		}
		else
		{
			// some tasks haven't finished yet, so we just discard them
			this->_tasks.clear();
			// the semaphore doesn't need to be released here
		}
		// the threads should clean themselves up here
	}

	// called from main thread
	auto push_task(auto callable) -> std::future<decltype(callable())>
	{
		using R = decltype(callable());
		auto const tasks_guard = std::lock_guard(this->_tasks_mutex);
		auto promise = std::make_shared<std::promise<R>>();
		auto result = promise->get_future();
		this->_tasks.push_back([promise = std::move(promise), callable = std::move(callable)]() {
			promise->set_value(callable());
		});

		// release if a new task is pushed into an empty queue
		if (this->_tasks.size() == 1)
		{
			this->_task_wait_semaphore.release();
		}

		return result;
	}

	// called from worker threads
	bz::optional<std::function<void()>> get_next_task()
	{
		// try to acquire the semaphore
		// blocks until there is at least one task in the queue, or the thread_pool is destructed
		this->_task_wait_semaphore.acquire();
		auto const tasks_guard = std::lock_guard(this->_tasks_mutex);

		if (this->_tasks.empty())
		{
			this->_task_wait_semaphore.release();
			return {};
		}
		else if (this->_tasks.size() == 1)
		{
			// don't release, as we don't have any more tasks
			auto result = std::move(this->_tasks[0]);
			this->_tasks.pop_front();
			return result;
		}
		else
		{
			auto result = std::move(this->_tasks[0]);
			this->_tasks.pop_front();
			// release the semaphore, as there are more tasks left in the queue
			this->_task_wait_semaphore.release();
			return result;
		}
	}

private:
	std::binary_semaphore _task_wait_semaphore;
	std::mutex _tasks_mutex;
	bz::vector<std::jthread> _threads;
	bz::vector<std::function<void()>> _tasks;
};

static void remove_ansi_escape_sequences(bz::u8string &s)
{
	auto sequence_begin = s.find("\033[");
	auto sequence_end = s.find(sequence_begin, 'm');
	auto trail_it = s.data() + (sequence_begin.data() - s.data_as_char_ptr());
	while (sequence_end != s.end())
	{
		auto const next_begin = s.find(sequence_end, "\033[");
		auto const next_end = s.find(next_begin, "m");
		auto const copy_size = (next_begin.data() - sequence_end.data()) - 1;
		std::memmove(trail_it, sequence_end.data() + 1, copy_size);
		trail_it += copy_size;
		sequence_begin = next_begin;
		sequence_end = next_end;
	}

	if (sequence_begin != s.end())
	{
		auto const copy_size = s.end().data() - sequence_begin.data();
		std::memmove(trail_it, sequence_begin.data(), copy_size);
		trail_it += copy_size;
	}

	s.resize(trail_it - s.data());
}

struct test_fail_info_t
{
	bz::vector<bz::u8string> commands;
	bz::u8string test_file;
	bz::process_result_t process_result;
	bz::vector<bz::u8string> wanted_diagnostics;
};

struct test_run_result_t
{
	size_t passed_count;
	size_t total_count;
};

static void print_test_fail_info(test_fail_info_t const &fail_info)
{
	for (auto const &command : fail_info.commands)
	{
		bz::print("{}\n", command);
	}
	if (fail_info.process_result.stdout_string != "")
	{
		bz::print("stdout:\n{}\n", fail_info.process_result.stdout_string);
	}
	if (fail_info.process_result.stderr_string != "")
	{
		bz::print("stderr:\n{}\n", fail_info.process_result.stderr_string);
	}
	bz::print("exit code: {}\n", fail_info.process_result.return_code);
	if (fail_info.wanted_diagnostics.not_empty())
	{
		bz::print("wanted diagnostics:\n");
		for (auto const &diag : fail_info.wanted_diagnostics)
		{
			bz::print("{}\n", diag);
		}
	}
}

static bz::vector<fs::path> get_files_in_folder(fs::path const &folder)
{
	auto result = bz::basic_range(fs::recursive_directory_iterator(folder), fs::recursive_directory_iterator())
		.transform([](auto const &entry) -> auto const & { return entry.path(); })
		.filter([](auto const &path) { return fs::is_regular_file(path) && path.extension() == ".bz"; })
		.transform([](auto const &path) { return fs::relative(fs::canonical(path)); })
		.collect();
	std::stable_sort(result.begin(), result.end(), [](auto const &lhs, auto const &rhs) {
		return std::distance(lhs.begin(), lhs.end()) < std::distance(rhs.begin(), rhs.end());
	});
	return result;
}

static bz::vector<bz::u8string> get_diagnostics_from_output(bz::u8string_view output)
{
	auto error_it = output.find("error: ");
	auto warning_it = output.find("warning: ");
	auto note_it = output.find("note: ");
	auto suggestion_it = output.find("suggestion: ");

	bz::vector<bz::u8string> result;
	while (true)
	{
		auto const min_it = std::min({ error_it, warning_it, note_it, suggestion_it });
		if (min_it == output.end())
		{
			break;
		}
		if (error_it == min_it)
		{
			auto const error_end = output.find_any(error_it, "\r\n");
			result.emplace_back(error_it, error_end);
			error_it = output.find(error_end, "error: ");
		}
		if (warning_it == min_it)
		{
			auto const warning_end = output.find_any(warning_it, "\r\n");
			result.emplace_back(warning_it, warning_end);
			warning_it = output.find(warning_end, "warning: ");
		}
		if (note_it == min_it)
		{
			auto const note_end = output.find_any(note_it, "\r\n");
			result.emplace_back(note_it, note_end);
			note_it = output.find(note_end, "note: ");
		}
		if (suggestion_it == min_it)
		{
			auto const suggestion_end = output.find_any(suggestion_it, "\r\n");
			result.emplace_back(suggestion_it, suggestion_end);
			suggestion_it = output.find(suggestion_end, "suggestion: ");
		}
	}

	return result;
}

static bz::u8string get_behavior_output_from_file(bz::u8string_view filename)
{
	auto const filename_std_string = std::string(filename.data(), filename.size());
	auto file = std::ifstream(filename_std_string);

	bz::u8string result;
	std::string line;
	while (std::getline(file, line))
	{
		auto line_sv = bz::u8string_view(line.data(), line.data() + line.size());
		if (line_sv.ends_with('\r'))
		{
			line_sv = line_sv.substring(0, line_sv.length() - 1);
		}
		if (line_sv.starts_with("// "))
		{
			result += line_sv.substring(3);
			result += '\n';
		}
		else
		{
			break;
		}
	}

	return result;
}

static bz::vector<bz::u8string> get_diagnostics_from_file(bz::u8string_view filename)
{
	auto const filename_std_string = std::string(filename.data(), filename.size());
	auto file = std::ifstream(filename_std_string);

	bz::vector<bz::u8string> result;
	std::string line;
	while (std::getline(file, line))
	{
		auto line_sv = bz::u8string_view(line.data(), line.data() + line.size());
		if (line_sv.ends_with('\r'))
		{
			line_sv = line_sv.substring(0, line_sv.length() - 1);
		}
		if (line_sv.starts_with("// error: "))
		{
			result.push_back(line_sv.substring(3));
		}
		else if (line_sv.starts_with("// warning: "))
		{
			result.push_back(line_sv.substring(3));
		}
		else if (line_sv.starts_with("// note: "))
		{
			result.push_back(line_sv.substring(3));
		}
		else if (line_sv.starts_with("// suggestion: "))
		{
			result.push_back(line_sv.substring(3));
		}
		else
		{
			break;
		}
	}

	return result;
}

static bz::optional<test_fail_info_t> run_behavior_test_file(
	bz::u8string_view bozon,
	bz::vector<bz::u8string> flags,
	bz::u8string_view file,
	bz::u8string_view out_file,
	bz::u8string_view clang,
	bz::u8string_view out_exe
)
{
	flags.push_back(file);
	auto const flags_size = flags.size();
	for (bz::u8string_view const emit_kind : { "obj", "c" })
	{
		// compile
		flags.push_back(bz::format("--emit={}", emit_kind));
		bz::u8string out_file_with_extension = out_file;
		if (emit_kind == "obj")
		{
			out_file_with_extension += ".o";
		}
		else if (emit_kind == "c")
		{
			out_file_with_extension += ".c";
		}
		flags.push_back("-o");
		flags.push_back(out_file_with_extension);
		auto compilation_result = bz::run_process(bozon, flags);
		remove_ansi_escape_sequences(compilation_result.stdout_string);
		remove_ansi_escape_sequences(compilation_result.stderr_string);
		if (compilation_result.return_code != 0 || compilation_result.stdout_string != "" || compilation_result.stderr_string != "")
		{
			return test_fail_info_t{
				.commands = { bz::make_command_string(bozon, flags) },
				.test_file = file,
				.process_result = std::move(compilation_result),
				.wanted_diagnostics = {},
			};
		}

		auto const link_args = bz::array<bz::u8string_view, 3>{
			out_file_with_extension,
			"-o",
			out_exe
		};
		auto link_result = bz::run_process(clang, link_args);
		if (link_result.return_code != 0)
		{
			return test_fail_info_t{
				.commands = { bz::make_command_string(bozon, flags), bz::make_command_string(clang, link_args) },
				.test_file = file,
				.process_result = std::move(link_result),
				.wanted_diagnostics = {},
			};
		}

		auto run_result = bz::run_process(out_exe, bz::array_view<bz::u8string_view const>{});
		run_result.stdout_string.erase('\r');
		auto const expected_output = get_behavior_output_from_file(file);
		if (run_result.return_code != 0 || run_result.stderr_string != "" || run_result.stdout_string != expected_output)
		{
			return test_fail_info_t{
				.commands = {
					bz::make_command_string(bozon, flags),
					bz::make_command_string(clang, link_args),
					bz::make_command_string(out_exe, bz::array_view<bz::u8string_view const>{})
				},
				.test_file = file,
				.process_result = std::move(run_result),
				.wanted_diagnostics = {},
			};
		}

		fs::remove(std::string_view(out_file_with_extension.data_as_char_ptr(), out_file_with_extension.size()));
		fs::remove(std::string_view(out_exe.data(), out_exe.size()));
		flags.resize(flags_size);
	}
	return {};
}

static bz::optional<test_fail_info_t> run_success_test_file(bz::u8string_view bozon, bz::vector<bz::u8string> flags, bz::u8string_view file)
{
	flags.push_back(file);
	for (auto const emit_kind : { "obj", "c" })
	{
		flags.push_back(bz::format("--emit={}", emit_kind));
		auto compilation_result = bz::run_process(bozon, flags);
		remove_ansi_escape_sequences(compilation_result.stdout_string);
		remove_ansi_escape_sequences(compilation_result.stderr_string);
		if (compilation_result.return_code != 0 || compilation_result.stdout_string != "" || compilation_result.stderr_string != "")
		{
			return test_fail_info_t{
				.commands = { bz::make_command_string(bozon, flags) },
				.test_file = file,
				.process_result = std::move(compilation_result),
				.wanted_diagnostics = {},
			};
		}
		flags.pop_back();
	}
	return {};
}

static bz::optional<test_fail_info_t> run_warning_test_file(bz::u8string_view bozon, bz::vector<bz::u8string> flags, bz::u8string_view file)
{
	flags.push_back(file);
	for (auto const emit_kind : { "obj", "c" })
	{
		flags.push_back(bz::format("--emit={}", emit_kind));
		auto compilation_result = bz::run_process(bozon, flags);
		remove_ansi_escape_sequences(compilation_result.stdout_string);
		remove_ansi_escape_sequences(compilation_result.stderr_string);
		auto const diagnostics = get_diagnostics_from_output(compilation_result.stderr_string);
		auto wanted_diagnostics = get_diagnostics_from_file(file);
		if (
			compilation_result.return_code != 0
			|| compilation_result.stdout_string != ""
			|| diagnostics != wanted_diagnostics
		)
		{
			return test_fail_info_t{
				.commands = { bz::make_command_string(bozon, flags) },
				.test_file = file,
				.process_result = std::move(compilation_result),
				.wanted_diagnostics = std::move(wanted_diagnostics),
			};
		}
		flags.pop_back();
	}
	return {};
}

static bz::optional<test_fail_info_t> run_error_test_file(bz::u8string_view bozon, bz::vector<bz::u8string> flags, bz::u8string_view file)
{
	flags.push_back(file);

	auto first_result = bz::run_process(bozon, flags);
	remove_ansi_escape_sequences(first_result.stdout_string);
	remove_ansi_escape_sequences(first_result.stderr_string);

	flags.push_back("--return-zero-on-error");
	auto second_result = bz::run_process(bozon, flags);
	remove_ansi_escape_sequences(second_result.stdout_string);
	remove_ansi_escape_sequences(second_result.stderr_string);
	flags.pop_back();

	auto const diagnostics = get_diagnostics_from_output(first_result.stderr_string);
	auto wanted_diagnostics = get_diagnostics_from_file(file);
	if (
		first_result.return_code == 0
		|| second_result.return_code != 0
		|| first_result.stdout_string != ""
		|| second_result.stdout_string != ""
		|| first_result.stderr_string != second_result.stderr_string
		|| diagnostics != wanted_diagnostics
	)
	{
		return test_fail_info_t{
			.commands = { bz::make_command_string(bozon, flags) },
			.test_file = file,
			.process_result = std::move(first_result),
			.wanted_diagnostics = std::move(wanted_diagnostics),
		};
	}

	return {};
}

static test_run_result_t run_behavior_tests(bz::u8string_view bozon, bz::vector<bz::u8string> common_flags, bz::u8string_view clang, thread_pool &pool)
{
	auto const files = get_files_in_folder("tests/behavior");
	if (files.empty())
	{
		return { .passed_count = 0, .total_count = 0 };
	}
	auto const max_filename_length = files.transform([](auto const &file) { return file.generic_string().length() + 3; }).max(60);

	auto const tests_temp_folder = fs::path("tests/temp");
	fs::create_directories(tests_temp_folder);

	bz::vector<test_fail_info_t> test_fail_infos;

	bz::print("running tests in tests/success:\n");

	auto futures = files.transform([&](auto const &file) {
		auto file_string = bz::u8string(file.generic_string().c_str());
		auto out_file = tests_temp_folder / file.filename();
		auto out_file_string = bz::u8string(out_file.generic_string().c_str());
		out_file += std::string_view(os_exe_extension.data(), os_exe_extension.size());
		auto out_exe_string = bz::u8string(out_file.generic_string().c_str());
		return pool.push_task([
			bozon,
			clang,
			common_flags = common_flags.as_array_view(),
			file_string = std::move(file_string),
			out_file_string = std::move(out_file_string),
			out_exe_string = std::move(out_exe_string)
		]() {
			return run_behavior_test_file(bozon, common_flags, file_string, out_file_string, clang, out_exe_string);
		});
	}).collect();
	bz_assert(futures.size() == files.size());

	size_t passed_count = 0;
	for (auto const i : bz::iota(0, files.size()))
	{
		auto const &file = files[i];
		auto &run_result_future = futures[i];
		auto const file_string_ = file.generic_string();
		auto const file_string = bz::u8string_view(file_string_.data(), file_string_.data() + file_string_.size());
		bz::print("    {:.<{}}", max_filename_length, file_string);
		fflush(stdout);

		auto run_result = run_result_future.get();

		if (run_result.has_value())
		{
			bz::print("{}FAIL{}\n", colors::bright_red, colors::clear);
			print_test_fail_info(run_result.get());
			test_fail_infos.push_back(std::move(run_result.get()));
		}
		else
		{
			bz::print("{}OK{}\n", colors::bright_green, colors::clear);
			passed_count += 1;
		}
	}

	auto const passed_percentage = static_cast<double>(100 * passed_count) / static_cast<double>(files.size());
	auto const color = passed_count == files.size() ? colors::bright_green : colors::bright_red;
	bz::print(
		"{}{}/{}{} ({}{:.2f}%{}) tests passed\n",
		color, passed_count, files.size(), colors::clear,
		color, passed_percentage, colors::clear
	);

	for (auto const &info : test_fail_infos)
	{
		bz::print("\n{}FAILED:{} {}:\n", colors::bright_red, colors::clear, info.test_file);
		print_test_fail_info(info);
	}

	return { passed_count, files.size() };
}

static test_run_result_t run_success_tests(bz::u8string_view bozon, bz::vector<bz::u8string> common_flags, thread_pool &pool)
{
	auto const files = get_files_in_folder("tests/success");
	if (files.empty())
	{
		return { .passed_count = 0, .total_count = 0 };
	}
	auto const max_filename_length = files.transform([](auto const &file) { return file.generic_string().length() + 3; }).max(60);

	bz::vector<test_fail_info_t> test_fail_infos;

	common_flags.push_back("--debug-no-emit-file");
	bz::print("running tests in tests/success:\n");

	auto futures = files.transform([&](auto const &file) {
		auto file_string = bz::u8string(file.generic_string().c_str());
		return pool.push_task([bozon, common_flags = common_flags.as_array_view(), file_string = std::move(file_string)]() {
			return run_success_test_file(bozon, common_flags, file_string);
		});
	}).collect();
	bz_assert(futures.size() == files.size());

	size_t passed_count = 0;
	for (auto const i : bz::iota(0, files.size()))
	{
		auto const &file = files[i];
		auto &run_result_future = futures[i];
		auto const file_string_ = file.generic_string();
		auto const file_string = bz::u8string_view(file_string_.data(), file_string_.data() + file_string_.size());
		bz::print("    {:.<{}}", max_filename_length, file_string);
		fflush(stdout);

		auto run_result = run_result_future.get();

		if (run_result.has_value())
		{
			bz::print("{}FAIL{}\n", colors::bright_red, colors::clear);
			print_test_fail_info(run_result.get());
			test_fail_infos.push_back(std::move(run_result.get()));
		}
		else
		{
			bz::print("{}OK{}\n", colors::bright_green, colors::clear);
			passed_count += 1;
		}
	}

	auto const passed_percentage = static_cast<double>(100 * passed_count) / static_cast<double>(files.size());
	auto const color = passed_count == files.size() ? colors::bright_green : colors::bright_red;
	bz::print(
		"{}{}/{}{} ({}{:.2f}%{}) tests passed\n",
		color, passed_count, files.size(), colors::clear,
		color, passed_percentage, colors::clear
	);

	for (auto const &info : test_fail_infos)
	{
		bz::print("\n{}FAILED:{} {}:\n", colors::bright_red, colors::clear, info.test_file);
		print_test_fail_info(info);
	}

	return { passed_count, files.size() };
}

static test_run_result_t run_warning_tests(bz::u8string_view bozon, bz::vector<bz::u8string> common_flags, thread_pool &pool)
{
	auto const files = get_files_in_folder("tests/warning");
	auto const max_filename_length = files.transform([](auto const &file) { return file.generic_string().length() + 3; }).max(60);

	bz::vector<test_fail_info_t> test_fail_infos;

	common_flags.push_back("--debug-no-emit-file");
	bz::print("running tests in tests/warning:\n");

	auto futures = files.transform([&](auto const &file) {
		auto file_string = bz::u8string(file.generic_string().c_str());
		return pool.push_task([bozon, common_flags = common_flags.as_array_view(), file_string = std::move(file_string)]() {
			return run_warning_test_file(bozon, common_flags, file_string);
		});
	}).collect();
	bz_assert(futures.size() == files.size());

	size_t passed_count = 0;
	for (auto const i : bz::iota(0, files.size()))
	{
		auto const &file = files[i];
		auto &run_result_future = futures[i];
		auto const file_string_ = file.generic_string();
		auto const file_string = bz::u8string_view(file_string_.data(), file_string_.data() + file_string_.size());
		bz::print("    {:.<{}}", max_filename_length, file_string);
		fflush(stdout);

		auto run_result = run_result_future.get();

		if (run_result.has_value())
		{
			bz::print("{}FAIL{}\n", colors::bright_red, colors::clear);
			print_test_fail_info(run_result.get());
			test_fail_infos.push_back(std::move(run_result.get()));
		}
		else
		{
			bz::print("{}OK{}\n", colors::bright_green, colors::clear);
			passed_count += 1;
		}
	}

	auto const passed_percentage = static_cast<double>(100 * passed_count) / static_cast<double>(files.size());
	auto const color = passed_count == files.size() ? colors::bright_green : colors::bright_red;
	bz::print(
		"{}{}/{}{} ({}{:.2f}%{}) tests passed\n",
		color, passed_count, files.size(), colors::clear,
		color, passed_percentage, colors::clear
	);

	for (auto const &info : test_fail_infos)
	{
		bz::print("\n{}FAILED:{} {}:\n", colors::bright_red, colors::clear, info.test_file);
		print_test_fail_info(info);
	}

	return { passed_count, files.size() };
}

static test_run_result_t run_error_tests(bz::u8string_view bozon, bz::vector<bz::u8string> common_flags, thread_pool &pool)
{
	auto const files = get_files_in_folder("tests/error");
	auto const max_filename_length = files.transform([](auto const &file) { return file.generic_string().length() + 3; }).max(60);

	bz::vector<test_fail_info_t> test_fail_infos;

	common_flags.push_back("--emit=null");
	bz::print("running tests in tests/error:\n");

	auto futures = files.transform([&](auto const &file) {
		auto file_string = bz::u8string(file.generic_string().c_str());
		return pool.push_task([bozon, common_flags = common_flags.as_array_view(), file_string = std::move(file_string)]() {
			return run_error_test_file(bozon, common_flags, file_string);
		});
	}).collect();
	bz_assert(futures.size() == files.size());

	size_t passed_count = 0;
	for (auto const i : bz::iota(0, files.size()))
	{
		auto const &file = files[i];
		auto &run_result_future = futures[i];
		auto const file_string_ = file.generic_string();
		auto const file_string = bz::u8string_view(file_string_.data(), file_string_.data() + file_string_.size());
		bz::print("    {:.<{}}", max_filename_length, file_string);
		fflush(stdout);

		auto run_result = run_result_future.get();

		if (run_result.has_value())
		{
			bz::print("{}FAIL{}\n", colors::bright_red, colors::clear);
			print_test_fail_info(run_result.get());
			test_fail_infos.push_back(std::move(run_result.get()));
		}
		else
		{
			bz::print("{}OK{}\n", colors::bright_green, colors::clear);
			passed_count += 1;
		}
	}

	auto const passed_percentage = static_cast<double>(100 * passed_count) / static_cast<double>(files.size());
	auto const color = passed_count == files.size() ? colors::bright_green : colors::bright_red;
	bz::print(
		"{}{}/{}{} ({}{:.2f}%{}) tests passed\n",
		color, passed_count, files.size(), colors::clear,
		color, passed_percentage, colors::clear
	);

	for (auto const &info : test_fail_infos)
	{
		bz::print("\n{}FAILED:{} {}:\n", colors::bright_red, colors::clear, info.test_file);
		print_test_fail_info(info);
	}

	return { passed_count, files.size() };
}

struct test_result_and_name_t
{
	test_run_result_t test_result;
	bz::u8string_view name;
};

struct tests_to_run_t
{
	bool behavior;
	bool success;
	bool warning;
	bool error;
};

int main(int argc, char const * const *argv)
{
	auto const args = bz::basic_range(argv + 1, argv + std::max(argc, 1))
		.transform([](auto const arg) { return bz::u8string_view(arg); })
		.collect();

	bz::u8string_view bozon = "";
	bz::u8string_view clang = "";
	tests_to_run_t tests_to_run = {
		.behavior = true,
		.success = true,
		.warning = true,
		.error = true,
	};
	for (auto const arg : args)
	{
		if (arg.starts_with("--bozon="))
		{
			bozon = arg.substring(bz::u8string_view("--bozon=").length());
		}
		else if (arg.starts_with("--clang="))
		{
			clang = arg.substring(bz::u8string_view("--clang=").length());
		}
		else if (arg.starts_with("--tests="))
		{
			tests_to_run = {
				.behavior = false,
				.success = false,
				.warning = false,
				.error = false,
			};
			auto const tests_to_run_string = arg.substring(bz::u8string_view("--tests=").length());
			auto it = tests_to_run_string.begin();
			while (it != tests_to_run_string.end())
			{
				auto const comma_it = tests_to_run_string.find(it, ',');
				auto const test_kind = bz::u8string_view(it, comma_it);

				if (test_kind == "behavior")
				{
					tests_to_run.behavior = true;
				}
				else if (test_kind == "success")
				{
					tests_to_run.success = true;
				}
				else if (test_kind == "warning")
				{
					tests_to_run.warning = true;
				}
				else if (test_kind == "error")
				{
					tests_to_run.error = true;
				}

				if (comma_it == tests_to_run_string.end())
				{
					break;
				}
				it = comma_it + 1;
			}
		}
	}
	if (bozon == "")
	{
		bozon = bozon_default;
	}
	if (clang == "")
	{
		clang = clang_default;
	}
	if (!tests_to_run.behavior && !tests_to_run.success && !tests_to_run.warning && !tests_to_run.error)
	{
		return 0;
	}

	auto const common_flags = bz::vector<bz::u8string>{
		"--stdlib-dir",
		"bozon-stdlib",
		"-Wall",
		"-Itests/import",
	};

	auto pool = thread_pool(std::thread::hardware_concurrency());

	auto test_results = bz::vector<test_result_and_name_t>();

	if (tests_to_run.behavior)
	{
		test_results.push_back({
			.test_result = run_behavior_tests(bozon, common_flags, clang, pool),
			.name = "tests/behavior",
		});
	}
	if (tests_to_run.success)
	{
		test_results.push_back({
			.test_result = run_success_tests(bozon, common_flags, pool),
			.name = "tests/success",
		});
	}
	if (tests_to_run.warning)
	{
		test_results.push_back({
			.test_result = run_warning_tests(bozon, common_flags, pool),
			.name = "tests/warning",
		});
	}
	if (tests_to_run.error)
	{
		test_results.push_back({
			.test_result = run_error_tests(bozon, common_flags, pool),
			.name = "tests/error",
		});
	}

	auto const passed_count = test_results.transform([](auto const &result) { return result.test_result.passed_count; }).sum();
	auto const total_count = test_results.transform([](auto const &result) { return result.test_result.total_count; }).sum();

	if (total_count != 0)
	{
		auto const print_section_info = [](test_run_result_t const &result, bz::u8string_view section_name) {
			if (result.total_count == 0)
			{
				return;
			}
			auto const color = result.passed_count == result.total_count ? colors::bright_green : colors::bright_red;
			auto const passed_percentage = static_cast<double>(100 * result.passed_count) / static_cast<double>(result.total_count);
			bz::print(
				"    {}{}/{}{} ({}{:.2f}%{}) tests passed in {}\n",
				color, result.passed_count, result.total_count, colors::clear,
				color, passed_percentage, colors::clear,
				section_name
			);
		};

		bz::print("\nsummary:\n");

		for (auto const &result : test_results)
		{
			print_section_info(result.test_result, result.name);
		}

		auto const passed_percentage = static_cast<double>(100 * passed_count) / static_cast<double>(total_count);
		auto const color = passed_count == total_count ? colors::bright_green : colors::bright_red;
		bz::print(
			"{}{}/{}{} ({}{:.2f}%{}) tests passed\n",
			color, passed_count, total_count, colors::clear,
			color, passed_percentage, colors::clear
		);
	}

	return 0;
}
