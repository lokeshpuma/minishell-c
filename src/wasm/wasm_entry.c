#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor_virtual.h"
#include "virtual_fs.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

static virtual_shell_t *g_wasm_shell = NULL;
static char *g_response_buffer = NULL;

static void json_escape(const char *src, vstream_t *dst) {
    if (!src) return;
    for (size_t i = 0; src[i] != '\0'; i++) {
        unsigned char c = (unsigned char)src[i];
        switch (c) {
            case '"':  vstream_append_str(dst, "\\\""); break;
            case '\\': vstream_append_str(dst, "\\\\"); break;
            case '\b': vstream_append_str(dst, "\\b"); break;
            case '\f': vstream_append_str(dst, "\\f"); break;
            case '\n': vstream_append_str(dst, "\\n"); break;
            case '\r': vstream_append_str(dst, "\\r"); break;
            case '\t': vstream_append_str(dst, "\\t"); break;
            default:
                if (c < 32) {
                    char hex[8];
                    snprintf(hex, sizeof(hex), "\\u%04x", c);
                    vstream_append_str(dst, hex);
                } else {
                    vstream_append(dst, (const char *)&src[i], 1);
                }
                break;
        }
    }
}

WASM_EXPORT
void wasm_shell_init(void) {
    if (g_wasm_shell) {
        virtual_shell_destroy(g_wasm_shell);
    }
    g_wasm_shell = virtual_shell_create();
}

WASM_EXPORT
void wasm_shell_reset(void) {
    if (!g_wasm_shell) {
        wasm_shell_init();
    } else {
        virtual_shell_reset(g_wasm_shell);
    }
}

WASM_EXPORT
const char *wasm_shell_get_cwd(void) {
    if (!g_wasm_shell) {
        wasm_shell_init();
    }
    return vfs_getcwd(g_wasm_shell->vfs);
}

WASM_EXPORT
const char *wasm_shell_get_prompt(void) {
    static char prompt[1024];
    const char *cwd = wasm_shell_get_cwd();
    snprintf(prompt, sizeof(prompt), "minishell:%s$ ", cwd);
    return prompt;
}

WASM_EXPORT
const char *wasm_shell_execute(const char *cmd_line) {
    if (!g_wasm_shell) {
        wasm_shell_init();
    }

    if (g_response_buffer) {
        free(g_response_buffer);
        g_response_buffer = NULL;
    }

    vstream_t out_stream;
    vstream_t err_stream;
    vstream_init(&out_stream);
    vstream_init(&err_stream);

    int exit_code = virtual_shell_execute(g_wasm_shell, cmd_line, &out_stream, &err_stream);

    /* Construct JSON response */
    vstream_t json;
    vstream_init(&json);
    vstream_printf(&json, "{\"exit_code\":%d,\"stdout\":\"", exit_code);
    json_escape(out_stream.data, &json);
    vstream_append_str(&json, "\",\"stderr\":\"");
    json_escape(err_stream.data, &json);
    vstream_append_str(&json, "\",\"cwd\":\"");
    json_escape(vfs_getcwd(g_wasm_shell->vfs), &json);
    vstream_append_str(&json, "\"}");

    vstream_free(&out_stream);
    vstream_free(&err_stream);

    g_response_buffer = json.data;
    return g_response_buffer;
}

WASM_EXPORT
void wasm_shell_free_string(char *ptr) {
    if (ptr) {
        free(ptr);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    wasm_shell_init();
    return 0;
}

