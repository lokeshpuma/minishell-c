cmake_minimum_required(VERSION 3.12)
project(MiniShell C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L)
endif()

# Common library
set(COMMON_SOURCES
    src/common/tokenizer.c
    src/common/parser.c
    src/common/command.c
    src/common/builtins.c
    src/common/history.c
)
add_library(minishell_common STATIC ${COMMON_SOURCES})
target_include_directories(minishell_common PUBLIC src/common)

# Virtual library
set(VIRTUAL_SOURCES
    src/wasm/virtual_fs.c
    src/wasm/executor_virtual.c
)
add_library(minishell_virtual STATIC ${VIRTUAL_SOURCES})
target_include_directories(minishell_virtual PUBLIC src/common src/wasm)
target_link_libraries(minishell_virtual PUBLIC minishell_common)

if (EMSCRIPTEN)
    # WebAssembly build
    set(WASM_EXPORTS "['_wasm_shell_init','_wasm_shell_reset','_wasm_shell_execute','_wasm_shell_get_cwd','_wasm_shell_get_prompt','_wasm_shell_free_string','_malloc','_free']")
    set(WASM_RUNTIME_METHODS "['ccall','cwrap','UTF8ToString','stringToUTF8']")

    add_executable(minishell_wasm src/wasm/wasm_entry.c)
    target_link_libraries(minishell_wasm PRIVATE minishell_virtual minishell_common)
    set_target_properties(minishell_wasm PROPERTIES
        OUTPUT_NAME "minishell"
        SUFFIX ".js"
        LINK_FLAGS "-s WASM=1 -s EXPORTED_FUNCTIONS=${WASM_EXPORTS} -s EXPORTED_RUNTIME_METHODS=${WASM_RUNTIME_METHODS} -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME='createMiniShellModule'"
    )
else()
    # Native POSIX executable
    set(NATIVE_SOURCES
        src/native/main.c
        src/native/executor_posix.c
        src/native/signals.c
    )
    add_executable(minishell ${NATIVE_SOURCES})
    target_include_directories(minishell PRIVATE src/common src/native)
    target_link_libraries(minishell PRIVATE minishell_common)

    # Unit Tests
    enable_testing()

    add_executable(test_tokenizer tests/test_tokenizer.c)
    target_link_libraries(test_tokenizer PRIVATE minishell_common)
    add_test(NAME test_tokenizer COMMAND test_tokenizer)

    add_executable(test_parser tests/test_parser.c)
    target_link_libraries(test_parser PRIVATE minishell_common)
    add_test(NAME test_parser COMMAND test_parser)

    add_executable(test_builtins tests/test_builtins.c)
    target_link_libraries(test_builtins PRIVATE minishell_common)
    add_test(NAME test_builtins COMMAND test_builtins)

    add_executable(test_virtual tests/test_virtual.c)
    target_link_libraries(test_virtual PRIVATE minishell_virtual minishell_common)
    add_test(NAME test_virtual COMMAND test_virtual)

    # Integration test
    add_test(NAME test_native_integration COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_native.sh $<TARGET_FILE:minishell>)
endif()

