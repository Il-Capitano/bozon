// cpp files that are needed for compilation,
// but have no tests yet are included here

#include "ctcli/ctcli.cpp"
#include "global_data.cpp"
#include "src_file.cpp"

#include "ctx/global_context.cpp"
#include "ctx/lex_context.cpp"
#include "ctx/parse_context.cpp"
#include "ctx/comptime_executor.cpp"
#include "ctx/bitcode_context.cpp"
#include "ctx/builtin_operators.cpp"
#include "ctx/error.cpp"
#include "ctx/warnings.cpp"

#include "ast/allocator.cpp"
#include "ast/constant_value.cpp"
#include "ast/expression.cpp"
#include "ast/statement.cpp"
#include "ast/typespec.cpp"

#include "parse/consteval.cpp"
#include "parse/parse_common.cpp"
#include "parse/safe_operations.cpp"
#include "parse/expression_parser.cpp"
#include "parse/statement_parser.cpp"

#include "abi/generic.cpp"
#include "abi/microsoft_x64.cpp"
#include "abi/systemv_amd64.cpp"

#include "bc/runtime/runtime_emit_bitcode.cpp"
#include "bc/comptime/comptime_emit_bitcode.cpp"

