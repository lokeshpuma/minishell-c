#ifndef MINISHELL_EXECUTOR_VIRTUAL_H
#define MINISHELL_EXECUTOR_VIRTUAL_H

#include <stddef.h>
#include <stdbool.h>
#include "virtual_fs.h"
#include "../common/history.h"
#include "../common/command.h"

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} vstream_t;

void vstream_init(vstream_t *s);
void vstream_free(vstream_t *s);
void vstream_append(vstream_t *s, const char *str, size_t len);
void vstream_append_str(vstream_t *s, const char *str);
void vstream_printf(vstream_t *s, const char *fmt, ...);

typedef struct {
    char *key;
    char *value;
} venv_entry_t;

typedef struct {
    virtual_fs_t *vfs;
    history_t *history;
    venv_entry_t *env;
    size_t env_count;
    size_t env_capacity;
    int last_exit_status;
} virtual_shell_t;

virtual_shell_t *virtual_shell_create(void);
void virtual_shell_destroy(virtual_shell_t *sh);
void virtual_shell_reset(virtual_shell_t *sh);

const char *virtual_shell_getenv(virtual_shell_t *sh, const char *name);
int virtual_shell_setenv(virtual_shell_t *sh, const char *name, const char *value);
int virtual_shell_unsetenv(virtual_shell_t *sh, const char *name);

/*
 * Executes a command line AST in the virtual environment.
 * Output and error messages are written into out_stream and err_stream.
 * Returns the final exit status.
 */
int execute_command_line_virtual(virtual_shell_t *sh, const command_line_t *cmd_line,
                                 vstream_t *out_stream, vstream_t *err_stream);

/* High-level execution of a raw command string */
int virtual_shell_execute(virtual_shell_t *sh, const char *line,
                          vstream_t *out_stream, vstream_t *err_stream);

#endif /* MINISHELL_EXECUTOR_VIRTUAL_H */

