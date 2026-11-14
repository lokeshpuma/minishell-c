#include "executor_posix.h"
#include "signals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

static int apply_redirections(const command_t *cmd) {
    if (cmd->stdin_file) {
        int fd = open(cmd->stdin_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "minishell: %s: %s\n", cmd->stdin_file, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            fprintf(stderr, "minishell: dup2 failed: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (cmd->stdout_file) {
        int flags = O_WRONLY | O_CREAT | (cmd->stdout_append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->stdout_file, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "minishell: %s: %s\n", cmd->stdout_file, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            fprintf(stderr, "minishell: dup2 failed: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (cmd->stderr_file) {
        int flags = O_WRONLY | O_CREAT | (cmd->stderr_append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->stderr_file, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "minishell: %s: %s\n", cmd->stderr_file, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDERR_FILENO) < 0) {
            fprintf(stderr, "minishell: dup2 failed: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        close(fd);
    }

    return 0;
}

static int execute_parent_builtin(command_t *cmd, shell_context_t *ctx) {
    int saved_stdin = -1;
    int saved_stdout = -1;
    int saved_stderr = -1;

    if (cmd->stdin_file) {
        saved_stdin = dup(STDIN_FILENO);
    }
    if (cmd->stdout_file) {
        saved_stdout = dup(STDOUT_FILENO);
    }
    if (cmd->stderr_file) {
        saved_stderr = dup(STDERR_FILENO);
    }

    if (apply_redirections(cmd) != 0) {
        if (saved_stdin >= 0) { dup2(saved_stdin, STDIN_FILENO); close(saved_stdin); }
        if (saved_stdout >= 0) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
        if (saved_stderr >= 0) { dup2(saved_stderr, STDERR_FILENO); close(saved_stderr); }
        return 1;
    }

    int status = execute_builtin(cmd->argc, cmd->argv, ctx);
    fflush(stdout);
    fflush(stderr);

    if (saved_stdin >= 0) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (saved_stderr >= 0) {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }

    return status;
}

int execute_pipeline_posix(pipeline_t *pipeline, shell_context_t *ctx) {
    if (!pipeline || pipeline->count == 0) {
        return 0;
    }

    size_t n = pipeline->count;

    /*
     * PRD Section 4.2:
     * "State-changing built-ins such as cd, export, unset and exit must execute
     * in the parent shell when used as a standalone command. If they appear in a
     * pipeline, execute them in a child context so they do not change the parent shell state."
     */
    if (n == 1) {
        command_t *cmd = &pipeline->commands[0];
        if (cmd->argc > 0 && is_state_changing_builtin(cmd->argv[0])) {
            return execute_parent_builtin(cmd, ctx);
        }
    }

    /* Allocate pipe file descriptors */
    int (*pipes)[2] = NULL;
    if (n > 1) {
        pipes = (int (*)[2])malloc(sizeof(int[2]) * (n - 1));
        if (!pipes) {
            fprintf(stderr, "minishell: out of memory for pipes\n");
            return 1;
        }
        for (size_t i = 0; i < n - 1; i++) {
            if (pipe(pipes[i]) < 0) {
                fprintf(stderr, "minishell: pipe creation failed: %s\n", strerror(errno));
                for (size_t j = 0; j < i; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                free(pipes);
                return 1;
            }
        }
    }

    pid_t *pids = (pid_t *)malloc(sizeof(pid_t) * n);
    if (!pids) {
        fprintf(stderr, "minishell: out of memory for pids\n");
        if (pipes) {
            for (size_t i = 0; i < n - 1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }
            free(pipes);
        }
        return 1;
    }

    /* Fork child processes */
    for (size_t i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            fprintf(stderr, "minishell: fork failed: %s\n", strerror(errno));
            /* Close all pipes */
            if (pipes) {
                for (size_t j = 0; j < n - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
            }
            /* Wait for already forked children */
            for (size_t j = 0; j < i; j++) {
                waitpid(pids[j], NULL, 0);
            }
            if (pipes) free(pipes);
            free(pids);
            return 1;
        }

        if (pids[i] == 0) {
            /* Child process */
            setup_child_signals();

            /* Connect stdin from previous pipe if not first command */
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                    fprintf(stderr, "minishell: dup2 stdin pipe failed: %s\n", strerror(errno));
                    _exit(1);
                }
            }

            /* Connect stdout to next pipe if not last command */
            if (i < n - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    fprintf(stderr, "minishell: dup2 stdout pipe failed: %s\n", strerror(errno));
                    _exit(1);
                }
            }

            /* Close all pipe descriptors in child */
            if (pipes) {
                for (size_t j = 0; j < n - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
            }

            /* Apply file redirections */
            if (apply_redirections(&pipeline->commands[i]) != 0) {
                _exit(1);
            }

            command_t *cmd = &pipeline->commands[i];
            if (cmd->argc == 0) {
                _exit(0);
            }

            /* Execute built-in or external program */
            if (is_builtin(cmd->argv[0])) {
                int status = execute_builtin(cmd->argc, cmd->argv, ctx);
                fflush(stdout);
                fflush(stderr);
                _exit(status);
            }

            execvp(cmd->argv[0], cmd->argv);

            /* If execvp returns, an error occurred */
            if (errno == ENOENT) {
                fprintf(stderr, "minishell: %s: command not found\n", cmd->argv[0]);
                _exit(127);
            } else if (errno == EACCES) {
                fprintf(stderr, "minishell: %s: Permission denied\n", cmd->argv[0]);
                _exit(126);
            } else {
                fprintf(stderr, "minishell: %s: %s\n", cmd->argv[0], strerror(errno));
                _exit(126);
            }
        }
    }

    /* Parent process: close all pipe fds immediately to avoid deadlocks */
    if (pipes) {
        for (size_t i = 0; i < n - 1; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
        free(pipes);
    }

    /* Temporarily ignore SIGINT in parent while waiting for child foreground pipeline */
    ignore_signals();

    int last_status = 0;
    for (size_t i = 0; i < n; i++) {
        int status = 0;
        waitpid(pids[i], &status, 0);

        if (i == n - 1) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                int sig = WTERMSIG(status);
                last_status = 128 + sig;
                if (sig == SIGINT) {
                    /* Print newline on SIGINT */
                    fprintf(stderr, "\n");
                }
            }
        }
    }

    /* Restore parent shell signal handlers */
    setup_shell_signals();
    free(pids);
    return last_status;
}

int execute_command_line_posix(command_line_t *cmd_line, shell_context_t *ctx) {
    if (!cmd_line || !ctx) return 0;

    int last_status = ctx->last_exit_status;
    for (size_t i = 0; i < cmd_line->count; i++) {
        last_status = execute_pipeline_posix(&cmd_line->pipelines[i], ctx);
        ctx->last_exit_status = last_status;
        if (ctx->should_exit) {
            break;
        }
    }
    return last_status;
}

