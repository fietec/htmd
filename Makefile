# Compiler settings
CC     := gcc
EMCC   := /home/constantijn/bin/emsdk/upstream/emscripten/emcc
CFLAGS := -Wall -Wextra -Iinclude
CLI_DEFS := -DHTMD_CLI

# WASM compile & link flags
WASM_CFLAGS := -Iinclude
WASM_LDFLAGS := \
	-s MODULARIZE=1 -s EXPORT_NAME="Module" \
	-s EXPORTED_FUNCTIONS="['_render_markdown', '_malloc', '_free']" \
	-s EXPORTED_RUNTIME_METHODS="['cwrap','lengthBytesUTF8','stringToUTF8','UTF8ToString']"

# Source files
SRC_DIR := src
OBJ_DIR := build
SITE_DIR := site
CLI_SRCS := $(SRC_DIR)/cli.c $(SRC_DIR)/render.c
WASM_SRCS := $(SRC_DIR)/render.c

CLI_OBJS := $(CLI_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
WASM_OBJS := $(WASM_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.wasm.o)

CLI_BIN := htmd
WASM_JS := $(SITE_DIR)/markdown.js

.PHONY: all
all: cli wasm

cli: $(CLI_BIN)

$(CLI_BIN): $(CLI_OBJS)
	$(CC) $(CFLAGS) $(CLI_DEFS) -o $@ $^

wasm: $(WASM_JS)

$(WASM_JS): $(WASM_OBJS)
	$(EMCC) $(WASM_OBJS) -o $@ $(WASM_LDFLAGS)

# Compile rules
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CLI_DEFS) -c $< -o $@

$(OBJ_DIR)/%.wasm.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(EMCC) $(WASM_CFLAGS) -c $< -o $@

# Clean
.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(CLI_BIN) $(WASM_JS)
