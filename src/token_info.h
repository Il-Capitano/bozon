#ifndef TOKEN_INFO_H
#define TOKEN_INFO_H

#include "core.h"
#include "lex/token.h"


struct precedence
{
	int value;
	bool is_left_associative;

	constexpr precedence(void)
		: value(-1), is_left_associative(true)
	{}

	constexpr precedence(int val, bool assoc)
		: value(val), is_left_associative(assoc)
	{}
};

constexpr bool operator < (precedence lhs, precedence rhs)
{
	if (lhs.value == -1)
	{
		return false;
	}
	else if (rhs.value == -1)
	{
		return true;
	}
	else
	{
		return rhs.is_left_associative ? lhs.value < rhs.value : lhs.value <= rhs.value;
	}
}

constexpr bool operator <= (precedence lhs, precedence rhs)
{
	if (lhs.value == -1)
	{
		return false;
	}
	else if (rhs.value == -1)
	{
		return true;
	}
	else
	{
		return lhs.value <= rhs.value;
	}
}

struct token_info_t
{
	uint32_t kind = lex::token::_last;

	bz::u8string_view token_value = "";
	bz::u8string_view token_name  = "";

	uint64_t flags = 0;

	precedence unary_prec  = precedence{};
	precedence binary_prec = precedence{};
};

namespace token_info_flags
{

enum : uint64_t
{
	keyword                          = bit_at< 0>,
	unary_operator                   = bit_at< 1>,
	binary_operator                  = bit_at< 2>,
	operator_                        = bit_at< 3>,
	unary_overloadable               = bit_at< 4>,
	binary_overloadable              = bit_at< 5>,
	overloadable                     = bit_at< 6>,
	valid_expression_or_type_token   = bit_at< 7>,
	unary_builtin                    = bit_at< 8>,
	binary_builtin                   = bit_at< 9>,
	builtin                          = bit_at<10>,
	unary_type_op                    = bit_at<11>,
	binary_type_op                   = bit_at<12>,
	type_op                          = bit_at<13>,
};

} // namespace token_info_flags

struct prec_t
{
	enum { unary, binary, none };
	int op_type;
	uint32_t kind;
	precedence prec;
};

constexpr bz::array operator_precedences = {
	prec_t{ prec_t::unary,  lex::token::plus,               {  3, false } },
	prec_t{ prec_t::unary,  lex::token::minus,              {  3, false } },
	prec_t{ prec_t::unary,  lex::token::plus_plus,          {  3, false } },
	prec_t{ prec_t::unary,  lex::token::minus_minus,        {  3, false } },
	prec_t{ prec_t::unary,  lex::token::bit_not,            {  3, false } },
	prec_t{ prec_t::unary,  lex::token::bool_not,           {  3, false } },
	prec_t{ prec_t::unary,  lex::token::address_of,         {  3, false } },
	prec_t{ prec_t::unary,  lex::token::auto_ref,           {  3, false } },
	prec_t{ prec_t::unary,  lex::token::auto_ref_const,     {  3, false } },
	prec_t{ prec_t::unary,  lex::token::dereference,        {  3, false } },
	prec_t{ prec_t::unary,  lex::token::kw_const,           {  3, false } },
	prec_t{ prec_t::unary,  lex::token::kw_consteval,       {  3, false } },
	prec_t{ prec_t::unary,  lex::token::kw_sizeof,          {  3, false } },
	prec_t{ prec_t::unary,  lex::token::kw_typeof,          {  3, false } },
	prec_t{ prec_t::unary,  lex::token::kw_move,            {  3, false } },
	prec_t{ prec_t::unary,  lex::token::kw_forward,         {  3, false } },
	prec_t{ prec_t::unary,  lex::token::dot_dot_dot,        {  3, false } },

	prec_t{ prec_t::binary, lex::token::kw_as,              {  4, true  } },

	prec_t{ prec_t::binary, lex::token::dot_dot,            {  5, true  } },

	prec_t{ prec_t::binary, lex::token::multiply,           {  6, true  } },
	prec_t{ prec_t::binary, lex::token::divide,             {  6, true  } },
	prec_t{ prec_t::binary, lex::token::modulo,             {  6, true  } },

	prec_t{ prec_t::binary, lex::token::plus,               {  7, true  } },
	prec_t{ prec_t::binary, lex::token::minus,              {  7, true  } },

	prec_t{ prec_t::binary, lex::token::bit_left_shift,     {  8, true  } },
	prec_t{ prec_t::binary, lex::token::bit_right_shift,    {  8, true  } },

	prec_t{ prec_t::binary, lex::token::less_than,          {  9, true  } },
	prec_t{ prec_t::binary, lex::token::less_than_eq,       {  9, true  } },
	prec_t{ prec_t::binary, lex::token::greater_than,       {  9, true  } },
	prec_t{ prec_t::binary, lex::token::greater_than_eq,    {  9, true  } },

	prec_t{ prec_t::binary, lex::token::equals,             { 10, true  } },
	prec_t{ prec_t::binary, lex::token::not_equals,         { 10, true  } },

	prec_t{ prec_t::binary, lex::token::bit_and,            { 11, true  } },
	prec_t{ prec_t::binary, lex::token::bit_xor,            { 12, true  } },
	prec_t{ prec_t::binary, lex::token::bit_or,             { 13, true  } },

	prec_t{ prec_t::binary, lex::token::bool_and,           { 14, true  } },
	prec_t{ prec_t::binary, lex::token::bool_xor,           { 15, true  } },
	prec_t{ prec_t::binary, lex::token::bool_or,            { 16, true  } },

	prec_t{ prec_t::binary, lex::token::assign,             { 18, false } },
	prec_t{ prec_t::binary, lex::token::plus_eq,            { 18, false } },
	prec_t{ prec_t::binary, lex::token::minus_eq,           { 18, false } },
	prec_t{ prec_t::binary, lex::token::multiply_eq,        { 18, false } },
	prec_t{ prec_t::binary, lex::token::divide_eq,          { 18, false } },
	prec_t{ prec_t::binary, lex::token::modulo_eq,          { 18, false } },
	prec_t{ prec_t::binary, lex::token::dot_dot_eq,         { 18, false } },
	prec_t{ prec_t::binary, lex::token::bit_left_shift_eq,  { 18, false } },
	prec_t{ prec_t::binary, lex::token::bit_right_shift_eq, { 18, false } },
	prec_t{ prec_t::binary, lex::token::bit_and_eq,         { 18, false } },
	prec_t{ prec_t::binary, lex::token::bit_xor_eq,         { 18, false } },
	prec_t{ prec_t::binary, lex::token::bit_or_eq,          { 18, false } },

	prec_t{ prec_t::binary, lex::token::comma,              { 20, true  } },
};

constexpr precedence no_assign     { 17, true };
constexpr precedence no_comma      { 19, true };
constexpr precedence call_prec     {  2, true };
constexpr precedence subscript_prec{  2, true };
constexpr precedence dot_prec      {  2, true };

constexpr auto token_info = []() {
	bz::array<token_info_t, lex::token::_last> result{};

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

	constexpr uint64_t builtin_flags        = operator_flags | builtin;
	constexpr uint64_t unary_builtin_flags  = builtin_flags | unary_operator_flags  | unary_builtin;
	constexpr uint64_t binary_builtin_flags = builtin_flags | binary_operator_flags | binary_builtin;
	constexpr uint64_t both_builtin_flags   = unary_builtin_flags | binary_builtin_flags;

	constexpr uint64_t type_op_flags        = operator_flags | type_op;
	constexpr uint64_t unary_type_op_flags  = type_op_flags | unary_operator_flags  | unary_type_op;
	constexpr uint64_t binary_type_op_flags = type_op_flags | binary_operator_flags | binary_type_op;
//	constexpr uint64_t both_type_op_flags   = unary_type_op_flags | binary_type_op_flags;

	result[lex::token::eof]         = { lex::token::eof, "", "end-of-file", 0 };

	result[lex::token::paren_open]    = { lex::token::paren_open,    "(", "", overloadable_flags };
	result[lex::token::paren_close]   = { lex::token::paren_close,   ")", "", expr_type_flags    };
	result[lex::token::curly_open]    = { lex::token::curly_open,    "{", "", expr_type_flags    };
	result[lex::token::curly_close]   = { lex::token::curly_close,   "}", "", 0                  };
	result[lex::token::square_open]   = { lex::token::square_open,   "[", "", overloadable_flags };
	result[lex::token::square_close]  = { lex::token::square_close,  "]", "", expr_type_flags    };
	result[lex::token::semi_colon]    = { lex::token::semi_colon,    ";", "", 0                  };
	result[lex::token::colon]         = { lex::token::colon,         ":", "", expr_type_flags    };
	result[lex::token::question_mark] = { lex::token::question_mark, "?", "", expr_type_flags    };
	result[lex::token::at]            = { lex::token::at,            "@", "", 0                  };


	result[lex::token::assign]       = { lex::token::assign,      "=",  "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::plus]         = { lex::token::plus,        "+",  "", both_builtin_flags   | both_overloadable_flags   };
	result[lex::token::plus_plus]    = { lex::token::plus_plus,   "++", "", unary_builtin_flags  | unary_overloadable_flags  };
	result[lex::token::plus_eq]      = { lex::token::plus_eq,     "+=", "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::minus]        = { lex::token::minus,       "-",  "", both_builtin_flags   | both_overloadable_flags   };
	result[lex::token::minus_minus]  = { lex::token::minus_minus, "--", "", unary_builtin_flags  | unary_overloadable_flags  };
	result[lex::token::minus_eq]     = { lex::token::minus_eq,    "-=", "", binary_builtin_flags | binary_overloadable_flags };
	// dereference
	result[lex::token::multiply]     = { lex::token::multiply,    "*",  "", both_builtin_flags   | both_overloadable_flags   | unary_type_op_flags };
	result[lex::token::multiply_eq]  = { lex::token::multiply_eq, "*=", "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::divide]       = { lex::token::divide,      "/",  "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::divide_eq]    = { lex::token::divide_eq,   "/=", "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::modulo]       = { lex::token::modulo,      "%",  "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::modulo_eq]    = { lex::token::modulo_eq,   "%=", "", binary_builtin_flags | binary_overloadable_flags };

	result[lex::token::auto_ref]       = { lex::token::auto_ref,       "#",  "", unary_type_op_flags };
	result[lex::token::auto_ref_const] = { lex::token::auto_ref_const, "##", "", unary_type_op_flags };

	// bit_and, address_of
	result[lex::token::ampersand]          = { lex::token::ampersand,          "&",   "", both_builtin_flags   | binary_overloadable_flags | unary_type_op_flags };
	result[lex::token::bit_and_eq]         = { lex::token::bit_and_eq,         "&=",  "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::bit_xor]            = { lex::token::bit_xor,            "^",   "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::bit_xor_eq]         = { lex::token::bit_xor_eq,         "^=",  "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::bit_or]             = { lex::token::bit_or,             "|",   "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::bit_or_eq]          = { lex::token::bit_or_eq,          "|=",  "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::bit_left_shift]     = { lex::token::bit_left_shift,     "<<",  "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::bit_left_shift_eq]  = { lex::token::bit_left_shift_eq,  "<<=", "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::bit_right_shift]    = { lex::token::bit_right_shift,    ">>",  "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::bit_right_shift_eq] = { lex::token::bit_right_shift_eq, ">>=", "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::bit_not]            = { lex::token::bit_not,            "~",   "", unary_builtin_flags  | unary_overloadable_flags  };

	result[lex::token::equals]          = { lex::token::equals,          "==", "", binary_builtin_flags | binary_overloadable_flags | binary_type_op_flags };
	result[lex::token::not_equals]      = { lex::token::not_equals,      "!=", "", binary_builtin_flags | binary_overloadable_flags | binary_type_op_flags };
	// angle_open
	result[lex::token::less_than]       = { lex::token::less_than,       "<",  "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::less_than_eq]    = { lex::token::less_than_eq,    "<=", "", binary_builtin_flags | binary_overloadable_flags };
	// angle_close
	result[lex::token::greater_than]    = { lex::token::greater_than,    ">",  "", binary_builtin_flags | binary_overloadable_flags };
	result[lex::token::greater_than_eq] = { lex::token::greater_than_eq, ">=", "", binary_builtin_flags | binary_overloadable_flags };

	result[lex::token::bool_and] = { lex::token::bool_and, "&&", "", binary_builtin_flags };
	result[lex::token::bool_xor] = { lex::token::bool_xor, "^^", "", binary_builtin_flags };
	result[lex::token::bool_or]  = { lex::token::bool_or,  "||", "", binary_builtin_flags };
	result[lex::token::bool_not] = { lex::token::bool_not, "!",  "", unary_builtin_flags  | unary_overloadable_flags  };

	result[lex::token::comma]      = { lex::token::comma,      ",",   "", binary_builtin_flags     };
	result[lex::token::dot_dot]    = { lex::token::dot_dot,    "..",  "", binary_overloadable_flags };
	result[lex::token::dot_dot_eq] = { lex::token::dot_dot_eq, "..=", "", binary_overloadable_flags };

	result[lex::token::dot]         = { lex::token::dot,         ".",   "", expr_type_flags };
	result[lex::token::arrow]       = { lex::token::arrow,       "->",  "", expr_type_flags };
	result[lex::token::fat_arrow]   = { lex::token::fat_arrow,   "=>",  "", expr_type_flags };
	result[lex::token::scope]       = { lex::token::scope,       "::",  "", expr_type_flags };
	result[lex::token::dot_dot_dot] = { lex::token::dot_dot_dot, "...", "", expr_type_flags | unary_type_op_flags | unary_builtin_flags };


	result[lex::token::identifier]             = { lex::token::identifier,             "", "identifier",             expr_type_flags };
	result[lex::token::integer_literal]        = { lex::token::integer_literal,        "", "integer literal",        expr_type_flags };
	result[lex::token::floating_point_literal] = { lex::token::floating_point_literal, "", "floating-point literal", expr_type_flags };
	result[lex::token::hex_literal]            = { lex::token::hex_literal,            "", "hexadecimal literal",    expr_type_flags };
	result[lex::token::oct_literal]            = { lex::token::oct_literal,            "", "octal literal",          expr_type_flags };
	result[lex::token::bin_literal]            = { lex::token::bin_literal,            "", "binary literal",         expr_type_flags };
	result[lex::token::string_literal]         = { lex::token::string_literal,         "", "string literal",         expr_type_flags };
	result[lex::token::raw_string_literal]     = { lex::token::raw_string_literal,     "", "raw string literal",     expr_type_flags };
	result[lex::token::character_literal]      = { lex::token::character_literal,      "", "character literal",      expr_type_flags };

	result[lex::token::kw_true]        = { lex::token::kw_true,        "true",        "", keyword_flags | expr_type_flags };
	result[lex::token::kw_false]       = { lex::token::kw_false,       "false",       "", keyword_flags | expr_type_flags };
	result[lex::token::kw_null]        = { lex::token::kw_null,        "null",        "", keyword_flags | expr_type_flags };
	result[lex::token::kw_unreachable] = { lex::token::kw_unreachable, "unreachable", "", keyword_flags | expr_type_flags };
	result[lex::token::kw_break]       = { lex::token::kw_break,       "break",       "", keyword_flags | expr_type_flags };
	result[lex::token::kw_continue]    = { lex::token::kw_continue,    "continue",    "", keyword_flags | expr_type_flags };


	result[lex::token::kw_if]            = { lex::token::kw_if,            "if",            "", keyword_flags | expr_type_flags };
	result[lex::token::kw_else]          = { lex::token::kw_else,          "else",          "", keyword_flags | expr_type_flags };
	result[lex::token::kw_switch]        = { lex::token::kw_switch,        "switch",        "", keyword_flags | expr_type_flags };
	result[lex::token::kw_while]         = { lex::token::kw_while,         "while",         "", keyword_flags                   };
	result[lex::token::kw_for]           = { lex::token::kw_for,           "for",           "", keyword_flags                   };
	result[lex::token::kw_return]        = { lex::token::kw_return,        "return",        "", keyword_flags                   };
	result[lex::token::kw_function]      = { lex::token::kw_function,      "function",      "", keyword_flags                   };
	result[lex::token::kw_operator]      = { lex::token::kw_operator,      "operator",      "", keyword_flags                   };
	result[lex::token::kw_class]         = { lex::token::kw_class,         "class",         "", keyword_flags                   };
	result[lex::token::kw_struct]        = { lex::token::kw_struct,        "struct",        "", keyword_flags                   };
	result[lex::token::kw_type]          = { lex::token::kw_type,          "type",          "", keyword_flags                   };
	result[lex::token::kw_namespace]     = { lex::token::kw_namespace,     "namespace",     "", keyword_flags                   };
	result[lex::token::kw_using]         = { lex::token::kw_using,         "using",         "", keyword_flags                   };
	result[lex::token::kw_static_assert] = { lex::token::kw_static_assert, "static_assert", "", keyword_flags                   };
	result[lex::token::kw_export]        = { lex::token::kw_export,        "export",        "", keyword_flags                   };
	result[lex::token::kw_import]        = { lex::token::kw_import,        "import",        "", keyword_flags                   };
	result[lex::token::kw_in]            = { lex::token::kw_in,            "in",            "", keyword_flags                   };

	result[lex::token::kw_sizeof] = { lex::token::kw_sizeof, "sizeof", "", keyword_flags | unary_builtin_flags };
	result[lex::token::kw_typeof] = { lex::token::kw_typeof, "typeof", "", keyword_flags | unary_builtin_flags };

	result[lex::token::kw_move]    = { lex::token::kw_move,    "move",      "", keyword_flags | unary_type_op_flags | unary_builtin_flags };
	result[lex::token::kw_forward] = { lex::token::kw_forward, "__forward", "", keyword_flags | unary_builtin_flags };

	result[lex::token::kw_auto]     = { lex::token::kw_auto,     "auto",     "", keyword_flags | expr_type_flags };
	result[lex::token::kw_typename] = { lex::token::kw_typename, "typename", "", keyword_flags | expr_type_flags };
	result[lex::token::kw_let]      = { lex::token::kw_let,      "let",      "", keyword_flags                   };

	result[lex::token::kw_consteval] = { lex::token::kw_consteval, "consteval", "", keyword_flags | unary_type_op_flags | unary_builtin_flags };
	result[lex::token::kw_const]     = { lex::token::kw_const,     "const",     "", keyword_flags | unary_type_op_flags                       };
	// the flags for 'as' are not ideal, as it's hard to express that it takes a non-type lhs and a type rhs
	result[lex::token::kw_as]        = { lex::token::kw_as,        "as",        "", keyword_flags | overloadable_flags | binary_operator      };


	for (auto p : operator_precedences)
	{
		if (p.op_type == prec_t::unary)
		{
			result[p.kind].unary_prec = p.prec;
		}
		else if (p.op_type == prec_t::binary)
		{
			result[p.kind].binary_prec = p.prec;
		}
	}

	// error checking to makes sure precedences are okay
	for (auto ti : result)
	{
		if ((ti.flags & binary_operator) != 0)
		{
			bz_assert(ti.binary_prec.value != -1);
		}
		else
		{
			bz_assert(ti.binary_prec.value == -1);
		}

		if ((ti.flags & unary_operator) != 0)
		{
			bz_assert(ti.unary_prec.value != -1);
		}
		else
		{
			bz_assert(ti.unary_prec.value == -1);
		}
	}

	return result;
}();

using token_name_kind_pair = std::pair<bz::u8string_view, uint32_t>;

constexpr auto make_multi_char_tokens()
{
	constexpr auto get_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if (ti.kind != lex::token::_last && (ti.flags & token_info_flags::keyword) == 0 && ti.token_value.length() > 1)
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
		if (ti.kind != lex::token::_last && (ti.flags & token_info_flags::keyword) == 0 && ti.token_value.length() > 1)
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
			return lhs.first.length() > rhs.first.length();
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs.first = rhs.first;
			lhs.second = rhs.second;
			rhs.first = tmp.first;
			rhs.second = tmp.second;
		}
	);

	return result;
}

constexpr auto make_keywords()
{
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
			return lhs.first.length() > rhs.first.length();
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs.first = rhs.first;
			lhs.second = rhs.second;
			rhs.first = tmp.first;
			rhs.second = tmp.second;
		}
	);

	return result;
}


constexpr auto multi_char_tokens = make_multi_char_tokens();
constexpr auto keywords          = make_keywords();

#define def_token_flag_query(fn_name, flag)                        \
constexpr bool fn_name(uint32_t kind)                              \
{                                                                  \
    return (token_info[kind].flags & token_info_flags::flag) != 0; \
}

def_token_flag_query(is_keyword, keyword)
def_token_flag_query(is_unary_operator,  unary_operator)
def_token_flag_query(is_binary_operator, binary_operator)
def_token_flag_query(is_operator,        operator_)
def_token_flag_query(is_unary_overloadable_operator,  unary_overloadable)
def_token_flag_query(is_binary_overloadable_operator, binary_overloadable)
def_token_flag_query(is_overloadable_operator,        overloadable)
def_token_flag_query(is_valid_expression_or_type_token, valid_expression_or_type_token)
def_token_flag_query(is_unary_builtin_operator,  unary_builtin)
def_token_flag_query(is_binary_builtin_operator, binary_builtin)
def_token_flag_query(is_builtin_operator,        builtin)
def_token_flag_query(is_unary_type_op,  unary_type_op)
def_token_flag_query(is_binary_type_op, binary_type_op)
def_token_flag_query(is_type_op,        type_op)

#undef def_token_flag_query

constexpr precedence get_unary_precedence(uint32_t kind)
{
	return token_info[kind].unary_prec;
}

constexpr precedence get_binary_precedence(uint32_t kind)
{
	return token_info[kind].binary_prec;
}

constexpr precedence get_binary_or_call_precedence(uint32_t kind)
{
	if (kind == lex::token::paren_open)
	{
		return call_prec;
	}
	else if (kind == lex::token::square_open)
	{
		return subscript_prec;
	}
	else if (kind == lex::token::dot || kind == lex::token::arrow)
	{
		return dot_prec;
	}
	else
	{
		return token_info[kind].binary_prec;
	}
}

#endif // TOKEN_INFO_H
