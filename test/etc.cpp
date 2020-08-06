// cpp files that are needed for compilation,
// but have no tests yet are included here
#include "global_data.cpp"

#include "lex/token.cpp"

#include "ast/typespec.cpp"
#include "ast/expression.cpp"
#include "ast/statement.cpp"

#include "ctx/error.cpp"
#include "ctx/warnings.cpp"
#include "ctx/lex_context.cpp"
#include "ctx/first_pass_parse_context.cpp"
#include "ctx/parse_context.cpp"
#include "ctx/bitcode_context.cpp"
#include "ctx/global_context.cpp"
#include "ctx/built_in_operators.cpp"
#include "ctx/safe_operations.cpp"

#include "bc/emit_bitcode.cpp"
