# compiler parameters
CXX := g++
CC := gcc

# linker parameters
LD := g++
LD_FLAGS_DEBUG := -g
LD_FLAGS_RELEASE :=
LD_LIBDIRS :=
LD_LIBS :=

# debug build by default
default: debug

# =========================
# ======== default ========
# =========================

default_cxx_debug_flags := -c -g -Og -Wall -Wextra -std=c++17
default_cxx_release_flags := -c -O3 -Wall -Wextra -std=c++17
default_cxx_include_dirs := ./include 
default_cc_debug_flags := -c -g -Og -Wall -Wextra -std=c++17
default_cc_release_flags := -c -O3 -Wall -Wextra -std=c++17
default_cc_include_dirs := ./include 
default_exe_debug := bin/default/debug/bozon.exe
default_exe_release := bin/default/release/bozon

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

$(default_exe_debug): bin/default/debug/int/bytecode.cpp.o bin/default/debug/int/first_pass_parser.cpp.o bin/default/debug/int/function_body.cpp.o bin/default/debug/int/lexer.cpp.o bin/default/debug/int/main.cpp.o bin/default/debug/int/parser.cpp.o bin/default/debug/int/parse_context.cpp.o bin/default/debug/int/src_file.cpp.o bin/default/debug/int/expression.cpp.o bin/default/debug/int/statement.cpp.o bin/default/debug/int/type.cpp.o
	$(LD) $(LD_FLAGS_DEBUG) $(addprefix -L,$(LD_LIBDIRS)) $(addprefix -l,$(LD_LIBS)) $^ -o $@

$(default_exe_release): bin/default/release/int/bytecode.cpp.o bin/default/release/int/first_pass_parser.cpp.o bin/default/release/int/function_body.cpp.o bin/default/release/int/lexer.cpp.o bin/default/release/int/main.cpp.o bin/default/release/int/parser.cpp.o bin/default/release/int/parse_context.cpp.o bin/default/release/int/src_file.cpp.o bin/default/release/int/expression.cpp.o bin/default/release/int/statement.cpp.o bin/default/release/int/type.cpp.o
	$(LD) $(LD_FLAGS_RELEASE) $(addprefix -L,$(LD_LIBDIRS)) $(addprefix -l,$(LD_LIBS)) $^ -o $@

bin/default/debug/int/bytecode.cpp.o: ./src/bytecode.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/bytecode.cpp.o: ./src/bytecode.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/first_pass_parser.cpp.o: ./src/first_pass_parser.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/first_pass_parser.cpp.o: ./src/first_pass_parser.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/function_body.cpp.o: ./src/function_body.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/function_body.cpp.o: ./src/function_body.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/lexer.cpp.o: ./src/lexer.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/core.h ./src/error.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/lexer.cpp.o: ./src/lexer.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/core.h ./src/error.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/main.cpp.o: ./src/main.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h ./src/src_file.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/main.cpp.o: ./src/main.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h ./src/src_file.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/parser.cpp.o: ./src/parser.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/parser.cpp.o: ./src/parser.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/parse_context.cpp.o: ./src/parse_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/parse_context.cpp.o: ./src/parse_context.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/src_file.cpp.o: ./src/src_file.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h ./src/src_file.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/src_file.cpp.o: ./src/src_file.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h ./src/src_file.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/expression.cpp.o: ./src/ast/expression.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/expression.cpp.o: ./src/ast/expression.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/statement.cpp.o: ./src/ast/statement.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/my_assert.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/statement.cpp.o: ./src/ast/statement.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/my_assert.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/debug/int/type.cpp.o: ./src/ast/type.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h
	$(CXX) $(default_cxx_debug_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

bin/default/release/int/type.cpp.o: ./src/ast/type.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parse_context.h ./src/parser.h
	$(CXX) $(default_cxx_release_flags) $(addprefix -I,$(default_cxx_include_dirs)) $< -o $@

# ======================
# ======== test ========
# ======================

test_cxx_debug_flags := -c -g -Og -Wall -Wextra -std=c++17
test_cxx_release_flags := -c -O3 -Wall -Wextra -std=c++17
test_cxx_include_dirs := ./src ./include 
test_cc_debug_flags := -c -g -Og -Wall -Wextra -std=c++17
test_cc_release_flags := -c -O3 -Wall -Wextra -std=c++17
test_cc_include_dirs := ./src ./include 
test_exe_debug := bin/test/debug/testbozon.exe
test_exe_release := bin/test/release/testbozon

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

$(test_exe_debug): bin/test/debug/int/lexer_test.cpp.o bin/test/debug/int/test_main.cpp.o
	$(LD) $(LD_FLAGS_DEBUG) $(addprefix -L,$(LD_LIBDIRS)) $(addprefix -l,$(LD_LIBS)) $^ -o $@

$(test_exe_release): bin/test/release/int/lexer_test.cpp.o bin/test/release/int/test_main.cpp.o
	$(LD) $(LD_FLAGS_RELEASE) $(addprefix -L,$(LD_LIBDIRS)) $(addprefix -l,$(LD_LIBS)) $^ -o $@

bin/test/debug/int/lexer_test.cpp.o: ./test/lexer_test.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/core.h ./src/error.h ./src/lexer.cpp ./src/my_assert.h ./test/test.h
	$(CXX) $(test_cxx_debug_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/release/int/lexer_test.cpp.o: ./test/lexer_test.cpp ./include/bz/allocator.h ./include/bz/core.h ./include/bz/format.h ./include/bz/iterator.h ./include/bz/meta.h ./include/bz/optional.h ./include/bz/string.h ./include/bz/string_view.h ./include/bz/variant.h ./include/bz/vector.h ./src/ast/../lexer.h ./src/core.h ./src/error.h ./src/lexer.cpp ./src/my_assert.h ./test/test.h
	$(CXX) $(test_cxx_release_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/debug/int/test_main.cpp.o: ./test/test_main.cpp
	$(CXX) $(test_cxx_debug_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

bin/test/release/int/test_main.cpp.o: ./test/test_main.cpp
	$(CXX) $(test_cxx_release_flags) $(addprefix -I,$(test_cxx_include_dirs)) $< -o $@

