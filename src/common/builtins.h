#ifndef MINISHELL_BUILTINS_H
#define MINISHELL_BUILTINS_H

#include <stdbool.h>
#include "history.h"

typedef struct shell_context shell_context_t;

struct shell_context {
    history_t *history;
    int last_exit_status;
    bool should_exit;
    int exit_status;
    /* Optional environment callbacks for wasm/virtual environment */
    char **(*get_environ)(void *ctx);
    int (*set_env)(const char *name, const char *val, void *ctx);
    int (*unset_env)(const char *name, void *ctx);
    const char *(*get_env)(const char *name, void *ctx);
    char *(*get_cwd)(char *buf, size_t size, void *ctx);
    int (*set_cwd)(const char *path, void *ctx);
    void *platform_data;
};

bool is_builtin(const char *cmd);
bool is_state_changing_builtin(const char *cmd);

/*
 * Executes a built-in command.
 * Returns the exit status (0 for success, non-zero for failure).
 */
int execute_builtin(int argc, char **argv, shell_context_t *ctx);

/* Individual built-in implementations */
int builtin_help(int argc, char **argv, shell_context_t *ctx);
int builtin_exit(int argc, char **argv, shell_context_t *ctx);
int builtin_pwd(int argc, char **argv, shell_context_t *ctx);
int builtin_cd(int argc, char **argv, shell_context_t *ctx);
int builtin_echo(int argc, char **argv, shell_context_t *ctx);
int builtin_clear(int argc, char **argv, shell_context_t *ctx);
int builtin_history(int argc, char **argv, shell_context_t *ctx);
int builtin_env(int argc, char **argv, shell_context_t *ctx);
int builtin_export(int argc, char **argv, shell_context_t *ctx);
int builtin_unset(int argc, char **argv, shell_context_t *ctx);
int builtin_type(int argc, char **argv, shell_context_t *ctx);

#endif /* MINISHELL_BUILTINS_H */

