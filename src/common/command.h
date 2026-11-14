#ifndef MINISHELL_COMMAND_H
#define MINISHELL_COMMAND_H

#include <stddef.h>
#include <stdbool.h>

#define MINISHELL_MAX_PIPELINE_STAGES 64

typedef struct {
    char **argv;        /* NULL-terminated array of arguments */
    int argc;
    char *stdin_file;   /* < file (NULL if none) */
    char *stdout_file;  /* > or >> file (NULL if none) */
    int stdout_append;  /* 0 for >, 1 for >> */
    char *stderr_file;  /* 2> or 2>> file (NULL if none) */
    int stderr_append;  /* 0 for 2>, 1 for 2>> */
} command_t;

typedef struct {
    command_t *commands; /* Array of commands connected by pipes */
    size_t count;
} pipeline_t;

typedef struct {
    pipeline_t *pipelines; /* Array of pipelines separated by semicolons */
    size_t count;
} command_line_t;

void command_init(command_t *cmd);
void command_free_internals(command_t *cmd);
int command_add_arg(command_t *cmd, const char *arg);

pipeline_t *pipeline_create(void);
void pipeline_free_internals(pipeline_t *pipeline);
void pipeline_free(pipeline_t *pipeline);

command_line_t *command_line_create(void);
void command_line_free(command_line_t *cmd_line);

int split_by_semicolon(const char *input, char ***out_commands, size_t *out_count);
void free_split_commands(char **commands, size_t count);

#endif /* MINISHELL_COMMAND_H */
