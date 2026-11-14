CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -O2 -g
EMCC ?= emcc

BIN_DIR = bin
OBJ_DIR = obj

# Common sources
COMMON_SRCS = src/common/tokenizer.c \
              src/common/parser.c \
              src/common/command.c \
              src/common/builtins.c \
              src/common/history.c

COMMON_OBJS = $(patsubst src/common/%.c, $(OBJ_DIR)/common/%.o, $(COMMON_SRCS))

# Native sources
NATIVE_SRCS = src/native/main.c \
              src/native/executor_posix.c \
              src/native/signals.c

NATIVE_OBJS = $(patsubst src/native/%.c, $(OBJ_DIR)/native/%.o, $(NATIVE_SRCS))

# Virtual/WASM sources
VIRTUAL_SRCS = src/wasm/virtual_fs.c \
               src/wasm/executor_virtual.c

VIRTUAL_OBJS = $(patsubst src/wasm/%.c, $(OBJ_DIR)/wasm/%.o, $(VIRTUAL_SRCS))

# WASM build settings
WASM_OUT_DIR = web/public/wasm
WASM_EXPORTS = '["_wasm_shell_init","_wasm_shell_reset","_wasm_shell_execute","_wasm_shell_get_cwd","_wasm_shell_get_prompt","_wasm_shell_free_string","_malloc","_free"]'
WASM_RUNTIME_METHODS = '["ccall","cwrap","UTF8ToString","stringToUTF8"]'
EMFLAGS = -std=c11 -O3 -D_POSIX_C_SOURCE=200809L \
          -s WASM=1 \
          -s EXPORTED_FUNCTIONS=$(WASM_EXPORTS) \
          -s EXPORTED_RUNTIME_METHODS=$(WASM_RUNTIME_METHODS) \
          -s ALLOW_MEMORY_GROWTH=1 \
          -s MODULARIZE=1 \
          -s EXPORT_NAME='createMiniShellModule'

.PHONY: all clean test wasm native

all: native

native: $(BIN_DIR)/minishell

$(BIN_DIR)/minishell: $(COMMON_OBJS) $(NATIVE_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/common/%.o: src/common/%.c | $(OBJ_DIR)/common
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/native/%.o: src/native/%.c | $(OBJ_DIR)/native
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/wasm/%.o: src/wasm/%.c | $(OBJ_DIR)/wasm
	$(CC) $(CFLAGS) -c $< -o $@

# Unit test binaries
$(BIN_DIR)/test_tokenizer: tests/test_tokenizer.c $(OBJ_DIR)/common/tokenizer.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/test_parser: tests/test_parser.c $(OBJ_DIR)/common/tokenizer.o $(OBJ_DIR)/common/parser.o $(OBJ_DIR)/common/command.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/test_builtins: tests/test_builtins.c $(OBJ_DIR)/common/builtins.o $(OBJ_DIR)/common/history.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/test_virtual: tests/test_virtual.c $(COMMON_OBJS) $(VIRTUAL_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

test: $(BIN_DIR)/minishell $(BIN_DIR)/test_tokenizer $(BIN_DIR)/test_parser $(BIN_DIR)/test_builtins $(BIN_DIR)/test_virtual
	@echo "--- Running Unit Tests ---"
	$(BIN_DIR)/test_tokenizer
	$(BIN_DIR)/test_parser
	$(BIN_DIR)/test_builtins
	$(BIN_DIR)/test_virtual
	@echo "--- Running Native Integration Tests ---"
	./tests/test_native.sh $(BIN_DIR)/minishell

wasm: | $(WASM_OUT_DIR)
	@if command -v $(EMCC) >/dev/null 2>&1; then \
		echo "Building WebAssembly with $(EMCC)..."; \
		$(EMCC) $(EMFLAGS) $(COMMON_SRCS) $(VIRTUAL_SRCS) src/wasm/wasm_entry.c -o $(WASM_OUT_DIR)/minishell.js; \
		echo "WASM build complete: $(WASM_OUT_DIR)/minishell.js and $(WASM_OUT_DIR)/minishell.wasm"; \
	else \
		echo "Warning: emcc not found. WebAssembly build requires Emscripten SDK."; \
		echo "In CI (GitHub Actions), emsdk is installed automatically."; \
	fi

$(BIN_DIR) $(OBJ_DIR)/common $(OBJ_DIR)/native $(OBJ_DIR)/wasm $(WASM_OUT_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
