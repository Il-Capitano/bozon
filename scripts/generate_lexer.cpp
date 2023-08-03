#include "token_info.h"

using token_name_kind_pair = std::pair<bz::u8string_view, uint32_t>;

constexpr auto regular_tokens = []() {
	constexpr auto get_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if ((ti.flags & token_info_flags::keyword) == 0 && ti.token_value != "")
			{
				++count;
			}
		}
		return count;
	};
	using result_t = bz::array<token_name_kind_pair, get_count()>;

	result_t result;

	size_t i = 0;
	for (auto &ti : token_info)
	{
		if ((ti.flags & token_info_flags::keyword) == 0 && ti.token_value != "")
		{
			result[i].first  = ti.token_value;
			result[i].second = ti.kind;
			++i;
		}
	}
	bz_assert(i == result.size());

	constexpr_bubble_sort(
		result,
		[](auto const &lhs, auto const &rhs) {
			auto lhs_it = lhs.first.begin();
			auto rhs_it = rhs.first.begin();
			auto const lhs_end = lhs.first.end();
			auto const rhs_end = rhs.first.end();

			for (; lhs_it != lhs_end && rhs_it != rhs_end; ++lhs_it, ++rhs_it)
			{
				if (*lhs_it < *rhs_it)
				{
					return true;
				}
				else if (*lhs_it > *rhs_it)
				{
					return false;
				}
			}
			return rhs_it != rhs_end;
		},
		[](auto &lhs, auto &rhs) {
			std::swap(lhs, rhs);
		}
	);

	return result;
}();

constexpr auto keywords = []() {
	constexpr auto get_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if (ti.kind != lex::token::_last && (ti.flags & token_info_flags::keyword) != 0)
			{
				++count;
			}
		}
		return count;
	};
	using result_t = bz::array<token_name_kind_pair, get_count()>;

	result_t result;

	size_t i = 0;
	for (auto &ti : token_info)
	{
		if (ti.kind != lex::token::_last && (ti.flags & token_info_flags::keyword) != 0)
		{
			result[i].first  = ti.token_value;
			result[i].second = ti.kind;
			++i;
		}
	}
	bz_assert(i == result.size());

	constexpr_bubble_sort(
		result,
		[](auto const &lhs, auto const &rhs) {
			return lhs.first.length() < rhs.first.length();
		},
		[](auto &lhs, auto &rhs) {
			std::swap(lhs, rhs);
		}
	);

	return result;
}();

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

static bz::u8string generate_regular_token_lexer_helper(bz::array_view<token_name_kind_pair const> tokens, int current_level, uint32_t default_kind)
{
	bz::u8string buffer = "";

	auto const indent = [&buffer, current_level](int extra) {
		buffer += bz::format("{:\t<{}}", 1 + current_level + extra, "");
	};

	size_t i = 0;
	while (i < tokens.size())
	{
		auto const &[token_value, kind] = tokens[i];
		bz_assert(current_level != 0 || token_value.length() == 1);
		auto const c = *(token_value.begin() + current_level);
		indent(0); buffer += bz::format("case '{:c}':\n", c);
		indent(0); buffer += "{\n";
		if (current_level == 0)
		{
			indent(1); buffer += "auto const begin_it = stream.it;\n";
		}
		indent(1); buffer += "++stream;\n";

		auto const next_i = [&]() {
			size_t next_i = i + 1;
			for (; next_i < tokens.size(); ++next_i)
			{
				auto const next_token_value = tokens[next_i].first;
				if (next_token_value.length() <= static_cast<size_t>(current_level))
				{
					return next_i;
				}
				auto const next_c = *(next_token_value.begin() + current_level);
				if (next_c != c)
				{
					return next_i;
				}
			}
			return next_i;
		}();

		if (i + 1 != next_i)
		{
			bz_assert(tokens[i + 1].first.length() == tokens[i].first.length() + 1);
			indent(1); buffer += "if (stream.it == end)\n";
			indent(1); buffer += "{\n";
			indent(2); buffer += bz::format("return make_regular_token({}, begin_it, stream.it, stream.file_id, stream.line, context);\n", kind);
			indent(1); buffer += "}\n";

			indent(1); buffer += "switch (*stream.it)\n";
			indent(1); buffer += "{\n";
			buffer += generate_regular_token_lexer_helper(tokens.slice(i + 1, next_i), current_level + 1, kind);
			indent(1); buffer += "}\n";
		}
		else
		{
			indent(1); buffer += bz::format("return make_regular_token({}, begin_it, stream.it, stream.file_id, stream.line, context);\n", kind);
		}

		// case end
		indent(0); buffer += "}\n";
		i = next_i;
	}

	if (current_level != 0)
	{
		indent(0); buffer += "default:\n";
		indent(0); buffer += "{\n";
		indent(1); buffer += bz::format("return make_regular_token({}, begin_it, stream.it, stream.file_id, stream.line, context);\n", default_kind);
		indent(0); buffer += "}\n";
	}

	return buffer;
}

static void generate_regular_token_lexer(void)
{
	auto const generated_text = generate_regular_token_lexer_helper(regular_tokens, 0, 0);
	auto const file_text = read_text_from_file("src/lex/regular_tokens.inc");

	if (generated_text != file_text)
	{
		auto out_file = std::ofstream("src/lex/regular_tokens.inc");
		out_file << generated_text;
	}
}

static void generate_keyword_lexer()
{
	bz::u8string buffer = "";

	buffer += "\tswitch (id_value.size())\n";
	buffer += "\t{\n";

	size_t current_size = 0;
	for (auto const &[token_string, kind] : keywords)
	{
		if (token_string.size() != current_size)
		{
			if (current_size != 0)
			{
				buffer += "\t\tbreak;\n";
			}
			buffer += bz::format("\tcase {}:\n", token_string.size());
			current_size = token_string.size();
		}

		buffer += bz::format("\t\tif (id_value == \"{}\")\n", token_string);
		buffer += "\t\t{\n";
		buffer += bz::format(
			"\t\t\treturn token(\n"
			"\t\t\t\t{},\n"
			"\t\t\t\tid_value,\n"
			"\t\t\t\tstream.file_id, line, begin_it, end_it\n"
			"\t\t\t);\n",
			kind
		);
		buffer += "\t\t}\n";
	}

	buffer += "\t\tbreak;\n"
		"\t}\n"
		"\treturn token(\n"
		"\t\ttoken::identifier,\n"
		"\t\tid_value,\n"
		"\t\tstream.file_id, line, begin_it, end_it\n"
		"\t);\n";

	auto const file_text = read_text_from_file("src/lex/keywords.inc");
	if (buffer != file_text)
	{
		auto file = std::ofstream("src/lex/keywords.inc");
		file << buffer;
	}
}

int main()
{
	generate_regular_token_lexer();
	generate_keyword_lexer();
}
