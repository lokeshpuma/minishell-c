#define _POSIX_C_SOURCE 200809L
#include "command.h"
#include <stdlib.h>
#include <string.h>

void command_init(command_t *cmd) {
    if (!cmd) return;
    cmd->argv = NULL;
    cmd->argc = 0;
    cmd->stdin_file = NULL;
    cmd->stdout_file = NULL;
    cmd->stdout_append = 0;
    cmd->stderr_file = NULL;
    cmd->stderr_append = 0;
}

void command_free_internals(command_t *cmd) {
    if (!cmd) return;
    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) {
            if (cmd->argv[i]) {
                free(cmd->argv[i]);
            }
        }
        free(cmd->argv);
        cmd->argv = NULL;
    }
    cmd->argc = 0;

    if (cmd->stdin_file) {
        free(cmd->stdin_file);
        cmd->stdin_file = NULL;
    }
    if (cmd->stdout_file) {
        free(cmd->stdout_file);
        cmd->stdout_file = NULL;
    }
    if (cmd->stderr_file) {
        free(cmd->stderr_file);
        cmd->stderr_file = NULL;
    }
    cmd->stdout_append = 0;
    cmd->stderr_append = 0;
}

int command_add_arg(command_t *cmd, const char *arg) {
    if (!cmd || !arg) return -1;
    char **new_argv = (char **)realloc(cmd->argv, sizeof(char *) * (cmd->argc + 2));
    if (!new_argv) return -1;
    cmd->argv = new_argv;
    cmd->argv[cmd->argc] = strdup(arg);
    if (!cmd->argv[cmd->argc]) return -1;
    cmd->argc++;
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

pipeline_t *pipeline_create(void) {
    pipeline_t *pipeline = (pipeline_t *)malloc(sizeof(pipeline_t));
    if (!pipeline) return NULL;
    pipeline->commands = NULL;
    pipeline->count = 0;
    return pipeline;
}

void pipeline_free_internals(pipeline_t *pipeline) {
    if (!pipeline) return;
    if (pipeline->commands) {
        for (size_t i = 0; i < pipeline->count; i++) {
            command_free_internals(&pipeline->commands[i]);
        }
        free(pipeline->commands);
        pipeline->commands = NULL;
    }
    pipeline->count = 0;
}

void pipeline_free(pipeline_t *pipeline) {
    if (!pipeline) return;
    pipeline_free_internals(pipeline);
    free(pipeline);
}

command_line_t *command_line_create(void) {
    command_line_t *cmd_line = (command_line_t *)malloc(sizeof(command_line_t));
    if (!cmd_line) return NULL;
    cmd_line->pipelines = NULL;
    cmd_line->count = 0;
    return cmd_line;
}

void command_line_free(command_line_t *cmd_line) {
    if (!cmd_line) return;
    if (cmd_line->pipelines) {
        for (size_t i = 0; i < cmd_line->count; i++) {
            pipeline_free_internals(&cmd_line->pipelines[i]);
        }
        free(cmd_line->pipelines);
    }
    free(cmd_line);
}

int split_by_semicolon(const char *input, char ***out_commands, size_t *out_count) {
    if (!input || !out_commands || !out_count) return -1;
    *out_commands = NULL;
    *out_count = 0;

    size_t len = strlen(input);
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;
    size_t start = 0;

    size_t cap = 8;
    char **cmds = (char **)malloc(sizeof(char *) * cap);
    if (!cmds) return -1;
    size_t count = 0;

    for (size_t i = 0; i <= len; i++) {
        char c = input[i];

        if (c == '\0' || (c == ';' && !in_single && !in_double && !escaped)) {
            size_t seg_len = i - start;
            char *seg = (char *)malloc(seg_len + 1);
            if (!seg) {
                for (size_t j = 0; j < count; j++) free(cmds[j]);
                free(cmds);
                return -1;
            }
            memcpy(seg, input + start, seg_len);
            seg[seg_len] = '\0';

            if (count >= cap) {
                cap *= 2;
                char **new_cmds = (char **)realloc(cmds, sizeof(char *) * cap);
                if (!new_cmds) {
                    free(seg);
                    for (size_t j = 0; j < count; j++) free(cmds[j]);
                    free(cmds);
                    return -1;
                }
                cmds = new_cmds;
            }
            cmds[count++] = seg;
            start = i + 1;
            continue;
        }

        if (escaped) {
            escaped = false;
        } else if (c == '\\' && !in_single) {
            escaped = true;
        } else if (c == '\'' && !in_double) {
            in_single = !in_single;
        } else if (c == '"' && !in_single) {
            in_double = !in_double;
        }
    }

    *out_commands = cmds;
    *out_count = count;
    return 0;
}

void free_split_commands(char **commands, size_t count) {
    if (!commands) return;
    for (size_t i = 0; i < count; i++) {
        if (commands[i]) free(commands[i]);
    }
    free(commands);
}
