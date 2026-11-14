#define _POSIX_C_SOURCE 200809L
#include "executor_virtual.h"
#include "../common/tokenizer.h"
#include "../common/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

void vstream_init(vstream_t *s) {
    if (!s) return;
    s->capacity = 128;
    s->len = 0;
    s->data = (char *)malloc(s->capacity);
    if (s->data) s->data[0] = '\0';
}

void vstream_free(vstream_t *s) {
    if (!s) return;
    if (s->data) {
        free(s->data);
        s->data = NULL;
    }
    s->len = 0;
    s->capacity = 0;
}

void vstream_append(vstream_t *s, const char *str, size_t len) {
    if (!s || !str || len == 0) return;
    if (s->len + len + 1 > s->capacity) {
        while (s->len + len + 1 > s->capacity) {
            s->capacity = s->capacity == 0 ? 128 : s->capacity * 2;
        }
        char *new_data = (char *)realloc(s->data, s->capacity);
        if (!new_data) return;
        s->data = new_data;
    }
    memcpy(s->data + s->len, str, len);
    s->len += len;
    s->data[s->len] = '\0';
}

void vstream_append_str(vstream_t *s, const char *str) {
    if (str) {
        vstream_append(s, str, strlen(str));
    }
}

void vstream_printf(vstream_t *s, const char *fmt, ...) {
    if (!s || !fmt) return;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        vstream_append(s, buf, (size_t)len);
    }
}

virtual_shell_t *virtual_shell_create(void) {
    virtual_shell_t *sh = (virtual_shell_t *)malloc(sizeof(virtual_shell_t));
    if (!sh) return NULL;
    sh->vfs = vfs_create();
    vfs_init_sample_data(sh->vfs);
    sh->history = history_create();
    sh->env_capacity = 16;
    sh->env_count = 0;
    sh->env = (venv_entry_t *)malloc(sizeof(venv_entry_t) * sh->env_capacity);
    sh->last_exit_status = 0;

    virtual_shell_setenv(sh, "USER", "demo_user");
    virtual_shell_setenv(sh, "HOME", "/home/user");
    virtual_shell_setenv(sh, "SHELL", "/bin/minishell");
    virtual_shell_setenv(sh, "TERM", "xterm-256color");
    virtual_shell_setenv(sh, "PATH", "/bin:/usr/bin");

    return sh;
}

void virtual_shell_destroy(virtual_shell_t *sh) {
    if (!sh) return;
    if (sh->vfs) vfs_destroy(sh->vfs);
    if (sh->history) history_free(sh->history);
    if (sh->env) {
        for (size_t i = 0; i < sh->env_count; i++) {
            if (sh->env[i].key) free(sh->env[i].key);
            if (sh->env[i].value) free(sh->env[i].value);
        }
        free(sh->env);
    }
    free(sh);
}

void virtual_shell_reset(virtual_shell_t *sh) {
    if (!sh) return;
    vfs_reset(sh->vfs);
    sh->last_exit_status = 0;
}

const char *virtual_shell_getenv(virtual_shell_t *sh, const char *name) {
    if (!sh || !name) return NULL;
    for (size_t i = 0; i < sh->env_count; i++) {
        if (strcmp(sh->env[i].key, name) == 0) {
            return sh->env[i].value;
        }
    }
    return NULL;
}

int virtual_shell_setenv(virtual_shell_t *sh, const char *name, const char *value) {
    if (!sh || !name) return -1;
    for (size_t i = 0; i < sh->env_count; i++) {
        if (strcmp(sh->env[i].key, name) == 0) {
            free(sh->env[i].value);
            sh->env[i].value = strdup(value ? value : "");
            return 0;
        }
    }
    if (sh->env_count >= sh->env_capacity) {
        size_t new_cap = sh->env_capacity * 2;
        venv_entry_t *new_env = (venv_entry_t *)realloc(sh->env, sizeof(venv_entry_t) * new_cap);
        if (!new_env) return -1;
        sh->env = new_env;
        sh->env_capacity = new_cap;
    }
    sh->env[sh->env_count].key = strdup(name);
    sh->env[sh->env_count].value = strdup(value ? value : "");
    sh->env_count++;
    return 0;
}

int virtual_shell_unsetenv(virtual_shell_t *sh, const char *name) {
    if (!sh || !name) return -1;
    for (size_t i = 0; i < sh->env_count; i++) {
        if (strcmp(sh->env[i].key, name) == 0) {
            free(sh->env[i].key);
            free(sh->env[i].value);
            for (size_t j = i; j + 1 < sh->env_count; j++) {
                sh->env[j] = sh->env[j + 1];
            }
            sh->env_count--;
            return 0;
        }
    }
    return 0;
}

/* Virtual Command Implementations */

static int cmd_help(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)argc; (void)argv; (void)in; (void)err; (void)sh;
    vstream_printf(out, "MiniShell (WebAssembly Demo - Static Portfolio Edition)\n");
    vstream_printf(out, "Note: This runs client-side in WebAssembly with a virtual in-memory filesystem.\n\n");
    vstream_printf(out, "Available browser commands:\n");
    vstream_printf(out, "  help                 Display this command reference\n");
    vstream_printf(out, "  pwd                  Print current virtual directory\n");
    vstream_printf(out, "  cd [path]            Change virtual directory\n");
    vstream_printf(out, "  ls [-l] [-a] [path]  List virtual directory entries\n");
    vstream_printf(out, "  echo [args...]       Print text to standard output\n");
    vstream_printf(out, "  cat [files...]       Concatenate and display files or stdin\n");
    vstream_printf(out, "  head [-n N] [file]   Display first N lines (default 10)\n");
    vstream_printf(out, "  tail [-n N] [file]   Display last N lines (default 10)\n");
    vstream_printf(out, "  wc [-l] [-w] [file]  Count lines, words, or bytes\n");
    vstream_printf(out, "  grep [-v] [-i] PAT   Search matching lines\n");
    vstream_printf(out, "  sort [-r] [-n] [file]Sort lines of text\n");
    vstream_printf(out, "  uniq [-c] [file]     Report or omit repeated lines\n");
    vstream_printf(out, "  mkdir [path]         Create virtual directory\n");
    vstream_printf(out, "  touch [file]         Create empty virtual file\n");
    vstream_printf(out, "  rm [-r] [path]       Remove virtual file or directory\n");
    vstream_printf(out, "  cp [src] [dst]       Copy virtual file\n");
    vstream_printf(out, "  mv [src] [dst]       Move/rename virtual file\n");
    vstream_printf(out, "  clear                Clear terminal output\n");
    vstream_printf(out, "  history              Display session command history\n");
    vstream_printf(out, "  env                  List virtual environment variables\n");
    vstream_printf(out, "  export NAME=value    Set virtual environment variable\n");
    vstream_printf(out, "  unset NAME           Remove virtual environment variable\n");
    vstream_printf(out, "  type [command]       Display command classification\n");
    vstream_printf(out, "\nSupported operators:\n");
    vstream_printf(out, "  cmd1 | cmd2          In-memory pipeline\n");
    vstream_printf(out, "  cmd < file           Input redirection from virtual file\n");
    vstream_printf(out, "  cmd > file           Output redirection to virtual file (overwrite)\n");
    vstream_printf(out, "  cmd >> file          Output redirection to virtual file (append)\n");
    vstream_printf(out, "  cmd 2> file          Error redirection to virtual file (overwrite)\n");
    vstream_printf(out, "  cmd 2>> file         Error redirection to virtual file (append)\n");
    vstream_printf(out, "  cmd1 ; cmd2          Sequential execution\n");
    return 0;
}

static int cmd_pwd(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)argc; (void)argv; (void)in; (void)err;
    vstream_printf(out, "%s\n", vfs_getcwd(sh->vfs));
    return 0;
}

static int cmd_cd(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in; (void)out;
    const char *target = (argc > 1) ? argv[1] : virtual_shell_getenv(sh, "HOME");
    if (!target) target = "/home/user";
    if (vfs_chdir(sh->vfs, target) != 0) {
        vstream_printf(err, "minishell: cd: %s: No such file or directory\n", target);
        return 1;
    }
    return 0;
}

static int cmd_ls(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in;
    bool show_long = false;
    bool show_all = false;
    const char *path = ".";

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (size_t j = 1; argv[i][j] != '\0'; j++) {
                if (argv[i][j] == 'l') show_long = true;
                else if (argv[i][j] == 'a') show_all = true;
            }
        } else {
            path = argv[i];
        }
    }

    char canon[1024];
    if (vfs_resolve_path(sh->vfs, path, canon, sizeof(canon)) != 0) {
        vstream_printf(err, "minishell: ls: %s: Invalid path\n", path);
        return 1;
    }

    vfs_node_t *node = vfs_find_node(sh->vfs, canon);
    if (!node) {
        vstream_printf(err, "minishell: ls: %s: No such file or directory\n", path);
        return 1;
    }

    if (node->type == VFS_NODE_FILE) {
        if (show_long) {
            vstream_printf(out, "-rw-r--r-- 1 user staff %6zu %s\n", node->size, node->name);
        } else {
            vstream_printf(out, "%s\n", node->name);
        }
        return 0;
    }

    if (show_all && show_long) {
        vstream_printf(out, "drwxr-xr-x 2 user staff    128 .\n");
        vstream_printf(out, "drwxr-xr-x 2 user staff    128 ..\n");
    } else if (show_all) {
        vstream_printf(out, ".\n..\n");
    }

    for (size_t i = 0; i < node->child_count; i++) {
        vfs_node_t *child = node->children[i];
        if (show_long) {
            const char *mode = (child->type == VFS_NODE_DIR) ? "drwxr-xr-x" : "-rw-r--r--";
            size_t sz = (child->type == VFS_NODE_DIR) ? 128 : child->size;
            vstream_printf(out, "%s 1 user staff %6zu %s%s\n", mode, sz, child->name,
                           (child->type == VFS_NODE_DIR) ? "/" : "");
        } else {
            vstream_printf(out, "%s%s\n", child->name, (child->type == VFS_NODE_DIR) ? "/" : "");
        }
    }

    return 0;
}

static int cmd_echo(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in; (void)err; (void)sh;
    for (int i = 1; i < argc; i++) {
        vstream_printf(out, "%s", argv[i]);
        if (i < argc - 1) {
            vstream_append_str(out, " ");
        }
    }
    vstream_append_str(out, "\n");
    return 0;
}

static int cmd_cat(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    if (argc == 1) {
        if (in && in->len > 0) {
            vstream_append(out, in->data, in->len);
        }
        return 0;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            if (in && in->len > 0) vstream_append(out, in->data, in->len);
            continue;
        }
        char *content = NULL;
        size_t size = 0;
        if (vfs_read_file(sh->vfs, argv[i], &content, &size) != 0) {
            vstream_printf(err, "minishell: cat: %s: No such file or directory\n", argv[i]);
            status = 1;
            continue;
        }
        if (content && size > 0) {
            vstream_append(out, content, size);
        }
        if (content) free(content);
    }
    return status;
}

static int cmd_head(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    int max_lines = 10;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_lines = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            file = argv[i];
        }
    }

    char *data = NULL;
    size_t size = 0;
    bool free_data = false;

    if (file) {
        if (vfs_read_file(sh->vfs, file, &data, &size) != 0) {
            vstream_printf(err, "minishell: head: %s: No such file or directory\n", file);
            return 1;
        }
        free_data = true;
    } else if (in && in->len > 0) {
        data = in->data;
        size = in->len;
    }

    if (!data) return 0;

    int line_count = 0;
    for (size_t i = 0; i < size && line_count < max_lines; i++) {
        vstream_append(out, &data[i], 1);
        if (data[i] == '\n') line_count++;
    }

    if (free_data && data) free(data);
    return 0;
}

static int cmd_tail(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    int max_lines = 10;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_lines = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            file = argv[i];
        }
    }

    char *data = NULL;
    size_t size = 0;
    bool free_data = false;

    if (file) {
        if (vfs_read_file(sh->vfs, file, &data, &size) != 0) {
            vstream_printf(err, "minishell: tail: %s: No such file or directory\n", file);
            return 1;
        }
        free_data = true;
    } else if (in && in->len > 0) {
        data = in->data;
        size = in->len;
    }

    if (!data) return 0;

    /* Count total newlines */
    int total_lines = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\n') total_lines++;
    }
    if (size > 0 && data[size - 1] != '\n') total_lines++;

    int start_from_line = total_lines > max_lines ? total_lines - max_lines : 0;
    int current_line = 0;
    for (size_t i = 0; i < size; i++) {
        if (current_line >= start_from_line) {
            vstream_append(out, &data[i], 1);
        }
        if (data[i] == '\n') current_line++;
    }

    if (free_data && data) free(data);
    return 0;
}

static int cmd_wc(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    bool count_lines = false;
    bool count_words = false;
    bool count_bytes = false;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (size_t j = 1; argv[i][j] != '\0'; j++) {
                if (argv[i][j] == 'l') count_lines = true;
                else if (argv[i][j] == 'w') count_words = true;
                else if (argv[i][j] == 'c') count_bytes = true;
            }
        } else {
            file = argv[i];
        }
    }

    if (!count_lines && !count_words && !count_bytes) {
        count_lines = true;
        count_words = true;
        count_bytes = true;
    }

    char *data = NULL;
    size_t size = 0;
    bool free_data = false;

    if (file) {
        if (vfs_read_file(sh->vfs, file, &data, &size) != 0) {
            vstream_printf(err, "minishell: wc: %s: No such file or directory\n", file);
            return 1;
        }
        free_data = true;
    } else if (in && in->len > 0) {
        data = in->data;
        size = in->len;
    }

    size_t lines = 0, words = 0, bytes = size;
    bool in_word = false;

    if (data) {
        for (size_t i = 0; i < size; i++) {
            if (data[i] == '\n') lines++;
            if (isspace((unsigned char)data[i])) {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                words++;
            }
        }
    }

    if (count_lines) vstream_printf(out, " %6zu", lines);
    if (count_words) vstream_printf(out, " %6zu", words);
    if (count_bytes) vstream_printf(out, " %6zu", bytes);
    if (file) vstream_printf(out, " %s", file);
    vstream_append_str(out, "\n");

    if (free_data && data) free(data);
    return 0;
}

static int cmd_grep(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    bool invert = false;
    bool ignore_case = false;
    const char *pattern = NULL;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (size_t j = 1; argv[i][j] != '\0'; j++) {
                if (argv[i][j] == 'v') invert = true;
                else if (argv[i][j] == 'i') ignore_case = true;
            }
        } else if (!pattern) {
            pattern = argv[i];
        } else if (!file) {
            file = argv[i];
        }
    }

    if (!pattern) {
        vstream_printf(err, "minishell: grep: missing pattern\n");
        return 2;
    }

    char *data = NULL;
    size_t size = 0;
    bool free_data = false;

    if (file) {
        if (vfs_read_file(sh->vfs, file, &data, &size) != 0) {
            vstream_printf(err, "minishell: grep: %s: No such file or directory\n", file);
            return 2;
        }
        free_data = true;
    } else if (in && in->len > 0) {
        data = in->data;
        size = in->len;
    }

    if (!data) return 1; /* No match */

    bool match_found = false;
    size_t line_start = 0;

    for (size_t i = 0; i <= size; i++) {
        if (i == size || data[i] == '\n') {
            size_t line_len = i - line_start;
            char line_buf[1024];
            if (line_len < sizeof(line_buf)) {
                memcpy(line_buf, data + line_start, line_len);
                line_buf[line_len] = '\0';

                bool matched = false;
                if (ignore_case) {
                    char l_line[1024], l_pat[256];
                    for (size_t k = 0; k <= line_len && k < sizeof(l_line) - 1; k++) {
                        l_line[k] = (char)tolower((unsigned char)line_buf[k]);
                    }
                    size_t plen = strlen(pattern);
                    for (size_t k = 0; k <= plen && k < sizeof(l_pat) - 1; k++) {
                        l_pat[k] = (char)tolower((unsigned char)pattern[k]);
                    }
                    matched = (strstr(l_line, l_pat) != NULL);
                } else {
                    matched = (strstr(line_buf, pattern) != NULL);
                }

                if (invert) matched = !matched;

                if (matched) {
                    match_found = true;
                    vstream_printf(out, "%s\n", line_buf);
                }
            }
            line_start = i + 1;
        }
    }

    if (free_data && data) free(data);
    return match_found ? 0 : 1;
}

static int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static int compare_strings_reverse(const void *a, const void *b) {
    return strcmp(*(const char **)b, *(const char **)a);
}

static int cmd_sort(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    bool reverse = false;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) reverse = true;
        else if (argv[i][0] != '-') file = argv[i];
    }

    char *data = NULL;
    size_t size = 0;
    bool free_data = false;

    if (file) {
        if (vfs_read_file(sh->vfs, file, &data, &size) != 0) {
            vstream_printf(err, "minishell: sort: %s: No such file or directory\n", file);
            return 2;
        }
        free_data = true;
    } else if (in && in->len > 0) {
        data = in->data;
        size = in->len;
    }

    if (!data || size == 0) return 0;

    /* Collect lines */
    char **lines = NULL;
    size_t count = 0;
    size_t cap = 0;
    size_t line_start = 0;

    for (size_t i = 0; i <= size; i++) {
        if (i == size || data[i] == '\n') {
            size_t llen = i - line_start;
            char *s = (char *)malloc(llen + 1);
            if (s) {
                memcpy(s, data + line_start, llen);
                s[llen] = '\0';
                if (count >= cap) {
                    cap = cap == 0 ? 16 : cap * 2;
                    lines = (char **)realloc(lines, sizeof(char *) * cap);
                }
                lines[count++] = s;
            }
            line_start = i + 1;
        }
    }

    qsort(lines, count, sizeof(char *), reverse ? compare_strings_reverse : compare_strings);

    for (size_t i = 0; i < count; i++) {
        vstream_printf(out, "%s\n", lines[i]);
        free(lines[i]);
    }
    if (lines) free(lines);
    if (free_data && data) free(data);
    return 0;
}

static int cmd_uniq(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    bool show_count = false;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) show_count = true;
        else if (argv[i][0] != '-') file = argv[i];
    }

    char *data = NULL;
    size_t size = 0;
    bool free_data = false;

    if (file) {
        if (vfs_read_file(sh->vfs, file, &data, &size) != 0) {
            vstream_printf(err, "minishell: uniq: %s: No such file or directory\n", file);
            return 1;
        }
        free_data = true;
    } else if (in && in->len > 0) {
        data = in->data;
        size = in->len;
    }

    if (!data || size == 0) return 0;

    char prev_line[1024] = {0};
    int repeat_count = 0;
    bool has_prev = false;
    size_t line_start = 0;

    for (size_t i = 0; i <= size; i++) {
        if (i == size || data[i] == '\n') {
            size_t llen = i - line_start;
            char curr_line[1024];
            if (llen < sizeof(curr_line)) {
                memcpy(curr_line, data + line_start, llen);
                curr_line[llen] = '\0';

                if (has_prev) {
                    if (strcmp(prev_line, curr_line) == 0) {
                        repeat_count++;
                    } else {
                        if (show_count) vstream_printf(out, " %4d %s\n", repeat_count, prev_line);
                        else vstream_printf(out, "%s\n", prev_line);
                        strncpy(prev_line, curr_line, sizeof(prev_line) - 1);
                        repeat_count = 1;
                    }
                } else {
                    strncpy(prev_line, curr_line, sizeof(prev_line) - 1);
                    repeat_count = 1;
                    has_prev = true;
                }
            }
            line_start = i + 1;
        }
    }

    if (has_prev) {
        if (show_count) vstream_printf(out, " %4d %s\n", repeat_count, prev_line);
        else vstream_printf(out, "%s\n", prev_line);
    }

    if (free_data && data) free(data);
    return 0;
}

static int cmd_mkdir(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in; (void)out;
    if (argc < 2) {
        vstream_printf(err, "minishell: mkdir: missing operand\n");
        return 1;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (vfs_mkdir(sh->vfs, argv[i]) != 0) {
            vstream_printf(err, "minishell: mkdir: cannot create directory '%s'\n", argv[i]);
            status = 1;
        }
    }
    return status;
}

static int cmd_touch(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in; (void)out;
    if (argc < 2) {
        vstream_printf(err, "minishell: touch: missing file operand\n");
        return 1;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (vfs_touch(sh->vfs, argv[i]) != 0) {
            vstream_printf(err, "minishell: touch: cannot touch '%s'\n", argv[i]);
            status = 1;
        }
    }
    return status;
}

static int cmd_rm(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in; (void)out;
    bool recursive = false;
    int status = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-rf") == 0) {
            recursive = true;
        } else {
            if (vfs_rm(sh->vfs, argv[i], recursive) != 0) {
                vstream_printf(err, "minishell: rm: cannot remove '%s'\n", argv[i]);
                status = 1;
            }
        }
    }
    return status;
}

static int cmd_cp(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in; (void)out;
    if (argc < 3) {
        vstream_printf(err, "minishell: cp: missing destination file operand\n");
        return 1;
    }
    if (vfs_cp(sh->vfs, argv[1], argv[2]) != 0) {
        vstream_printf(err, "minishell: cp: cannot copy '%s' to '%s'\n", argv[1], argv[2]);
        return 1;
    }
    return 0;
}

static int cmd_mv(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in; (void)out;
    if (argc < 3) {
        vstream_printf(err, "minishell: mv: missing destination file operand\n");
        return 1;
    }
    if (vfs_mv(sh->vfs, argv[1], argv[2]) != 0) {
        vstream_printf(err, "minishell: mv: cannot move '%s' to '%s'\n", argv[1], argv[2]);
        return 1;
    }
    return 0;
}

static int cmd_clear(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)argc; (void)argv; (void)in; (void)err; (void)sh;
    vstream_printf(out, "\033[H\033[2J\033[3J");
    return 0;
}

static int cmd_history(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)argc; (void)argv; (void)in; (void)err;
    if (sh && sh->history) {
        size_t count = history_count(sh->history);
        for (size_t i = 0; i < count; i++) {
            vstream_printf(out, "  %4zu  %s\n", i + 1, history_get(sh->history, i));
        }
    }
    return 0;
}

static int cmd_env(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)argc; (void)argv; (void)in; (void)err;
    if (sh) {
        for (size_t i = 0; i < sh->env_count; i++) {
            vstream_printf(out, "%s=%s\n", sh->env[i].key, sh->env[i].value);
        }
    }
    return 0;
}

static int cmd_export(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in;
    if (argc == 1) {
        return cmd_env(argc, argv, in, out, err, sh);
    }
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            virtual_shell_setenv(sh, argv[i], eq + 1);
            *eq = '=';
        } else {
            virtual_shell_setenv(sh, argv[i], "");
        }
    }
    return 0;
}

static int cmd_unset(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in; (void)out; (void)err;
    for (int i = 1; i < argc; i++) {
        virtual_shell_unsetenv(sh, argv[i]);
    }
    return 0;
}

static int cmd_type(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    (void)in; (void)sh;
    if (argc < 2) {
        vstream_printf(err, "minishell: type: missing operand\n");
        return 1;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        const char *c = argv[i];
        if (strcmp(c, "help") == 0 || strcmp(c, "pwd") == 0 || strcmp(c, "cd") == 0 ||
            strcmp(c, "ls") == 0 || strcmp(c, "echo") == 0 || strcmp(c, "cat") == 0 ||
            strcmp(c, "head") == 0 || strcmp(c, "tail") == 0 || strcmp(c, "wc") == 0 ||
            strcmp(c, "grep") == 0 || strcmp(c, "sort") == 0 || strcmp(c, "uniq") == 0 ||
            strcmp(c, "mkdir") == 0 || strcmp(c, "touch") == 0 || strcmp(c, "rm") == 0 ||
            strcmp(c, "cp") == 0 || strcmp(c, "mv") == 0 || strcmp(c, "clear") == 0 ||
            strcmp(c, "history") == 0 || strcmp(c, "env") == 0 || strcmp(c, "export") == 0 ||
            strcmp(c, "unset") == 0 || strcmp(c, "type") == 0) {
            vstream_printf(out, "%s is a shell builtin\n", c);
        } else {
            vstream_printf(err, "minishell: type: %s: not found\n", c);
            status = 1;
        }
    }
    return status;
}

/* Command Dispatch Table */
typedef struct {
    const char *name;
    int (*fn)(int argc, char **argv, const vstream_t *in, vstream_t *out, vstream_t *err, virtual_shell_t *sh);
} virtual_cmd_entry_t;

static const virtual_cmd_entry_t VIRTUAL_COMMANDS[] = {
    {"help",    cmd_help},
    {"pwd",     cmd_pwd},
    {"cd",      cmd_cd},
    {"ls",      cmd_ls},
    {"echo",    cmd_echo},
    {"cat",     cmd_cat},
    {"head",    cmd_head},
    {"tail",    cmd_tail},
    {"wc",      cmd_wc},
    {"grep",    cmd_grep},
    {"sort",    cmd_sort},
    {"uniq",    cmd_uniq},
    {"mkdir",   cmd_mkdir},
    {"touch",   cmd_touch},
    {"rm",      cmd_rm},
    {"cp",      cmd_cp},
    {"mv",      cmd_mv},
    {"clear",   cmd_clear},
    {"history", cmd_history},
    {"env",     cmd_env},
    {"export",  cmd_export},
    {"unset",   cmd_unset},
    {"type",    cmd_type},
    {NULL, NULL}
};

static int execute_single_virtual_command(command_t *cmd, const vstream_t *in,
                                         vstream_t *out, vstream_t *err, virtual_shell_t *sh) {
    if (cmd->argc == 0) return 0;
    const char *name = cmd->argv[0];

    for (size_t i = 0; VIRTUAL_COMMANDS[i].name != NULL; i++) {
        if (strcmp(VIRTUAL_COMMANDS[i].name, name) == 0) {
            return VIRTUAL_COMMANDS[i].fn(cmd->argc, cmd->argv, in, out, err, sh);
        }
    }

    /* PRD 4.8: Safe rejection of unlisted commands */
    vstream_printf(err, "minishell: %s: command not available in browser demo\n", name);
    return 127;
}

static int execute_virtual_pipeline(pipeline_t *pipeline, virtual_shell_t *sh,
                                    vstream_t *final_out, vstream_t *final_err) {
    if (!pipeline || pipeline->count == 0) return 0;

    vstream_t current_in;
    vstream_init(&current_in);

    int last_status = 0;

    for (size_t i = 0; i < pipeline->count; i++) {
        command_t *cmd = &pipeline->commands[i];
        bool is_last = (i == pipeline->count - 1);

        /* Handle input redirection (< file) */
        if (cmd->stdin_file) {
            char *file_data = NULL;
            size_t file_size = 0;
            if (vfs_read_file(sh->vfs, cmd->stdin_file, &file_data, &file_size) != 0) {
                vstream_printf(final_err, "minishell: %s: No such file or directory\n", cmd->stdin_file);
                vstream_free(&current_in);
                return 1;
            }
            vstream_free(&current_in);
            vstream_init(&current_in);
            if (file_data && file_size > 0) {
                vstream_append(&current_in, file_data, file_size);
            }
            if (file_data) free(file_data);
        }

        vstream_t stage_out;
        vstream_init(&stage_out);

        vstream_t stage_err;
        vstream_init(&stage_err);

        last_status = execute_single_virtual_command(cmd, &current_in, &stage_out, &stage_err, sh);

        /* Handle stderr redirection (2> or 2>>) */
        if (cmd->stderr_file) {
            if (stage_err.len > 0) {
                vfs_write_file(sh->vfs, cmd->stderr_file, stage_err.data, stage_err.len, cmd->stderr_append);
            }
        } else {
            if (stage_err.len > 0) {
                vstream_append(final_err, stage_err.data, stage_err.len);
            }
        }
        vstream_free(&stage_err);

        /* Handle stdout redirection (> or >>) */
        if (cmd->stdout_file) {
            vfs_write_file(sh->vfs, cmd->stdout_file, stage_out.data, stage_out.len, cmd->stdout_append);
            vstream_free(&current_in);
            vstream_init(&current_in); /* empty output for next stage */
        } else {
            if (is_last) {
                vstream_append(final_out, stage_out.data, stage_out.len);
            } else {
                vstream_free(&current_in);
                current_in = stage_out;
                stage_out.data = NULL;
            }
        }
        vstream_free(&stage_out);
    }

    vstream_free(&current_in);
    return last_status;
}

int execute_command_line_virtual(virtual_shell_t *sh, const command_line_t *cmd_line,
                                 vstream_t *out_stream, vstream_t *err_stream) {
    if (!sh || !cmd_line) return 0;

    int last_status = sh->last_exit_status;
    for (size_t i = 0; i < cmd_line->count; i++) {
        last_status = execute_virtual_pipeline(&cmd_line->pipelines[i], sh, out_stream, err_stream);
        sh->last_exit_status = last_status;
    }
    return last_status;
}

static const char *get_venv_cb(const char *name, void *user_data) {
    virtual_shell_t *sh = (virtual_shell_t *)user_data;
    return virtual_shell_getenv(sh, name);
}

int virtual_shell_execute(virtual_shell_t *sh, const char *line,
                          vstream_t *out_stream, vstream_t *err_stream) {
    if (!sh || !line) return 0;

    /* Add to session history */
    history_add(sh->history, line);

    char **sub_cmds = NULL;
    size_t sub_count = 0;
    if (split_by_semicolon(line, &sub_cmds, &sub_count) != 0) {
        vstream_printf(err_stream, "minishell: syntax error splitting commands\n");
        sh->last_exit_status = 2;
        return 2;
    }

    int status = sh->last_exit_status;
    for (size_t s = 0; s < sub_count; s++) {
        const char *sub = sub_cmds[s];
        while (*sub == ' ' || *sub == '\t' || *sub == '\r' || *sub == '\n') sub++;
        if (*sub == '\0') continue;

        expand_context_t exp_ctx;
        exp_ctx.last_exit_status = sh->last_exit_status;
        exp_ctx.get_env_var = get_venv_cb;
        exp_ctx.user_data = sh;

        token_list_t *tokens = token_list_create();
        if (!tokens) {
            vstream_printf(err_stream, "minishell: out of memory\n");
            sh->last_exit_status = 1;
            status = 1;
            break;
        }

        char err_buf[512] = {0};
        if (tokenize(sub_cmds[s], &exp_ctx, tokens, err_buf, sizeof(err_buf)) != 0) {
            vstream_printf(err_stream, "%s\n", err_buf[0] ? err_buf : "minishell: tokenization error");
            token_list_free(tokens);
            sh->last_exit_status = 2;
            status = 2;
            break;
        }

        command_line_t *cmd_line = parse(tokens, err_buf, sizeof(err_buf));
        token_list_free(tokens);

        if (!cmd_line) {
            vstream_printf(err_stream, "%s\n", err_buf[0] ? err_buf : "minishell: parse error");
            sh->last_exit_status = 2;
            status = 2;
            break;
        }

        status = execute_command_line_virtual(sh, cmd_line, out_stream, err_stream);
        command_line_free(cmd_line);
        sh->last_exit_status = status;
    }

    free_split_commands(sub_cmds, sub_count);
    return status;
}
