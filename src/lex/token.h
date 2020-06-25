#ifndef LEX_TOKEN_H
#define LEX_TOKEN_H

#include "core.h"

namespace lex
{

struct token
{
	enum : uint32_t
	{
		eof           = '\0',

		paren_open    = '(',
		paren_close   = ')',
		curly_open    = '{',
		curly_close   = '}',
		square_open   = '[',
		square_close  = ']',
		angle_open    = '<',
		angle_close   = '>',
		semi_colon    = ';',
		colon         = ':',
		comma         = ',',
		dot           = '.',
		question_mark = '?',


		// ==== operators ====
		// binary
		assign        = '=',
		plus          = '+',
		minus         = '-',
		multiply      = '*',
		divide        = '/',
		modulo        = '%',
		less_than     = '<',
		greater_than  = '>',
		bit_and       = '&',
		bit_xor       = '^',
		bit_or        = '|',

		// unary
		bit_not       = '~',
		bool_not      = '!',
		address_of    = '&',
		dereference   = '*',

		star          = '*',
		ampersand     = '&',

		identifier = 256,
		integer_literal,
		floating_point_literal,
		hex_literal,
		oct_literal,
		bin_literal,
		string_literal,
		character_literal,

		// multi-char tokens
		plus_plus,           // ++
		minus_minus,         // --
		plus_eq,             // +=
		minus_eq,            // -=
		multiply_eq,         // *=
		divide_eq,           // /=
		modulo_eq,           // %=
		bit_left_shift,      // <<
		bit_right_shift,     // >>
		bit_and_eq,          // &=
		bit_xor_eq,          // ^=
		bit_or_eq,           // |=
		bit_left_shift_eq,   // <<=
		bit_right_shift_eq,  // >>=
		equals,              // ==
		not_equals,          // !=
		less_than_eq,        // <=
		greater_than_eq,     // >=
		bool_and,            // &&
		bool_xor,            // ^^
		bool_or,             // ||
		arrow,               // ->
		fat_arrow,           // =>
		scope,               // ::
		dot_dot,             // ..
		dot_dot_eq,          // ..=
		dot_dot_dot,         // ...


		// keywords
		kw_if,               // if
		kw_else,             // else
		kw_while,            // while
		kw_for,              // for
		kw_return,           // return
		kw_function,         // function
		kw_operator,         // operator
		kw_class,            // class
		kw_struct,           // struct
		kw_typename,         // typename
		kw_namespace,        // namespace
		kw_sizeof,           // sizeof
		kw_typeof,           // typeof
		kw_using,            // using

		kw_as,               // as

		kw_auto,             // auto
		kw_let,              // let
		kw_const,            // const

		kw_true,
		kw_false,
		kw_null,

		_last
	};

	uint32_t kind;
	bz::u8string_view value;
	bz::u8string_view postfix;
	struct
	{
		bz::u8string_view file_name;
		bz::u8string_view::const_iterator begin;
		bz::u8string_view::const_iterator end;
		size_t line;
	} src_pos;

	token(
		uint32_t _kind,
		bz::u8string_view _value,
		bz::u8string_view _file_name,
		bz::u8string_view::const_iterator _begin,
		bz::u8string_view::const_iterator _end,
		size_t _line
	)
		: kind   (_kind),
		  value  (_value),
		  postfix(""),
		  src_pos{ _file_name, _begin, _end, _line }
	{}

	token(
		uint32_t _kind,
		bz::u8string_view _value,
		bz::u8string_view _postfix,
		bz::u8string_view _file_name,
		bz::u8string_view::const_iterator _begin,
		bz::u8string_view::const_iterator _end,
		size_t _line
	)
		: kind   (_kind),
		  value  (_value),
		  postfix(_postfix),
		  src_pos{ _file_name, _begin, _end, _line }
	{}
};

struct token_info_t
{
	uint32_t kind = token::_last;

	bz::u8string_view token_value = "";
	bz::u8string_view token_name  = "";

	uint64_t flags = 0;
/*
	bool is_keyword         = false;
	bool is_unary_operator  = false;
	bool is_binary_operator = false;
	bool is_operator        = false;
	bool is_overloadable    = false;
	bool is_valid_expression_or_type_token = false;
*/
};

template<size_t N>
constexpr uint64_t bit_at = uint64_t(1) << N;

namespace token_info_flags
{

enum : uint64_t
{
	keyword             = bit_at<0>,
	unary_operator      = bit_at<1>,
	binary_operator     = bit_at<2>,
	operator_           = bit_at<3>,
	unary_overloadable  = bit_at<4>,
	binary_overloadable = bit_at<5>,
	overloadable        = bit_at<6>,
	valid_expression_or_type_token = bit_at<7>,
};

} // namespace token_info_flags

constexpr auto token_info = []() {
	std::array<token_info_t, token::_last> result{};

	using namespace token_info_flags;

	constexpr uint64_t keyword_flags         = keyword;
	constexpr uint64_t expr_type_flags       = valid_expression_or_type_token;
	constexpr uint64_t operator_flags        = operator_ | valid_expression_or_type_token;
	constexpr uint64_t unary_operator_flags  = unary_operator  | operator_flags;
	constexpr uint64_t binary_operator_flags = binary_operator | operator_flags;
//	constexpr uint64_t both_operator_flags   = unary_operator_flags | binary_operator_flags;

	constexpr uint64_t overloadable_flags        = operator_flags | overloadable;
	constexpr uint64_t unary_overloadable_flags  = unary_operator_flags  | unary_overloadable  | overloadable;
	constexpr uint64_t binary_overloadable_flags = binary_operator_flags | binary_overloadable | overloadable;
	constexpr uint64_t both_overloadable_flags   = unary_overloadable_flags | binary_overloadable_flags;

	result[token::eof]         = { token::eof, "", "end-of-file", 0 };

	result[token::paren_open]    = { token::paren_open,    "(", "", overloadable_flags };
	result[token::paren_close]   = { token::paren_close,   ")", "", expr_type_flags    };
	result[token::curly_open]    = { token::curly_open,    "{", "", 0                  };
	result[token::curly_close]   = { token::curly_close,   "}", "", 0                  };
	result[token::square_open]   = { token::square_open,   "[", "", overloadable_flags };
	result[token::square_close]  = { token::square_close,  "]", "", expr_type_flags    };
	result[token::semi_colon]    = { token::semi_colon,    ";", "", 0                  };
	result[token::colon]         = { token::colon,         ":", "", expr_type_flags    };
	result[token::question_mark] = { token::question_mark, "?", "", expr_type_flags    };


	result[token::assign]       = { token::assign,      "=",  "", binary_overloadable_flags };
	result[token::plus]         = { token::plus,        "+",  "", both_overloadable_flags   };
	result[token::plus_plus]    = { token::plus_plus,   "++", "", unary_overloadable_flags  };
	result[token::plus_eq]      = { token::plus_eq,     "+=", "", binary_overloadable_flags };
	result[token::minus]        = { token::minus,       "-",  "", both_overloadable_flags   };
	result[token::minus_minus]  = { token::minus_minus, "--", "", unary_overloadable_flags  };
	result[token::minus_eq]     = { token::minus_eq,    "-=", "", binary_overloadable_flags };
	// dereference
	result[token::multiply]     = { token::multiply,    "*",  "", both_overloadable_flags   };
	result[token::multiply_eq]  = { token::multiply_eq, "*=", "", binary_overloadable_flags };
	result[token::divide]       = { token::divide,      "/",  "", binary_overloadable_flags };
	result[token::divide_eq]    = { token::divide_eq,   "/=", "", binary_overloadable_flags };
	result[token::modulo]       = { token::modulo,      "%",  "", binary_overloadable_flags };
	result[token::modulo_eq]    = { token::modulo_eq,   "%=", "", binary_overloadable_flags };

	// bit_and, address_of
	result[token::ampersand]          = { token::ampersand,          "&",   "", binary_overloadable_flags | unary_operator_flags };
	result[token::bit_and_eq]         = { token::bit_and_eq,         "&=",  "", binary_overloadable_flags                        };
	result[token::bit_xor]            = { token::bit_xor,            "^",   "", binary_overloadable_flags                        };
	result[token::bit_xor_eq]         = { token::bit_xor_eq,         "^=",  "", binary_overloadable_flags                        };
	result[token::bit_or]             = { token::bit_or,             "|",   "", binary_overloadable_flags                        };
	result[token::bit_or_eq]          = { token::bit_or_eq,          "|=",  "", binary_overloadable_flags                        };
	result[token::bit_left_shift]     = { token::bit_left_shift,     "<<",  "", binary_overloadable_flags                        };
	result[token::bit_left_shift_eq]  = { token::bit_left_shift_eq,  "<<=", "", binary_overloadable_flags                        };
	result[token::bit_right_shift]    = { token::bit_right_shift,    ">>",  "", binary_overloadable_flags                        };
	result[token::bit_right_shift_eq] = { token::bit_right_shift_eq, ">>=", "", binary_overloadable_flags                        };
	result[token::bit_not]            = { token::bit_not,            "~",   "", unary_overloadable_flags                         };

	result[token::equals]     = { token::equals,     "==", "", binary_overloadable_flags };
	result[token::not_equals] = { token::not_equals, "!=", "", binary_overloadable_flags };
	// angle_open
	result[token::less_than]    = { token::less_than,    "<",  "", binary_overloadable_flags };
	result[token::less_than_eq] = { token::less_than_eq, "<=", "", binary_overloadable_flags };
	// angle_close
	result[token::greater_than]    = { token::greater_than,    ">",  "", binary_overloadable_flags };
	result[token::greater_than_eq] = { token::greater_than_eq, ">=", "", binary_overloadable_flags };

	result[token::bool_and] = { token::bool_and, "&&", "", binary_overloadable_flags };
	result[token::bool_xor] = { token::bool_xor, "^^", "", binary_overloadable_flags };
	result[token::bool_or]  = { token::bool_or,  "||", "", binary_overloadable_flags };
	result[token::bool_not] = { token::bool_not, "!",  "", unary_overloadable_flags  };

	result[token::comma]      = { token::comma,      ",",   "", binary_operator_flags     };
	result[token::dot_dot]    = { token::dot_dot,    "..",  "", binary_overloadable_flags };
	result[token::dot_dot_eq] = { token::dot_dot_eq, "..=", "", binary_overloadable_flags };

	result[token::dot]         = { token::dot,         ".",   "", expr_type_flags };
	result[token::arrow]       = { token::arrow,       "->",  "", expr_type_flags };
	result[token::fat_arrow]   = { token::fat_arrow,   "=>",  "", expr_type_flags };
	result[token::scope]       = { token::scope,       "::",  "", expr_type_flags };
	result[token::dot_dot_dot] = { token::dot_dot_dot, "...", "", expr_type_flags };


	result[token::identifier]             = { token::identifier,             "", "identifier",             expr_type_flags };
	result[token::integer_literal]        = { token::integer_literal,        "", "integer literal",        expr_type_flags };
	result[token::floating_point_literal] = { token::floating_point_literal, "", "floating-point literal", expr_type_flags };
	result[token::hex_literal]            = { token::hex_literal,            "", "hexadecimal literal",    expr_type_flags };
	result[token::oct_literal]            = { token::oct_literal,            "", "octal literal",          expr_type_flags };
	result[token::bin_literal]            = { token::bin_literal,            "", "binary literal",         expr_type_flags };
	result[token::string_literal]         = { token::string_literal,         "", "string literal",         expr_type_flags };
	result[token::character_literal]      = { token::character_literal,      "", "character literal",      expr_type_flags };

	result[token::kw_true]  = { token::kw_true,  "true",  "", keyword_flags | expr_type_flags };
	result[token::kw_false] = { token::kw_false, "false", "", keyword_flags | expr_type_flags };
	result[token::kw_null]  = { token::kw_null,  "null",  "", keyword_flags | expr_type_flags };


	result[token::kw_if]        = { token::kw_if,        "if",        "", keyword_flags                        };
	result[token::kw_else]      = { token::kw_else,      "else",      "", keyword_flags                        };
	result[token::kw_while]     = { token::kw_while,     "while",     "", keyword_flags                        };
	result[token::kw_for]       = { token::kw_for,       "for",       "", keyword_flags                        };
	result[token::kw_return]    = { token::kw_return,    "return",    "", keyword_flags                        };
	result[token::kw_function]  = { token::kw_function,  "function",  "", keyword_flags                        };
	result[token::kw_operator]  = { token::kw_operator,  "operator",  "", keyword_flags                        };
	result[token::kw_class]     = { token::kw_class,     "class",     "", keyword_flags                        };
	result[token::kw_struct]    = { token::kw_struct,    "struct",    "", keyword_flags                        };
	result[token::kw_typename]  = { token::kw_typename,  "typename",  "", keyword_flags                        };
	result[token::kw_namespace] = { token::kw_namespace, "namespace", "", keyword_flags                        };
	result[token::kw_sizeof]    = { token::kw_sizeof,    "sizeof",    "", keyword_flags | unary_operator_flags };
	result[token::kw_typeof]    = { token::kw_typeof,    "typeof",    "", keyword_flags | unary_operator_flags };
	result[token::kw_using]     = { token::kw_using,     "using",     "", keyword_flags                        };

	result[token::kw_auto]  = { token::kw_auto,  "auto",  "", keyword_flags | expr_type_flags };
	result[token::kw_let]   = { token::kw_let,   "let",   "", keyword_flags                   };
	result[token::kw_const] = { token::kw_const, "const", "", keyword_flags | expr_type_flags };

	result[token::kw_as]    = { token::kw_as,    "as",    "", keyword_flags | expr_type_flags };

	return result;
}();

using token_name_kind_pair = std::pair<bz::u8string_view, uint32_t>;

constexpr auto make_multi_char_tokens()
{
	constexpr auto get_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if (ti.kind != token::_last && (ti.flags & token_info_flags::keyword) == 0 && ti.token_value.length() > 1)
			{
				++count;
			}
		}
		return count;
	};
	using result_t = std::array<token_name_kind_pair, get_count()>;

	result_t result;

	size_t i = 0;
	for (auto &ti : token_info)
	{
		if (ti.kind != token::_last && (ti.flags & token_info_flags::keyword) == 0 && ti.token_value.length() > 1)
		{
			result[i].first  = ti.token_value;
			result[i].second = ti.kind;
			++i;
		}
	}
	bz_assert(i == result.size());

	// bubble sort, because std::sort is not constexpr in c++17
	for (size_t i = 0; i < result.size(); ++i)
	{
		for (size_t j = 0; j < result.size() - 1; ++j)
		{
			auto &lhs = result[j];
			auto &rhs = result[j + 1];
			if (lhs.first.length() < rhs.first.length())
			{
				auto const tmp = lhs;
				lhs.first  = rhs.first;
				lhs.second = rhs.second;
				rhs.first  = tmp.first;
				rhs.second = tmp.second;
			}
		}
	}

	return result;
}

constexpr auto make_keywords()
{
	constexpr auto get_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if (ti.kind != token::_last && (ti.flags & token_info_flags::keyword) != 0)
			{
				++count;
			}
		}
		return count;
	};
	using result_t = std::array<token_name_kind_pair, get_count()>;

	result_t result;

	size_t i = 0;
	for (auto &ti : token_info)
	{
		if (ti.kind != token::_last && (ti.flags & token_info_flags::keyword) != 0)
		{
			result[i].first  = ti.token_value;
			result[i].second = ti.kind;
			++i;
		}
	}
	bz_assert(i == result.size());

	// bubble sort, because std::sort is not constexpr in c++17
	for (size_t i = 0; i < result.size(); ++i)
	{
		for (size_t j = 0; j < result.size() - 1; ++j)
		{
			auto &lhs = result[j];
			auto &rhs = result[j + 1];
			if (lhs.first.length() < rhs.first.length())
			{
				auto const tmp = lhs;
				lhs.first  = rhs.first;
				lhs.second = rhs.second;
				rhs.first  = tmp.first;
				rhs.second = tmp.second;
			}
		}
	}

	return result;
}


constexpr auto multi_char_tokens = make_multi_char_tokens();
constexpr auto keywords          = make_keywords();


constexpr bz::u8string_view get_token_value(uint32_t kind)
{
	bz_assert(kind < token_info.size());
	auto &ti = token_info[kind];
	if (ti.token_value.size() == 0)
	{
		return ti.token_name;
	}
	else
	{
		return ti.token_value;
	}
}

inline bz::u8string get_token_name_for_message(uint32_t kind)
{
	bz_assert(kind < token_info.size());
	auto &ti = token_info[kind];
	if (ti.token_value.size() == 0)
	{
		return ti.token_name;
	}
	else
	{
		return bz::format("'{}'", ti.token_value);
	}
}


constexpr bool is_unary_operator(uint32_t kind)
{
	return (token_info[kind].flags & token_info_flags::unary_operator) != 0;
}

constexpr bool is_binary_operator(uint32_t kind)
{
	return (token_info[kind].flags & token_info_flags::binary_operator) != 0;
}

constexpr bool is_operator(uint32_t kind)
{
	return (token_info[kind].flags & token_info_flags::operator_) != 0;
}

constexpr bool is_overloadable_unary_operator(uint32_t kind)
{
	return (token_info[kind].flags & token_info_flags::unary_overloadable) != 0;
}

constexpr bool is_overloadable_binary_operator(uint32_t kind)
{
	return (token_info[kind].flags & token_info_flags::binary_overloadable) != 0;
}

constexpr bool is_overloadable_operator(uint32_t kind)
{
	return (token_info[kind].flags & token_info_flags::overloadable) != 0;
}

constexpr bool is_valid_expression_or_type_token(uint32_t kind)
{
	return (token_info[kind].flags & token_info_flags::valid_expression_or_type_token) != 0;
}


using token_pos = bz::vector<token>::const_iterator;

struct token_range
{
	token_pos begin = nullptr;
	token_pos end   = nullptr;
};

struct src_tokens
{
	token_pos begin;
	token_pos pivot;
	token_pos end;

	src_tokens(void) noexcept
		: begin(nullptr), pivot(nullptr), end(nullptr)
	{}

	src_tokens(token_pos _begin, token_pos _pivot, token_pos _end) noexcept
		: begin(_begin), pivot(_pivot), end(_end)
	{}
};

} // namespace lex

#endif // LEX_TOKEN_H
