# compiler parameters
CXX := g++
CXX_FLAGS_DEBUG := -c -Og -g -Wall -Wextra -std=c++17
CXX_FLAGS_RELEASE := -c -O3 -Wall -Wextra -std=c++17
CXX_INCLUDES :=C:/dev/library/general/include 

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

# all objs that are in BIN_INT
# used for clean-up
ALL_OBJ := $(wildcard bin/debug/int/*.o) $(wildcard bin/release/int/*.o)
ALL_EXE := $(wildcard bin/debug/*.exe) $(wildcard bin/release/*.exe)

# object files and executable are rebuildable
# used for clean-up
REBUILDABLES := $(ALL_EXE) $(ALL_OBJ)

# builds the executable
all: debug

# debug rebuild
rebuild-debug: clean debug

# release rebuild
rebuild-release: clean release

# release build
release: $(EXE_RELEASE)

# debug build
debug: $(EXE_DEBUG)

# cleans the binary files
clean:
	rm -f $(subst /,\,$(REBUILDABLES))

$(EXE_DEBUG): bin/debug/int/bytecode.cpp.o bin/debug/int/context.cpp.o bin/debug/int/first_pass_parser.cpp.o bin/debug/int/lexer.cpp.o bin/debug/int/main.cpp.o bin/debug/int/parser.cpp.o bin/debug/int/expression.cpp.o bin/debug/int/statement.cpp.o bin/debug/int/type.cpp.o
	$(LD) $(LD_FLAGS_DEBUG) $(addprefix -L,$(LD_LIBDIRS)) $(addprefix -l,$(LD_LIBS)) $^ -o $@

$(EXE_RELEASE): bin/release/int/bytecode.cpp.o bin/release/int/context.cpp.o bin/release/int/first_pass_parser.cpp.o bin/release/int/lexer.cpp.o bin/release/int/main.cpp.o bin/release/int/parser.cpp.o bin/release/int/expression.cpp.o bin/release/int/statement.cpp.o bin/release/int/type.cpp.o
	$(LD) $(LD_FLAGS_RELEASE) $(addprefix -L,$(LD_LIBDIRS)) $(addprefix -l,$(LD_LIBS)) $^ -o $@

bin/debug/int/bytecode.cpp.o: ./src/bytecode.cpp ./src/bytecode.cpp ./src/bytecode.h ./src/core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/error.h C:/dev/library/general/include/bz/format.h ./src/my_assert.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/bytecode.cpp.o: ./src/bytecode.cpp ./src/bytecode.cpp ./src/bytecode.h ./src/core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/error.h C:/dev/library/general/include/bz/format.h ./src/my_assert.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/context.cpp.o: ./src/context.cpp ./src/context.cpp ./src/context.h ./src/core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/error.h C:/dev/library/general/include/bz/format.h ./src/my_assert.h ./src/ast/type.h ./src/ast/../lexer.h ./src/ast/node.h ./src/ast/expression.h ./src/ast/statement.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/context.cpp.o: ./src/context.cpp ./src/context.cpp ./src/context.h ./src/core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/error.h C:/dev/library/general/include/bz/format.h ./src/my_assert.h ./src/ast/type.h ./src/ast/../lexer.h ./src/ast/node.h ./src/ast/expression.h ./src/ast/statement.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/first_pass_parser.cpp.o: ./src/first_pass_parser.cpp ./src/first_pass_parser.cpp ./src/first_pass_parser.h ./src/core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/error.h C:/dev/library/general/include/bz/format.h ./src/my_assert.h ./src/lexer.h ./src/ast/statement.h ./src/ast/node.h ./src/ast/expression.h ./src/ast/type.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/first_pass_parser.cpp.o: ./src/first_pass_parser.cpp ./src/first_pass_parser.cpp ./src/first_pass_parser.h ./src/core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/error.h C:/dev/library/general/include/bz/format.h ./src/my_assert.h ./src/lexer.h ./src/ast/statement.h ./src/ast/node.h ./src/ast/expression.h ./src/ast/type.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/lexer.cpp.o: ./src/lexer.cpp ./src/lexer.cpp ./src/lexer.h ./src/core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/error.h C:/dev/library/general/include/bz/format.h ./src/my_assert.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/lexer.cpp.o: ./src/lexer.cpp ./src/lexer.cpp ./src/lexer.h ./src/core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/error.h C:/dev/library/general/include/bz/format.h ./src/my_assert.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/main.cpp.o: ./src/main.cpp ./src/main.cpp ./src/core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/error.h C:/dev/library/general/include/bz/format.h ./src/my_assert.h ./src/lexer.h ./src/first_pass_parser.h ./src/ast/statement.h ./src/ast/node.h ./src/ast/expression.h ./src/ast/type.h ./src/parser.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/main.cpp.o: ./src/main.cpp ./src/main.cpp ./src/core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/error.h C:/dev/library/general/include/bz/format.h ./src/my_assert.h ./src/lexer.h ./src/first_pass_parser.h ./src/ast/statement.h ./src/ast/node.h ./src/ast/expression.h ./src/ast/type.h ./src/parser.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/parser.cpp.o: ./src/parser.cpp ./src/parser.cpp ./src/parser.h ./src/ast/expression.h ./src/ast/../core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/ast/../error.h C:/dev/library/general/include/bz/format.h ./src/ast/../my_assert.h ./src/ast/node.h ./src/ast/../lexer.h ./src/ast/type.h ./src/ast/statement.h ./src/first_pass_parser.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/parser.cpp.o: ./src/parser.cpp ./src/parser.cpp ./src/parser.h ./src/ast/expression.h ./src/ast/../core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/ast/../error.h C:/dev/library/general/include/bz/format.h ./src/ast/../my_assert.h ./src/ast/node.h ./src/ast/../lexer.h ./src/ast/type.h ./src/ast/statement.h ./src/first_pass_parser.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/expression.cpp.o: ./src/ast/expression.cpp ./src/ast/expression.cpp ./src/ast/expression.h ./src/ast/../core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/ast/../error.h C:/dev/library/general/include/bz/format.h ./src/ast/../my_assert.h ./src/ast/node.h ./src/ast/../lexer.h ./src/ast/type.h ./src/ast/../context.h ./src/ast/../ast/statement.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/expression.cpp.o: ./src/ast/expression.cpp ./src/ast/expression.cpp ./src/ast/expression.h ./src/ast/../core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/ast/../error.h C:/dev/library/general/include/bz/format.h ./src/ast/../my_assert.h ./src/ast/node.h ./src/ast/../lexer.h ./src/ast/type.h ./src/ast/../context.h ./src/ast/../ast/statement.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/statement.cpp.o: ./src/ast/statement.cpp ./src/ast/statement.cpp ./src/ast/statement.h ./src/ast/../core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/ast/../error.h C:/dev/library/general/include/bz/format.h ./src/ast/../my_assert.h ./src/ast/node.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/type.h ./src/ast/../context.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/statement.cpp.o: ./src/ast/statement.cpp ./src/ast/statement.cpp ./src/ast/statement.h ./src/ast/../core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/ast/../error.h C:/dev/library/general/include/bz/format.h ./src/ast/../my_assert.h ./src/ast/node.h ./src/ast/../lexer.h ./src/ast/expression.h ./src/ast/type.h ./src/ast/../context.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/debug/int/type.cpp.o: ./src/ast/type.cpp ./src/ast/type.cpp ./src/ast/type.h ./src/ast/../core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/ast/../error.h C:/dev/library/general/include/bz/format.h ./src/ast/../my_assert.h ./src/ast/../lexer.h ./src/ast/node.h ./src/ast/../context.h ./src/ast/../ast/expression.h ./src/ast/../ast/statement.h
	$(CXX) $(CXX_FLAGS_DEBUG) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

bin/release/int/type.cpp.o: ./src/ast/type.cpp ./src/ast/type.cpp ./src/ast/type.h ./src/ast/../core.h C:/dev/library/general/include/bz/variant.h C:/dev/library/general/include/bz/core.h C:/dev/library/general/include/bz/meta.h C:/dev/library/general/include/bz/string.h C:/dev/library/general/include/bz/iterator.h C:/dev/library/general/include/bz/allocator.h C:/dev/library/general/include/bz/string_view.h C:/dev/library/general/include/bz/vector.h C:/dev/library/general/include/bz/optional.h ./src/ast/../error.h C:/dev/library/general/include/bz/format.h ./src/ast/../my_assert.h ./src/ast/../lexer.h ./src/ast/node.h ./src/ast/../context.h ./src/ast/../ast/expression.h ./src/ast/../ast/statement.h
	$(CXX) $(CXX_FLAGS_RELEASE) $(addprefix -I,$(CXX_INCLUDES)) $< -o $@

