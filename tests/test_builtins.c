#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "../src/common/builtins.h"
#include "../src/common/history.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
    } else { \
        fprintf(stderr, "FAIL: %s (line %d): %s\n", __func__, __LINE__, msg); \
    } \
} while (0)

static void test_builtin_identification(void) {
    TEST_ASSERT(is_builtin("help"), "help is builtin");
    TEST_ASSERT(is_builtin("exit"), "exit is builtin");
    TEST_ASSERT(is_builtin("pwd"), "pwd is builtin");
    TEST_ASSERT(is_builtin("cd"), "cd is builtin");
    TEST_ASSERT(is_builtin("echo"), "echo is builtin");
    TEST_ASSERT(is_builtin("clear"), "clear is builtin");
    TEST_ASSERT(is_builtin("history"), "history is builtin");
    TEST_ASSERT(is_builtin("env"), "env is builtin");
    TEST_ASSERT(is_builtin("export"), "export is builtin");
    TEST_ASSERT(is_builtin("unset"), "unset is builtin");
    TEST_ASSERT(is_builtin("type"), "type is builtin");
    TEST_ASSERT(!is_builtin("ls"), "ls is not builtin");
    TEST_ASSERT(!is_builtin("grep"), "grep is not builtin");

    TEST_ASSERT(is_state_changing_builtin("cd"), "cd is state changing");
    TEST_ASSERT(is_state_changing_builtin("export"), "export is state changing");
    TEST_ASSERT(is_state_changing_builtin("unset"), "unset is state changing");
    TEST_ASSERT(is_state_changing_builtin("exit"), "exit is state changing");
    TEST_ASSERT(!is_state_changing_builtin("echo"), "echo is not state changing");
    TEST_ASSERT(!is_state_changing_builtin("pwd"), "pwd is not state changing");
}

static void test_export_and_unset(void) {
    shell_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    char *export_argv[] = {"export", "MY_TEST_KEY=12345", NULL};
    int ret = builtin_export(2, export_argv, &ctx);
    TEST_ASSERT(ret == 0, "export succeeds");
    const char *val = getenv("MY_TEST_KEY");
    TEST_ASSERT(val != NULL && strcmp(val, "12345") == 0, "getenv reflects export");

    char *unset_argv[] = {"unset", "MY_TEST_KEY", NULL};
    ret = builtin_unset(2, unset_argv, &ctx);
    TEST_ASSERT(ret == 0, "unset succeeds");
    val = getenv("MY_TEST_KEY");
    TEST_ASSERT(val == NULL, "getenv reflects unset");

    /* Invalid identifier */
    char *bad_export[] = {"export", "123BAD=val", NULL};
    ret = builtin_export(2, bad_export, &ctx);
    TEST_ASSERT(ret != 0, "invalid identifier in export fails");
}

static void test_exit_command(void) {
    shell_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.last_exit_status = 5;

    char *exit_argv[] = {"exit", "42", NULL};
    int ret = builtin_exit(2, exit_argv, &ctx);
    TEST_ASSERT(ret == 42, "exit returns status 42");
    TEST_ASSERT(ctx.should_exit == true, "ctx should_exit set");
    TEST_ASSERT(ctx.exit_status == 42, "ctx exit_status 42");

    memset(&ctx, 0, sizeof(ctx));
    ctx.last_exit_status = 7;
    char *exit_default[] = {"exit", NULL};
    ret = builtin_exit(1, exit_default, &ctx);
    TEST_ASSERT(ret == 7, "exit defaults to last_exit_status");
    TEST_ASSERT(ctx.should_exit == true, "ctx should_exit set");
    TEST_ASSERT(ctx.exit_status == 7, "ctx exit_status matches last");
}

static void test_cd_command(void) {
    shell_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    char original_cwd[1024];
    getcwd(original_cwd, sizeof(original_cwd));

    char *cd_argv[] = {"cd", "/tmp", NULL};
    int ret = builtin_cd(2, cd_argv, &ctx);
    TEST_ASSERT(ret == 0, "cd /tmp succeeds");

    char new_cwd[1024];
    getcwd(new_cwd, sizeof(new_cwd));
    TEST_ASSERT(strstr(new_cwd, "tmp") != NULL, "cwd changed to tmp");

    /* Restore cwd */
    char *restore_argv[] = {"cd", original_cwd, NULL};
    builtin_cd(2, restore_argv, &ctx);
}

int main(void) {
    printf("=== Running Builtins Unit Tests ===\n");
    test_builtin_identification();
    test_export_and_unset();
    test_exit_command();
    test_cd_command();

    printf("Builtins Tests: %d/%d passed\n", g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}

