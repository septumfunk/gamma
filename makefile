$(info == gamma ==)

# Compiler
HOST := $(shell uname -s 2>/dev/null)
$(info HOST: $(HOST))
$(info TARGET: DOS (x86))

ifeq ($(HOST),Darwin)
CROSS = i386-pc-msdosdjgpp-

CC = $(CROSS)gcc
AR = $(CROSS)ar
STRIP = $(CROSS)strip
MKDIR = mkdir -p
CLEAN = rm -rf
else
CC = gcc
AR = ar
STRIP = strip
MKDIR = mkdir
CLEAN = deltree /y
endif

$(info CC: $(CC))
$(info AR: $(AR))

# Config
BUILD = bin
CONFIG ?= debug
OUT = $(BUILD)/$(CONFIG)

OBJ_ROOT = obj
OBJ = $(OBJ_ROOT)/$(CONFIG)

$(info CONFIG: $(CONFIG))
$(info OUT: $(OUT))
$(info ====)

# Flags
CFLAGS = -std=gnu11 -Werror -Wall -Wextra -Wpedantic -Iinclude -fgnu89-inline

ifeq ($(CONFIG),release)
CFLAGS += -O2 -DNDEBUG
else
CFLAGS += -O0 -g
endif

# Sources
GAMMA_SOURCES = \
    src/render.c \
	src/scene.c

TEST_SOURCES = \
    tests/main.c

# Objects
GAMMA_OBJECTS = $(patsubst src/%.c,$(OBJ)/%.o,$(GAMMA_SOURCES))
TEST_OBJECTS = $(patsubst tests/%.c,$(OBJ)/tests/%.o,$(TEST_SOURCES))

# Build
.PHONY: all clean fresh

all: $(OUT)/gamma.exe

# Directories
$(BUILD):
	@echo Creating build directories...
	$(MKDIR) $(BUILD)

$(OUT): | $(BUILD)
	$(MKDIR) $(OUT)

$(OBJ_ROOT):
	$(MKDIR) $(OBJ_ROOT)

$(OBJ): | $(OBJ_ROOT)
	$(MKDIR) $(OBJ)

$(OBJ)/tests: | $(OBJ)
	$(MKDIR) $(OBJ)/tests

# Libraries
DEPS_LIB = -lm lib/allegro/liballeg.a

CFLAGS += -isystem lib/allegro/include

GAMMA_LIB = $(OUT)/libgamma.a

# Compile
$(OBJ)/%.o: src/%.c | $(OBJ)
	@echo Compiling library sources...
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/tests/%.o: tests/%.c | $(OBJ)/tests
	@echo Compiling test sources...
	$(CC) $(CFLAGS) -c $< -o $@

# Archive
$(GAMMA_LIB): $(GAMMA_OBJECTS) | $(OUT)
	@echo Archiving static library...
	$(AR) rcs $@ $(GAMMA_OBJECTS)

# Link
$(OUT)/gamma.exe: $(TEST_OBJECTS) $(GAMMA_LIB)
	@echo Linking exe...
	$(CC) $(TEST_OBJECTS) $(GAMMA_LIB) $(DEPS_LIB) -o $@
ifeq ($(CONFIG),release)
	$(STRIP) --strip-debug $@
endif
	@echo ====

# Clean
clean:
	@echo Clean build...
	$(CLEAN) $(BUILD) $(OBJ_ROOT)

# Fresh build (sequential, safe with -j)
fresh:
	$(MAKE) clean
	$(MAKE) all CONFIG=$(CONFIG)