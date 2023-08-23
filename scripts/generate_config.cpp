#include <bz/process.h>
#include <bz/u8string.h>
#include <bz/optional.h>
#include <bz/ranges.h>
#include <bz/vector.h>
#include <bz/format.h>
#include <fstream>
#include <vector>

static bz::u8string read_text_from_file(char const *path)
{
	auto file = std::ifstream(path);
	std::vector<char> file_content{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>()
	};
	auto const file_content_view = bz::u8string_view(
		file_content.data(),
		file_content.data() + file_content.size()
	);


	bz::u8string file_str = file_content_view;
	file_str.erase('\r');
	return file_str;
}

static bool is_whitespace(bz::u8char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void trim(bz::u8string &s)
{
	auto const data_begin = s.data();
	auto const size = s.size();
	auto const data_end = data_begin + size;

	size_t begin_trim_size = 0;
	auto begin_it = data_begin;
	while (begin_it != data_end && is_whitespace(*begin_it))
	{
		++begin_it;
		begin_trim_size += 1;
	}

	size_t end_trim_size = 0;
	auto end_it = data_end;
	while (end_it != begin_it && is_whitespace(*(end_it - 1)))
	{
		--end_it;
		end_trim_size += 1;
	}

	if (begin_trim_size != 0)
	{
		std::memmove(data_begin, data_begin + begin_trim_size, size - begin_trim_size);
	}
	s.resize(size - begin_trim_size - end_trim_size);
}

static bz::u8string to_lower(bz::u8string_view s)
{
	bz::u8string result;
	result.reserve(s.size());
	for (auto const c : s)
	{
		result += std::tolower(c);
	}
	return result;
}

static bz::u8string to_upper(bz::u8string_view s)
{
	bz::u8string result;
	result.reserve(s.size());
	for (auto const c : s)
	{
		result += std::toupper(c);
	}
	return result;
}

static bool has_llvm(void)
{
	return bz::run_process("llvm-config", bz::array_view<char const * const>{ "--version" }).return_code != 0
		|| bz::run_process("llvm-config-16", bz::array_view<char const * const>{ "--version" }).return_code != 0;
}

static bz::optional<bz::u8string> get_llvm_default_target(void)
{
	{
		auto process_result = bz::run_process("llvm-config", bz::array_view<char const * const>{ "--host-target" });
		if (process_result.return_code == 0)
		{
			trim(process_result.stdout_string);
			return std::move(process_result.stdout_string);
		}
	}

	{
		auto process_result = bz::run_process("llvm-config-16", bz::array_view<char const * const>{ "--host-target" });
		if (process_result.return_code == 0)
		{
			trim(process_result.stdout_string);
			return std::move(process_result.stdout_string);
		}
	}

	return {};
}

static constexpr bz::u8string_view native_target_triple_detection = R"(#ifndef BOZON_CONFIG_NATIVE_TARGET
#if defined(__x86_64__) || defined(_M_X64)
#define BOZON_CONFIG_NATIVE_TARGET_ARCH "x86_64"
#else
#define BOZON_CONFIG_NATIVE_TARGET_ARCH "unknown"
#endif
#define BOZON_CONFIG_NATIVE_TARGET_VENDOR "unknown"
#if defined(_WIN32)
#define BOZON_CONFIG_NATIVE_TARGET_OS "windows"
#elif defined(__linux__)
#define BOZON_CONFIG_NATIVE_TARGET_OS "linux"
#else
#define BOZON_CONFIG_NATIVE_TARGET_OS "unknown"
#endif
#define BOZON_CONFIG_NATIVE_TARGET_ENV "unknown"
#define BOZON_CONFIG_NATIVE_TARGET BOZON_CONFIG_NATIVE_TARGET_ARCH "-" BOZON_CONFIG_NATIVE_TARGET_VENDOR "-" BOZON_CONFIG_NATIVE_TARGET_OS "-" BOZON_CONFIG_NATIVE_TARGET_ENV
#endif
)";

static constexpr bz::array<bz::u8string_view, 2> available_backends = { "llvm", "c" };

struct config_t
{
	bz::optional<bz::u8string> target;
	bz::optional<bz::vector<bz::u8string>> backends;
};

static bz::vector<bz::u8string> split(bz::u8string_view s, bz::u8char c)
{
	bz::vector<bz::u8string> result;

	if (s.size() == 0)
	{
		return result;
	}

	auto begin = s.begin();
	auto it = s.find(c);

	while (it != s.end())
	{
		result.push_back(bz::u8string_view(begin, it));
		begin = it + 1;
		it = s.find(begin, c);
	}
	result.push_back(bz::u8string_view(begin, it));

	return result;
}

static bz::u8string config_variable_name(bz::u8string_view backend)
{
	return bz::format("backend_{}", to_lower(backend));
}

static bz::u8string config_macro_name(bz::u8string_view backend)
{
	return bz::format("BOZON_CONFIG_BACKEND_{}", to_upper(backend));
}

int main(int argc, char const * const *argv)
{
	auto config = config_t();

	for (bz::u8string_view const arg : bz::array_view(argv + std::min(argc, 1), argv + argc))
	{
		if (arg.starts_with("--target="))
		{
			auto const target = arg.substring(bz::u8string_view("--target=").length());
			if (!config.target.has_value())
			{
				config.target = target;
			}
		}
		else if (arg.starts_with("--backends="))
		{
			auto const backends = arg.substring(bz::u8string_view("--backends=").length());
			if (!config.backends.has_value())
			{
				config.backends = split(backends, ',');
			}
		}
	}

	if (!config.target.has_value())
	{
		config.target = get_llvm_default_target();
	}

	if (!config.backends.has_value())
	{
		config.backends = available_backends.transform([](auto const s) { return bz::u8string(s); }).collect();
	}

	bz::u8string config_file_string = "#ifndef CONFIG_H\n#define CONFIG_H\n\nnamespace config\n{\n\n";

	for (auto const &backend_caseless : config.backends.get())
	{
		auto const backend = to_lower(backend_caseless);
		if (!available_backends.contains(backend))
		{
			bz::print("error: unknown backend '{}'\n", backend_caseless);
			return 1;
		}
	}

	auto const contains_lowercase = [](bz::array_view<bz::u8string const> arr, bz::u8string_view s) {
		auto const it = std::find_if(arr.begin(), arr.end(), [&](auto const &elem) {
			return to_lower(elem) == s;
		});
		return it != arr.end();
	};

	for (auto const &backend : available_backends)
	{
		if (contains_lowercase(config.backends.get(), backend))
		{
			if (backend == "llvm" && !has_llvm())
			{
				bz::print("error: unable to find 'llvm-config', while the LLVM backend is enabled\n");
			}

			config_file_string += bz::format("inline constexpr bool {} = true;\n", config_variable_name(backend));
			config_file_string += bz::format("#define {}\n", config_macro_name(backend));
		}
		else
		{
			config_file_string += bz::format("inline constexpr bool {} = false;\n", config_variable_name(backend));
		}
	}

	if (config.target.has_value())
	{
		config_file_string += bz::format("#define BOZON_CONFIG_NATIVE_TARGET \"{}\"\n", config.target.get());
	}
	else
	{
		config_file_string += native_target_triple_detection;
	}

	config_file_string += "\n} // namespace config\n\n#endif // CONFIG_H\n";

	auto const file_text = read_text_from_file("src/config.h");

	if (config_file_string != file_text)
	{
		auto out_file = std::ofstream("src/config.h");
		out_file << config_file_string;
	}
}
