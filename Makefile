# compiler parameters
CXX := clang++
CC := clang

# linker parameters
LD := clang++

# debug build by default
default: debug

# =========================
# ======== default ========
# =========================

default_cxx_debug_flags := -c -g -Og -Wall -Wextra -std=c++2a -march=native
default_cxx_release_flags := -c -O3 -Wall -Wextra -std=c++2a -march=native
default_cxx_include_dirs := ./include ./src
default_cc_debug_flags := -c -g -Og -Wall -Wextra -std=c11
default_cc_release_flags := -c -O3 -Wall -Wextra -std=c11
default_cc_include_dirs := ./include ./src
default_ld_debug_flags :=  -g
default_ld_release_flags :=
default_ld_lib_dirs :=
default_ld_libs := LLVM
default_exe_debug := bin/default/debug/bozon.exe
default_exe_release := bin/default/release/bozon.exe

# all binary files
# used for clean-up
all_obj_debug := $(wildcard bin/default/debug/int/*.o)
all_exe_debug := $(wildcard bin/default/debug/*.exe)

all_obj_release := $(wildcard bin/default/release/int/*.o)
all_exe_release := $(wildcard bin/default/release/*.exe)

# used for clean-up
rebuildables_debug := $(all_exe_debug) $(all_obj_debug)
rebuildables_release := $(all_exe_release) $(all_obj_release)

# deletes the binary files
clean: clean-debug clean-release

# deletes debug binary files
clean-debug:
	rm -f $(rebuildables_debug)

# deletes release binary files
clean-release:
	rm -f $(rebuildables_release)

# debug build
debug: $(default_exe_debug)

# release build
release: $(default_exe_release)

# debug rebuild
rebuild-debug: clean-debug debug

# release rebuild
rebuild-release: clean-release release

# builds and executes the exe
exec: exec-debug

exec-debug: $(default_exe_debug)
	$(default_exe_debug)

exec-release: $(default_exe_release)
	$(default_exe_release)

$(default_exe_debug): bin/default/debug/int/first_pass_parser.cpp.o bin/default/debug/int/main.cpp.o bin/default/debug/int/parser.cpp.o bin/default/debug/int/src_file.cpp.o bin/default/debug/int/expression.cpp.o bin/default/debug/int/statement.cpp.o bin/default/debug/int/typespec.cpp.o bin/default/debug/int/emit_bitcode.cpp.o bin/default/debug/int/bitcode_context.cpp.o bin/default/debug/int/built_in_operators.cpp.o bin/default/debug/int/error.cpp.o bin/default/debug/int/first_pass_parse_context.cpp.o bin/default/debug/int/global_context.cpp.o bin/default/debug/int/lex_context.cpp.o bin/default/debug/int/parse_context.cpp.o bin/default/debug/int/src_manager.cpp.o bin/default/debug/int/lexer.cpp.o bin/default/debug/int/token.cpp.o
	$(LD) $(default_ld_debug_flags) $^ -o $@ $(addprefix -L,$(default_ld_lib_dirs)) $(addprefix -l,$(default_ld_libs))

$(default_exe_release): bin/default/release/int/first_pass_parser.cpp.o bin/default/release/int/main.cpp.o bin/default/release/int/parser.cpp.o bin/default/release/int/src_file.cpp.o bin/default/release/int/expression.cpp.o bin/default/release/int/statement.cpp.o bin/default/release/int/typespec.cpp.o bin/default/release/int/emit_bitcode.cpp.o bin/default/release/int/bitcode_context.cpp.o bin/default/release/int/built_in_operators.cpp.o bin/default/release/int/error.cpp.o bin/default/release/int/first_pass_parse_context.cpp.o bin/default/release/int/global_context.cpp.o bin/default/release/int/lex_context.cpp.o bin/default/release/int/parse_context.cpp.o bin/default/release/int/src_manager.cpp.o bin/default/release/int/lexer.cpp.o bin/default/release/int/token.cpp.o
	$(LD) $(default_ld_release_flags) $^ -o $@ $(addprefix -L,$(default_ld_lib_dirs)) $(addprefix -l,$(default_ld_libs))

bin/default/debug/int/first_pass_parser.cpp.o: ./src/first_pass_parser.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/first_pass_parser.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/first_pass_parser.cpp.o: ./src/first_pass_parser.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/first_pass_parser.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/main.cpp.o: ./src/main.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/bitcode_context.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/ctx/parse_context.h ./src/ctx/src_manager.h ./src/first_pass_parser.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h ./src/src_file.h ./src/timer.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/main.cpp.o: ./src/main.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/bitcode_context.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/ctx/parse_context.h ./src/ctx/src_manager.h ./src/first_pass_parser.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h ./src/src_file.h ./src/timer.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/parser.cpp.o: ./src/parser.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/parse_context.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/parser.cpp.o: ./src/parser.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/parse_context.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/src_file.cpp.o: ./src/src_file.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/bc/emit_bitcode.h ./src/colors.h ./src/core.h ./src/ctx/bitcode_context.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/ctx/parse_context.h ./src/first_pass_parser.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h ./src/src_file.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/src_file.cpp.o: ./src/src_file.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/bc/emit_bitcode.h ./src/colors.h ./src/core.h ./src/ctx/bitcode_context.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/ctx/parse_context.h ./src/first_pass_parser.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h ./src/src_file.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/expression.cpp.o: ./src/ast/expression.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/typespec.h ./src/core.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/expression.cpp.o: ./src/ast/expression.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/typespec.h ./src/core.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/statement.cpp.o: ./src/ast/statement.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/statement.cpp.o: ./src/ast/statement.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/typespec.cpp.o: ./src/ast/typespec.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/typespec.cpp.o: ./src/ast/typespec.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/emit_bitcode.cpp.o: ./src/bc/emit_bitcode.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/bc/emit_bitcode.h ./src/colors.h ./src/core.h ./src/ctx/bitcode_context.h ./src/ctx/built_in_operators.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/parse_context.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/emit_bitcode.cpp.o: ./src/bc/emit_bitcode.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/bc/emit_bitcode.h ./src/colors.h ./src/core.h ./src/ctx/bitcode_context.h ./src/ctx/built_in_operators.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/parse_context.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/bitcode_context.cpp.o: ./src/ctx/bitcode_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/bitcode_context.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/bitcode_context.cpp.o: ./src/ctx/bitcode_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/bitcode_context.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/built_in_operators.cpp.o: ./src/ctx/built_in_operators.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/built_in_operators.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/parse_context.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/built_in_operators.cpp.o: ./src/ctx/built_in_operators.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/built_in_operators.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/parse_context.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/error.cpp.o: ./src/ctx/error.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/core.h ./src/ctx/error.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/error.cpp.o: ./src/ctx/error.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/core.h ./src/ctx/error.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/first_pass_parse_context.cpp.o: ./src/ctx/first_pass_parse_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/first_pass_parse_context.cpp.o: ./src/ctx/first_pass_parse_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/global_context.cpp.o: ./src/ctx/global_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/parse_context.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/global_context.cpp.o: ./src/ctx/global_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/parse_context.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/lex_context.cpp.o: ./src/ctx/lex_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/lex/file_iterator.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/lex_context.cpp.o: ./src/ctx/lex_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/lex/file_iterator.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/parse_context.cpp.o: ./src/ctx/parse_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/built_in_operators.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/parse_context.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/parse_context.cpp.o: ./src/ctx/parse_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/core.h ./src/ctx/built_in_operators.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/parse_context.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/src_manager.cpp.o: ./src/ctx/src_manager.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/bc/emit_bitcode.h ./src/core.h ./src/ctx/bitcode_context.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/ctx/parse_context.h ./src/ctx/src_manager.h ./src/first_pass_parser.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h ./src/src_file.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/src_manager.cpp.o: ./src/ctx/src_manager.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/bc/emit_bitcode.h ./src/core.h ./src/ctx/bitcode_context.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/ctx/parse_context.h ./src/ctx/src_manager.h ./src/first_pass_parser.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./src/parser.h ./src/src_file.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/lexer.cpp.o: ./src/lex/lexer.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/colors.h ./src/core.h ./src/ctx/error.h ./src/ctx/lex_context.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/lexer.cpp.o: ./src/lex/lexer.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/colors.h ./src/core.h ./src/ctx/error.h ./src/ctx/lex_context.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/token.cpp.o: ./src/lex/token.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/core.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/token.cpp.o: ./src/lex/token.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/core.h ./src/lex/token.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

# ======================
# ======== test ========
# ======================

test_cxx_debug_flags := -c -g -Og -Wall -Wextra -std=c++17
test_cxx_release_flags := -c -O3 -Wall -Wextra -std=c++17
test_cxx_include_dirs := ./include ./src
test_cc_debug_flags := -c -g -Og -Wall -Wextra -std=c11
test_cc_release_flags := -c -O3 -Wall -Wextra -std=c11
test_cc_include_dirs := ./include ./src
test_ld_debug_flags :=  -g
test_ld_release_flags :=
test_ld_lib_dirs :=
test_ld_libs :=
test_exe_debug := bin/test/debug/testbozon.exe
test_exe_release := bin/test/release/test_bozon.exe

# all binary files
# used for clean-up
test_all_obj_debug := $(wildcard bin/test/debug/int/*.o)
test_all_exe_debug := $(wildcard bin/test/debug/*.exe)

test_all_obj_release := $(wildcard bin/test/release/int/*.o)
test_all_exe_release := $(wildcard bin/test/release/*.exe)

# used for clean-up
test_rebuildables_debug := $(test_all_exe_debug) $(test_all_obj_debug)
test_rebuildables_release := $(test_all_exe_release) $(test_all_obj_release)

# deletes the binary files
test-clean: test-clean-debug test-clean-release

# deletes debug binary files
test-clean-debug:
	rm -f $(test_rebuildables_debug)

# deletes release binary files
test-clean-release:
	rm -f $(test_rebuildables_release)

test: test-debug

# debug build
test-debug: $(test_exe_debug)

# release build
test-release: $(test_exe_release)

# debug rebuild
test-rebuild-debug: test-clean-debug test-debug

# release rebuild
test-rebuild-release: test-clean-release test-release

# builds and executes the exe
test-exec: test-exec-debug

test-exec-debug: $(test_exe_debug)
	$(test_exe_debug)

test-exec-release: $(test_exe_release)
	$(test_exe_release)

$(test_exe_debug): bin/test/debug/int/etc.cpp.o bin/test/debug/int/first_pass_parser_test.cpp.o bin/test/debug/int/lexer_test.cpp.o bin/test/debug/int/parser_test.cpp.o bin/test/debug/int/test_main.cpp.o
	$(LD) $(test_ld_debug_flags) $^ -o $@ $(addprefix -L,$(test_ld_lib_dirs)) $(addprefix -l,$(test_ld_libs))

$(test_exe_release): bin/test/release/int/etc.cpp.o bin/test/release/int/first_pass_parser_test.cpp.o bin/test/release/int/lexer_test.cpp.o bin/test/release/int/parser_test.cpp.o bin/test/release/int/test_main.cpp.o
	$(LD) $(test_ld_release_flags) $^ -o $@ $(addprefix -L,$(test_ld_lib_dirs)) $(addprefix -l,$(test_ld_libs))

bin/test/debug/int/etc.cpp.o: ./test/etc.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.cpp ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.cpp ./src/ast/statement.h ./src/ast/typespec.cpp ./src/ast/typespec.h ./src/core.h ./src/ctx/built_in_operators.cpp ./src/ctx/built_in_operators.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.cpp ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.cpp ./src/ctx/global_context.h ./src/ctx/lex_context.cpp ./src/ctx/lex_context.h ./src/ctx/parse_context.cpp ./src/ctx/parse_context.h ./src/lex/file_iterator.h ./src/lex/token.cpp ./src/lex/token.h ./src/my_assert.h ./src/parser.h
	$(CXX) $(test_cxx_debug_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/release/int/etc.cpp.o: ./test/etc.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.cpp ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.cpp ./src/ast/statement.h ./src/ast/typespec.cpp ./src/ast/typespec.h ./src/core.h ./src/ctx/built_in_operators.cpp ./src/ctx/built_in_operators.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.cpp ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.cpp ./src/ctx/global_context.h ./src/ctx/lex_context.cpp ./src/ctx/lex_context.h ./src/ctx/parse_context.cpp ./src/ctx/parse_context.h ./src/lex/file_iterator.h ./src/lex/token.cpp ./src/lex/token.h ./src/my_assert.h ./src/parser.h
	$(CXX) $(test_cxx_release_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/debug/int/first_pass_parser_test.cpp.o: ./test/first_pass_parser_test.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/colors.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/first_pass_parser.cpp ./src/first_pass_parser.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./test/test.h
	$(CXX) $(test_cxx_debug_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/release/int/first_pass_parser_test.cpp.o: ./test/first_pass_parser_test.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/colors.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/first_pass_parse_context.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/first_pass_parser.cpp ./src/first_pass_parser.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./test/test.h
	$(CXX) $(test_cxx_release_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/debug/int/lexer_test.cpp.o: ./test/lexer_test.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/colors.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/lex/file_iterator.h ./src/lex/lexer.cpp ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./test/test.h
	$(CXX) $(test_cxx_debug_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/release/int/lexer_test.cpp.o: ./test/lexer_test.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/colors.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/lex/file_iterator.h ./src/lex/lexer.cpp ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./test/test.h
	$(CXX) $(test_cxx_release_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/debug/int/parser_test.cpp.o: ./test/parser_test.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/colors.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/ctx/parse_context.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./src/parser.cpp ./src/parser.h ./test/test.h
	$(CXX) $(test_cxx_debug_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/release/int/parser_test.cpp.o: ./test/parser_test.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/result.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/typespec.h ./src/colors.h ./src/core.h ./src/ctx/decl_set.h ./src/ctx/error.h ./src/ctx/global_context.h ./src/ctx/lex_context.h ./src/ctx/parse_context.h ./src/lex/file_iterator.h ./src/lex/lexer.h ./src/lex/token.h ./src/my_assert.h ./src/parser.cpp ./src/parser.h ./test/test.h
	$(CXX) $(test_cxx_release_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/debug/int/test_main.cpp.o: ./test/test_main.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/vector.h ./src/colors.h ./src/timer.h ./test/test.h
	$(CXX) $(test_cxx_debug_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/release/int/test_main.cpp.o: ./test/test_main.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/vector.h ./src/colors.h ./src/timer.h ./test/test.h
	$(CXX) $(test_cxx_release_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

