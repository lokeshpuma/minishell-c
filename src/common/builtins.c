#include "builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef _WIN32
extern char **environ;
#endif

bool is_builtin(const char *cmd) {
    if (!cmd) return false;
    return (strcmp(cmd, "help") == 0 ||
            strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "pwd") == 0 ||
            strcmp(cmd, "cd") == 0 ||
            strcmp(cmd, "echo") == 0 ||
            strcmp(cmd, "clear") == 0 ||
            strcmp(cmd, "history") == 0 ||
            strcmp(cmd, "env") == 0 ||
            strcmp(cmd, "export") == 0 ||
            strcmp(cmd, "unset") == 0 ||
            strcmp(cmd, "type") == 0);
}

bool is_state_changing_builtin(const char *cmd) {
    if (!cmd) return false;
    return (strcmp(cmd, "cd") == 0 ||
            strcmp(cmd, "export") == 0 ||
            strcmp(cmd, "unset") == 0 ||
            strcmp(cmd, "exit") == 0);
}

int builtin_help(int argc, char **argv, shell_context_t *ctx) {
    (void)argc;
    (void)argv;
    (void)ctx;
    printf("MiniShell - Mini Operating System Shell in C\n");
    printf("Supported built-in commands:\n");
    printf("  help                 Display this help menu\n");
    printf("  exit [status]        Exit the shell with optional status code\n");
    printf("  pwd                  Print current working directory\n");
    printf("  cd [path]            Change current working directory\n");
    printf("  echo [args...]       Print arguments separated by space\n");
    printf("  clear                Clear the terminal screen\n");
    printf("  history              Display session command history\n");
    printf("  env                  List environment variables\n");
    printf("  export NAME=value    Set an environment variable\n");
    printf("  unset NAME           Remove an environment variable\n");
    printf("  type COMMAND         Identify how a command would be interpreted\n");
    printf("\nSupported operators:\n");
    printf("  command1 | command2  Connect output of command1 to input of command2\n");
    printf("  command < file       Redirect standard input from file\n");
    printf("  command > file       Redirect standard output to file (truncate)\n");
    printf("  command >> file      Redirect standard output to file (append)\n");
    printf("  command 2> file      Redirect standard error to file (truncate)\n");
    printf("  command 2>> file     Redirect standard error to file (append)\n");
    printf("  cmd1 ; cmd2          Sequential command execution\n");
    return 0;
}

int builtin_exit(int argc, char **argv, shell_context_t *ctx) {
    int status = ctx ? ctx->last_exit_status : 0;
    if (argc > 1) {
        char *endptr = NULL;
        long val = strtol(argv[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "minishell: exit: %s: numeric argument required\n", argv[1]);
            if (ctx) {
                ctx->exit_status = 255;
                ctx->should_exit = true;
            }
            return 255;
        }
        status = (int)(val & 0xFF);
    }
    if (ctx) {
        ctx->exit_status = status;
        ctx->should_exit = true;
    }
    return status;
}

int builtin_pwd(int argc, char **argv, shell_context_t *ctx) {
    (void)argc;
    (void)argv;
    char buf[1024];
    if (ctx && ctx->get_cwd) {
        if (ctx->get_cwd(buf, sizeof(buf), ctx->platform_data)) {
            printf("%s\n", buf);
            return 0;
        }
    } else {
        if (getcwd(buf, sizeof(buf)) != NULL) {
            printf("%s\n", buf);
            return 0;
        }
    }
    fprintf(stderr, "minishell: pwd: error retrieving current directory\n");
    return 1;
}

int builtin_cd(int argc, char **argv, shell_context_t *ctx) {
    const char *target = NULL;
    if (argc == 1) {
        if (ctx && ctx->get_env) {
            target = ctx->get_env("HOME", ctx->platform_data);
        } else {
            target = getenv("HOME");
        }
        if (!target || target[0] == '\0') {
            fprintf(stderr, "minishell: cd: HOME not set\n");
            return 1;
        }
    } else if (argc == 2) {
        target = argv[1];
    } else {
        fprintf(stderr, "minishell: cd: too many arguments\n");
        return 1;
    }

    if (ctx && ctx->set_cwd) {
        if (ctx->set_cwd(target, ctx->platform_data) != 0) {
            fprintf(stderr, "minishell: cd: %s: No such file or directory\n", target);
            return 1;
        }
    } else {
        if (chdir(target) != 0) {
            fprintf(stderr, "minishell: cd: %s: %s\n", target, strerror(errno));
            return 1;
        }
    }
    return 0;
}

int builtin_echo(int argc, char **argv, shell_context_t *ctx) {
    (void)ctx;
    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) {
            printf(" ");
        }
    }
    printf("\n");
    return 0;
}

int builtin_clear(int argc, char **argv, shell_context_t *ctx) {
    (void)argc;
    (void)argv;
    (void)ctx;
    /* Send ANSI escape code to clear screen and move cursor to home position */
    printf("\033[H\033[2J\033[3J");
    fflush(stdout);
    return 0;
}

int builtin_history(int argc, char **argv, shell_context_t *ctx) {
    (void)argc;
    (void)argv;
    if (ctx && ctx->history) {
        history_print(ctx->history);
    }
    return 0;
}

int builtin_env(int argc, char **argv, shell_context_t *ctx) {
    (void)argc;
    (void)argv;
    if (ctx && ctx->get_environ) {
        char **env_list = ctx->get_environ(ctx->platform_data);
        if (env_list) {
            for (size_t i = 0; env_list[i] != NULL; i++) {
                printf("%s\n", env_list[i]);
            }
        }
    } else {
#ifndef _WIN32
        for (char **env = environ; *env != NULL; env++) {
            printf("%s\n", *env);
        }
#endif
    }
    return 0;
}

static bool is_valid_identifier(const char *name) {
    if (!name || (!isalpha((unsigned char)name[0]) && name[0] != '_')) {
        return false;
    }
    for (size_t i = 1; name[i] != '\0'; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') {
            return false;
        }
    }
    return true;
}

int builtin_export(int argc, char **argv, shell_context_t *ctx) {
    if (argc == 1) {
        return builtin_env(argc, argv, ctx);
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        char *eq = strchr(arg, '=');
        char name[256];
        const char *val = "";

        if (eq) {
            size_t name_len = (size_t)(eq - arg);
            if (name_len >= sizeof(name)) {
                fprintf(stderr, "minishell: export: '%s': identifier too long\n", arg);
                status = 1;
                continue;
            }
            memcpy(name, arg, name_len);
            name[name_len] = '\0';
            val = eq + 1;
        } else {
            if (strlen(arg) >= sizeof(name)) {
                fprintf(stderr, "minishell: export: '%s': identifier too long\n", arg);
                status = 1;
                continue;
            }
            strncpy(name, arg, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            val = "";
        }

        if (!is_valid_identifier(name)) {
            fprintf(stderr, "minishell: export: '%s': not a valid identifier\n", arg);
            status = 1;
            continue;
        }

        if (ctx && ctx->set_env) {
            if (ctx->set_env(name, val, ctx->platform_data) != 0) {
                status = 1;
            }
        } else {
#ifndef _WIN32
            if (setenv(name, val, 1) != 0) {
                fprintf(stderr, "minishell: export: %s: %s\n", name, strerror(errno));
                status = 1;
            }
#endif
        }
    }
    return status;
}

int builtin_unset(int argc, char **argv, shell_context_t *ctx) {
    if (argc == 1) {
        return 0;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        const char *name = argv[i];
        if (!is_valid_identifier(name)) {
            fprintf(stderr, "minishell: unset: '%s': not a valid identifier\n", name);
            status = 1;
            continue;
        }

        if (ctx && ctx->unset_env) {
            if (ctx->unset_env(name, ctx->platform_data) != 0) {
                status = 1;
            }
        } else {
#ifndef _WIN32
            if (unsetenv(name) != 0) {
                fprintf(stderr, "minishell: unset: %s: %s\n", name, strerror(errno));
                status = 1;
            }
#endif
        }
    }
    return status;
}

int builtin_type(int argc, char **argv, shell_context_t *ctx) {
    (void)ctx;
    if (argc < 2) {
        fprintf(stderr, "minishell: type: missing argument\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        const char *cmd = argv[i];
        if (is_builtin(cmd)) {
            printf("%s is a shell builtin\n", cmd);
            continue;
        }

        /* If command contains a slash, check if it exists directly */
        if (strchr(cmd, '/')) {
            if (access(cmd, X_OK) == 0) {
                printf("%s is %s\n", cmd, cmd);
            } else {
                fprintf(stderr, "minishell: type: %s: not found\n", cmd);
                status = 1;
            }
            continue;
        }

        /* Search PATH */
        const char *path_env = getenv("PATH");
        if (!path_env) {
            fprintf(stderr, "minishell: type: %s: not found\n", cmd);
            status = 1;
            continue;
        }

        char *path_copy = strdup(path_env);
        if (!path_copy) {
            fprintf(stderr, "minishell: type: out of memory\n");
            status = 1;
            continue;
        }

        bool found = false;
        char *token = strtok(path_copy, ":");
        while (token != NULL) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", token, cmd);
            if (access(full_path, X_OK) == 0) {
                printf("%s is %s\n", cmd, full_path);
                found = true;
                break;
            }
            token = strtok(NULL, ":");
        }
        free(path_copy);

        if (!found) {
            fprintf(stderr, "minishell: type: %s: not found\n", cmd);
            status = 1;
        }
    }
    return status;
}

int execute_builtin(int argc, char **argv, shell_context_t *ctx) {
    if (argc == 0 || !argv || !argv[0]) return 0;
    const char *cmd = argv[0];

    if (strcmp(cmd, "help") == 0) return builtin_help(argc, argv, ctx);
    if (strcmp(cmd, "exit") == 0) return builtin_exit(argc, argv, ctx);
    if (strcmp(cmd, "pwd") == 0) return builtin_pwd(argc, argv, ctx);
    if (strcmp(cmd, "cd") == 0) return builtin_cd(argc, argv, ctx);
    if (strcmp(cmd, "echo") == 0) return builtin_echo(argc, argv, ctx);
    if (strcmp(cmd, "clear") == 0) return builtin_clear(argc, argv, ctx);
    if (strcmp(cmd, "history") == 0) return builtin_history(argc, argv, ctx);
    if (strcmp(cmd, "env") == 0) return builtin_env(argc, argv, ctx);
    if (strcmp(cmd, "export") == 0) return builtin_export(argc, argv, ctx);
    if (strcmp(cmd, "unset") == 0) return builtin_unset(argc, argv, ctx);
    if (strcmp(cmd, "type") == 0) return builtin_type(argc, argv, ctx);

    return 127;
}

