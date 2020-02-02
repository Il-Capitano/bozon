# compiler parameters
CXX := g++
CXX_FLAGS_DEBUG := -c -g -Og -Wall -Wextra -std=c++17
CXX_FLAGS_RELEASE := -c -O3 -Wall -Wextra -std=c++17
CXX_INCLUDES := C:/dev/library/general/include 

CC := gcc
CC_FLAGS_DEBUG := -c -g -Og -Wall -Wextra -std=c11
CC_FLAGS_RELEASE := -c -O3 -Wall -Wextra -std=c11
CC_INCLUDES := C:/dev/library/general/include 

# linker parameters
LD := g++
LD_FLAGS_DEBUG := -g
LD_FLAGS_RELEASE :=
LD_LIBDIRS :=
LD_LIBS :=

# debug executable path
EXE_DEBUG := bin/debug/bozon.exe

# release executable path
EXE_RELEASE := bin/release/bozon.exe

# all binary files
# used for clean-up
ALL_OBJ_DEBUG := $(wildcard bin/debug/int/*.o)
ALL_EXE_DEBUG := $(wildcard bin/debug/*.exe)

ALL_OBJ_RELEASE := $(wildcard bin/release/int/*.o)
ALL_EXE_RELEASE := $(wildcard bin/release/*.exe)

# object files and executable are rebuildable
# used for clean-up
REBUILDABLES_DEBUG := $(ALL_EXE_DEBUG) $(ALL_OBJ_DEBUG)
REBUILDABLES_RELEASE := $(ALL_EXE_RELEASE) $(ALL_OBJ_RELEASE)

# builds the executable
all: debug

# debug rebuild
rebuild-debug: clean-debug debug

# release rebuild
rebuild-release: clean-release release

# debug build
debug: $(EXE_DEBUG)

# release build
release: $(EXE_RELEASE)

# deletes the binary files
clean: clean-debug clean-release

# deletes debug binary files
clean-debug:
	rm -f $(REBUILDABLES_DEBUG)

# deletes release binary files
clean-release:
	rm -f $(REBUILDABLES_DEBUG)

# builds and executes the exe
exec: exec-debug

exec-debug: $(EXE_DEBUG)
	$(EXE_DEBUG)

exec-release: $(EXE_RELEASE)
	$(EXE_RELEASE)

$(EXE_DEBUG): bin/debug/int/bytecode.cpp.o bin/debug/int/context.cpp.o bin/debug/int/first_pass_parser.cpp.o bin/debug/int/function_body.cpp.o bin/debug/int/lexer.cpp.o bin/debug/int/main.cpp.o bin/debug/int/parser.cpp.o bin/debug/int/expression.cpp.o bin/debug/int/statement.cpp.o bin/debug/int/type.cpp.o
	$(LD) $(LD_FLAGS_DEBUG) $(addprefix -L,$(LD_LIBDIRS)) $(addprefix -l,$(LD_LIBS)) $^ -o $@

$(EXE_RELEASE): bin/release/int/bytecode.cpp.o bin/release/int/context.cpp.o bin/release/int/first_pass_parser.cpp.o bin/release/int/function_body.cpp.o bin/release/int/lexer.cpp.o bin/release/int/main.cpp.o bin/release/int/parser.cpp.o bin/release/int/expression.cpp.o bin/release/int/statement.cpp.o bin/release/int/type.cpp.o
	$(LD) $(LD_FLAGS_RELEASE) $(addprefix -L,$(LD_LIBDIRS)) $(addprefix -l,$(LD_LIBS)) $^ -o $@

bin/debug/int/bytecode.cpp.o: ./src/bytecode.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/bytecode.cpp.o: ./src/bytecode.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/context.cpp.o: ./src/context.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/context.cpp.o: ./src/context.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/first_pass_parser.cpp.o: ./src/first_pass_parser.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/my_assert.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/first_pass_parser.cpp.o: ./src/first_pass_parser.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/my_assert.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/function_body.cpp.o: ./src/function_body.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/function_body.cpp.o: ./src/function_body.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/lexer.cpp.o: ./src/lexer.cpp ./src/ast/../lexer.h ./src/core.h ./src/error.h ./src/my_assert.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/lexer.cpp.o: ./src/lexer.cpp ./src/ast/../lexer.h ./src/core.h ./src/error.h ./src/my_assert.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/main.cpp.o: ./src/main.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/main.cpp.o: ./src/main.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/parser.cpp.o: ./src/parser.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/parser.cpp.o: ./src/parser.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/expression.cpp.o: ./src/ast/expression.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/expression.cpp.o: ./src/ast/expression.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/statement.cpp.o: ./src/ast/statement.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/statement.cpp.o: ./src/ast/statement.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/type.cpp.o: ./src/ast/type.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/type.cpp.o: ./src/ast/type.cpp ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/node.h ./src/ast/statement.h ./src/ast/type.h ./src/bytecode.h ./src/context.h ./src/core.h ./src/error.h ./src/first_pass_parser.h ./src/function_body.h ./src/my_assert.h ./src/parser.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/format.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/optional.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/vector.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

