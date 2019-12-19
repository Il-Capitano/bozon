# recursive wildcard
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

# compiler parameters
CXX := g++
CXX_FLAGS_DEBUG := -c -g -Wall -Wextra -std=c++17
CXX_FLAGS_RELEASE := -c -O2 -Wall -Wextra -std=c++17
CXX_INCLUDES := C:/dev/library/general/include

# linker parameters
LD := g++
LD_FLAGS_DEBUG := -g
LD_FLAGS_RELEASE :=
LD_LIBS :=

# directories
BIN_DEBUG := bin/debug
BIN_RELEASE := bin/release
BIN_INT_DEBUG := bin/debug/int
BIN_INT_RELEASE := bin/release/int
SRC := src

# debug executable path
EXE_DEBUG := $(BIN_DEBUG)/compiler.exe

# release executable path
EXE_RELEASE := $(BIN_RELEASE)/compiler.exe

# source files
SRCS := $(call rwildcard,$(SRC)/,*.cpp)

# object files generated from source files
OBJS := $(patsubst %.cpp,%.o,$(SRCS))
# remove all directories from cpp files so
# all obj files are in the same directory
OBJS_DEBUG := $(addprefix $(BIN_INT_DEBUG)/,$(notdir $(OBJS)))
OBJS_RELEASE := $(addprefix $(BIN_INT_RELEASE)/,$(notdir $(OBJS)))

# all objs that are in BIN_INT
# used for clean-up
ALL_OBJ := $(wildcard $(BIN_INT_DEBUG)/*.o) $(wildcard $(BIN_INT_RELEASE)/*.o)

ALL_EXE := $(wildcard $(BIN_DEBUG)/*.exe) $(wildcard $(BIN_RELEASE)/*.exe)

# object files and executable are rebuildable
# used for clean-up
REBUILDABLES := $(ALL_EXE) $(ALL_OBJ)

# builds the executable
all: debug

release: $(EXE_RELEASE)

debug: $(EXE_DEBUG)

$(EXE_DEBUG): $(OBJS_DEBUG)
	$(LD) $(LD_FLAGS_DEBUG) $(addprefix -L,$(LD_LIBS)) $^ -o $@

$(EXE_RELEASE): $(OBJS_RELEASE)
	$(LD) $(LD_FLAGS_RELEASE) $(addprefix -L,$(LD_LIBS)) $^ -o $@

# cleans the binary files
clean:
	rm -f $(subst /,\,$(REBUILDABLES))

# directive to build obj files
# $1 = <src file dir>
# $2 = <src file name>
# $3 = <additional dependencies>
define build-objs-debug
$$(BIN_INT_DEBUG)/$(2).o: $(1)$(2).cpp $(3)
	$$(CXX) $$(CXX_FLAGS_DEBUG) $$(addprefix -I,$(CXX_INCLUDES)) $$< -o $$@
endef
define build-objs-release
$(BIN_INT_RELEASE)/$(2).o: $(1)$(2).cpp $(3)
	$$(CXX) $$(CXX_FLAGS_RELEASE) $$(addprefix -I,$(CXX_INCLUDES)) $$< -o $$@
endef

# generates the obj file build directives using the build-objs macro
# check_cpp_deps is a program that extracts the header files from a cpp file
# this is used to add the additional dependencies for header files automatically
$(foreach src-file,$(SRCS),$(eval $(call build-objs-debug,$(dir $(src-file)),$(basename $(notdir $(src-file))),$(shell check_cpp_deps $(src-file) $(CXX_INCLUDES)))))
$(foreach src-file,$(SRCS),$(eval $(call build-objs-release,$(dir $(src-file)),$(basename $(notdir $(src-file))),$(shell check_cpp_deps $(src-file) $(CXX_INCLUDES)))))
