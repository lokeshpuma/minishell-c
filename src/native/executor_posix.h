#ifndef MINISHELL_EXECUTOR_POSIX_H
#define MINISHELL_EXECUTOR_POSIX_H

#include "../common/command.h"
#include "../common/builtins.h"

int execute_pipeline_posix(pipeline_t *pipeline, shell_context_t *ctx);
int execute_command_line_posix(command_line_t *cmd_line, shell_context_t *ctx);

#endif /* MINISHELL_EXECUTOR_POSIX_H */

