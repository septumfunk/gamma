# Compiler
HOST := $(shell uname -s 2>/dev/null)

ifeq ($(HOST),Darwin)
CROSS = i386-pc-msdosdjgpp-

CC = $(CROSS)gcc
AR = $(CROSS)ar
MKDIR = mkdir -p
CLEAN = rm -rf
else
CC = gcc
AR = ar
MKDIR = mkdir
CLEAN = deltree /y
endif

# Config
BUILD = bin
CONFIG = debug
OUT = $(BUILD)/$(CONFIG)
OBJ = $(OUT)/obj

# Flags
CPPFLAGS = -Iinclude
CFLAGS = -std=c11 -Werror -Wall -Wextra -Wpedantic

ifeq ($(CONFIG),release)
CFLAGS += -O2 -DNDEBUG
else
CFLAGS += -O0 -g
endif

# Sources
GAMMA_SOURCES = \
    src/render.c
TEST_SOURCES = \
    tests/main.c

# Objects
GAMMA_OBJECTS = $(patsubst src/%.c,$(OBJ)/%.o,$(GAMMA_SOURCES))
TEST_OBJECTS = $(patsubst tests/%.c,$(OBJ)/tests/%.o,$(TEST_SOURCES))

# Build
.PHONY: all clean

all: $(OUT)/gamma_test.exe

# Directories
$(BUILD):
	$(MKDIR) $(BUILD)
$(OUT): $(BUILD)
	$(MKDIR) $(OUT)
$(OBJ): $(OUT)
	$(MKDIR) $(OBJ)
$(OBJ)/tests: $(OBJ)
	$(MKDIR) $(OBJ)/tests

# Compile
$(OBJ)/%.o: src/%.c | $(OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ)/tests/%.o: tests/%.c | $(OBJ)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OUT)/libgamma.a: $(GAMMA_OBJECTS) | $(OUT)
	$(AR) rcs $@ $^

$(OUT)/gamma_test.exe: $(TEST_OBJECTS) $(OUT)/libgamma.a
	$(CC) $^ -o $@

# Clean
clean:
	$(CLEAN) $(BUILD)