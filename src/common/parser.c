#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *err_buf, size_t err_buf_size, const char *msg) {
    if (err_buf && err_buf_size > 0) {
        snprintf(err_buf, err_buf_size, "%s", msg);
    }
}

static void set_error_token(char *err_buf, size_t err_buf_size, const char *token_str) {
    if (err_buf && err_buf_size > 0) {
        snprintf(err_buf, err_buf_size, "minishell: syntax error near unexpected token '%s'", token_str);
    }
}

command_line_t *parse(const token_list_t *tokens, char *err_buf, size_t err_buf_size) {
    if (!tokens) {
        set_error(err_buf, err_buf_size, "minishell: internal error: null token list");
        return NULL;
    }

    command_line_t *cmd_line = command_line_create();
    if (!cmd_line) {
        set_error(err_buf, err_buf_size, "minishell: out of memory");
        return NULL;
    }

    if (tokens->count == 0) {
        return cmd_line;
    }

    size_t i = 0;
    while (i < tokens->count) {
        /* Semicolon check at start of command */
        if (tokens->tokens[i].type == TOKEN_SEMICOLON) {
            set_error_token(err_buf, err_buf_size, ";");
            command_line_free(cmd_line);
            return NULL;
        }

        /* Parse one pipeline */
        pipeline_t current_pipeline;
        current_pipeline.commands = NULL;
        current_pipeline.count = 0;

        bool pipeline_done = false;
        while (i < tokens->count && !pipeline_done) {
            /* Check pipeline stages limit */
            if (current_pipeline.count >= MINISHELL_MAX_PIPELINE_STAGES) {
                char msg[128];
                snprintf(msg, sizeof(msg), "minishell: maximum pipeline stages (%d) exceeded", MINISHELL_MAX_PIPELINE_STAGES);
                set_error(err_buf, err_buf_size, msg);
                pipeline_free_internals(&current_pipeline);
                command_line_free(cmd_line);
                return NULL;
            }

            /* Parse one simple command */
            command_t current_cmd;
            command_init(&current_cmd);

            bool cmd_has_content = false;
            while (i < tokens->count) {
                token_type_t ttype = tokens->tokens[i].type;

                if (ttype == TOKEN_SEMICOLON) {
                    /* Pipeline ends here */
                    i++; /* consume ; */
                    pipeline_done = true;
                    break;
                }

                if (ttype == TOKEN_PIPE) {
                    if (!cmd_has_content) {
                        set_error_token(err_buf, err_buf_size, "|");
                        command_free_internals(&current_cmd);
                        pipeline_free_internals(&current_pipeline);
                        command_line_free(cmd_line);
                        return NULL;
                    }
                    /* Next command in pipeline */
                    i++; /* consume | */
                    if (i >= tokens->count) {
                        set_error_token(err_buf, err_buf_size, "|");
                        command_free_internals(&current_cmd);
                        pipeline_free_internals(&current_pipeline);
                        command_line_free(cmd_line);
                        return NULL;
                    }
                    if (tokens->tokens[i].type == TOKEN_PIPE) {
                        set_error_token(err_buf, err_buf_size, "|");
                        command_free_internals(&current_cmd);
                        pipeline_free_internals(&current_pipeline);
                        command_line_free(cmd_line);
                        return NULL;
                    }
                    if (tokens->tokens[i].type == TOKEN_SEMICOLON) {
                        set_error_token(err_buf, err_buf_size, ";");
                        command_free_internals(&current_cmd);
                        pipeline_free_internals(&current_pipeline);
                        command_line_free(cmd_line);
                        return NULL;
                    }
                    break;
                }

                if (ttype == TOKEN_WORD) {
                    if (command_add_arg(&current_cmd, tokens->tokens[i].value) != 0) {
                        set_error(err_buf, err_buf_size, "minishell: out of memory");
                        command_free_internals(&current_cmd);
                        pipeline_free_internals(&current_pipeline);
                        command_line_free(cmd_line);
                        return NULL;
                    }
                    cmd_has_content = true;
                    i++;
                    continue;
                }

                /* Redirection operators */
                if (ttype == TOKEN_REDIRECT_IN || ttype == TOKEN_REDIRECT_OUT ||
                    ttype == TOKEN_REDIRECT_APPEND || ttype == TOKEN_REDIRECT_ERR ||
                    ttype == TOKEN_REDIRECT_ERR_APPEND) {
                    
                    if (i + 1 >= tokens->count || tokens->tokens[i + 1].type != TOKEN_WORD) {
                        const char *unexpected = (i + 1 < tokens->count) 
                            ? token_type_to_string(tokens->tokens[i + 1].type) 
                            : "newline";
                        set_error_token(err_buf, err_buf_size, unexpected);
                        command_free_internals(&current_cmd);
                        pipeline_free_internals(&current_pipeline);
                        command_line_free(cmd_line);
                        return NULL;
                    }

                    const char *target = tokens->tokens[i + 1].value;
                    cmd_has_content = true;

                    if (ttype == TOKEN_REDIRECT_IN) {
                        if (current_cmd.stdin_file) free(current_cmd.stdin_file);
                        current_cmd.stdin_file = strdup(target);
                    } else if (ttype == TOKEN_REDIRECT_OUT) {
                        if (current_cmd.stdout_file) free(current_cmd.stdout_file);
                        current_cmd.stdout_file = strdup(target);
                        current_cmd.stdout_append = 0;
                    } else if (ttype == TOKEN_REDIRECT_APPEND) {
                        if (current_cmd.stdout_file) free(current_cmd.stdout_file);
                        current_cmd.stdout_file = strdup(target);
                        current_cmd.stdout_append = 1;
                    } else if (ttype == TOKEN_REDIRECT_ERR) {
                        if (current_cmd.stderr_file) free(current_cmd.stderr_file);
                        current_cmd.stderr_file = strdup(target);
                        current_cmd.stderr_append = 0;
                    } else if (ttype == TOKEN_REDIRECT_ERR_APPEND) {
                        if (current_cmd.stderr_file) free(current_cmd.stderr_file);
                        current_cmd.stderr_file = strdup(target);
                        current_cmd.stderr_append = 1;
                    }

                    i += 2; /* consume redirect and target filename */
                    continue;
                }

                /* If we encounter an unexpected token */
                set_error_token(err_buf, err_buf_size, token_type_to_string(ttype));
                command_free_internals(&current_cmd);
                pipeline_free_internals(&current_pipeline);
                command_line_free(cmd_line);
                return NULL;
            }

            if (!cmd_has_content) {
                /* Empty command before pipe or semicolon */
                set_error_token(err_buf, err_buf_size, "|");
                command_free_internals(&current_cmd);
                pipeline_free_internals(&current_pipeline);
                command_line_free(cmd_line);
                return NULL;
            }

            /* Add command to pipeline */
            command_t *new_cmds = (command_t *)realloc(current_pipeline.commands,
                                                       sizeof(command_t) * (current_pipeline.count + 1));
            if (!new_cmds) {
                set_error(err_buf, err_buf_size, "minishell: out of memory");
                command_free_internals(&current_cmd);
                pipeline_free_internals(&current_pipeline);
                command_line_free(cmd_line);
                return NULL;
            }
            current_pipeline.commands = new_cmds;
            current_pipeline.commands[current_pipeline.count] = current_cmd;
            current_pipeline.count++;
        }

        if (current_pipeline.count == 0) {
            /* Empty pipeline, e.g., trailing semicolon */
            continue;
        }

        /* Append current_pipeline to cmd_line */
        pipeline_t *new_pipes = (pipeline_t *)realloc(cmd_line->pipelines,
                                                      sizeof(pipeline_t) * (cmd_line->count + 1));
        if (!new_pipes) {
            set_error(err_buf, err_buf_size, "minishell: out of memory");
            pipeline_free_internals(&current_pipeline);
            command_line_free(cmd_line);
            return NULL;
        }
        cmd_line->pipelines = new_pipes;
        cmd_line->pipelines[cmd_line->count] = current_pipeline;
        cmd_line->count++;
    }

    return cmd_line;
}
