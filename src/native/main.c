#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "../common/tokenizer.h"
#include "../common/parser.h"
#include "../common/command.h"
#include "../common/builtins.h"
#include "../common/history.h"
#include "executor_posix.h"
#include "signals.h"

static void print_prompt(void) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("minishell:%s$ ", cwd);
    } else {
        printf("minishell$ ");
    }
    fflush(stdout);
}

static bool is_all_whitespace(const char *str) {
    while (*str) {
        if (*str != ' ' && *str != '\t' && *str != '\r' && *str != '\n') {
            return false;
        }
        str++;
    }
    return true;
}

static int process_line(char *line, shell_context_t *ctx) {
    /* Strip trailing newline */
    size_t len = strlen(line);
    if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
    if (len > 0 && line[len - 1] == '\r') {
        line[len - 1] = '\0';
        len--;
    }

    if (is_all_whitespace(line)) {
        return 0;
    }

    /* Add to history */
    history_add(ctx->history, line);

    /* Split by semicolon so variables ($VAR, $?) reflect changes from earlier commands */
    char **sub_cmds = NULL;
    size_t sub_count = 0;
    if (split_by_semicolon(line, &sub_cmds, &sub_count) != 0) {
        fprintf(stderr, "minishell: syntax error splitting commands\n");
        ctx->last_exit_status = 2;
        return 2;
    }

    for (size_t s = 0; s < sub_count; s++) {
        if (is_all_whitespace(sub_cmds[s])) {
            continue;
        }

        expand_context_t exp_ctx;
        exp_ctx.last_exit_status = ctx->last_exit_status;
        exp_ctx.get_env_var = NULL;
        exp_ctx.user_data = NULL;

        token_list_t *tokens = token_list_create();
        if (!tokens) {
            fprintf(stderr, "minishell: out of memory\n");
            ctx->last_exit_status = 1;
            break;
        }

        char err_buf[512] = {0};
        if (tokenize(sub_cmds[s], &exp_ctx, tokens, err_buf, sizeof(err_buf)) != 0) {
            fprintf(stderr, "%s\n", err_buf[0] ? err_buf : "minishell: tokenization error");
            token_list_free(tokens);
            ctx->last_exit_status = 2;
            break;
        }

        command_line_t *cmd_line = parse(tokens, err_buf, sizeof(err_buf));
        token_list_free(tokens);

        if (!cmd_line) {
            fprintf(stderr, "%s\n", err_buf[0] ? err_buf : "minishell: parse error");
            ctx->last_exit_status = 2;
            break;
        }

        execute_command_line_posix(cmd_line, ctx);
        command_line_free(cmd_line);

        if (ctx->should_exit) {
            break;
        }
    }

    free_split_commands(sub_cmds, sub_count);
    return ctx->last_exit_status;
}

int main(int argc, char **argv) {
    shell_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.history = history_create();
    ctx.last_exit_status = 0;
    ctx.should_exit = false;
    ctx.exit_status = 0;

    /* Handle -c "command" mode */
    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        char *line = strdup(argv[2]);
        if (line) {
            process_line(line, &ctx);
            free(line);
        }
        int status = ctx.should_exit ? ctx.exit_status : ctx.last_exit_status;
        history_free(ctx.history);
        return status;
    }

    setup_shell_signals();

    bool is_interactive = isatty(STDIN_FILENO);

    char *line = NULL;
    size_t linecap = 0;

    while (!ctx.should_exit) {
        if (g_sigint_received) {
            g_sigint_received = 0;
            ctx.last_exit_status = 130;
        }

        if (is_interactive) {
            print_prompt();
        }

        errno = 0;
        ssize_t nread = getline(&line, &linecap, stdin);

        if (nread == -1) {
            if (errno == EINTR || g_sigint_received) {
                g_sigint_received = 0;
                ctx.last_exit_status = 130;
                clearerr(stdin);
                continue;
            }
            /* End of file (Ctrl+D) */
            if (is_interactive) {
                printf("exit\n");
            }
            break;
        }

        process_line(line, &ctx);
    }

    if (line) {
        free(line);
    }

    int final_status = ctx.should_exit ? ctx.exit_status : ctx.last_exit_status;
    history_free(ctx.history);

    return final_status;
}
