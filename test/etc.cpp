// cpp files that are needed for compilation,
// but have no tests yet are included here
#include "global_data.cpp"
#include "src_file.cpp"

#include "lex/token.cpp"

#include "ast/constant_value.cpp"
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

#include "parse/expression_parser.cpp"
#include "parse/statement_parser.cpp"
#include "parse/parse_common.cpp"

#include "bc/runtime/runtime_emit_bitcode.cpp"
#include "abi/generic.cpp"
#include "abi/microsoft_x64.cpp"
#include "abi/systemv_amd64.cpp"
