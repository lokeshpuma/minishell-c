#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/wasm/virtual_fs.h"
#include "../src/wasm/executor_virtual.h"

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

static void test_vfs_basic(void) {
    virtual_fs_t *vfs = vfs_create();
    vfs_init_sample_data(vfs);

    TEST_ASSERT(strcmp(vfs_getcwd(vfs), "/home/user") == 0, "default cwd is /home/user");

    char *buf = NULL;
    size_t sz = 0;
    int ret = vfs_read_file(vfs, "welcome.txt", &buf, &sz);
    TEST_ASSERT(ret == 0 && buf != NULL, "read welcome.txt succeeds");
    TEST_ASSERT(strstr(buf, "Welcome to MiniShell") != NULL, "welcome.txt content matches");
    if (buf) free(buf);

    ret = vfs_write_file(vfs, "test_file.txt", "hello vfs", 9, false);
    TEST_ASSERT(ret == 0, "write test_file.txt succeeds");

    buf = NULL;
    sz = 0;
    ret = vfs_read_file(vfs, "test_file.txt", &buf, &sz);
    TEST_ASSERT(ret == 0 && sz == 9 && strcmp(buf, "hello vfs") == 0, "test_file.txt verified");
    if (buf) free(buf);

    vfs_destroy(vfs);
}

static void test_virtual_commands(void) {
    virtual_shell_t *sh = virtual_shell_create();
    vstream_t out, err;

    vstream_init(&out);
    vstream_init(&err);
    int ret = virtual_shell_execute(sh, "pwd", &out, &err);
    TEST_ASSERT(ret == 0, "pwd succeeds");
    TEST_ASSERT(strstr(out.data, "/home/user") != NULL, "pwd prints /home/user");
    vstream_free(&out);
    vstream_free(&err);

    vstream_init(&out);
    vstream_init(&err);
    ret = virtual_shell_execute(sh, "echo hello virtual world", &out, &err);
    TEST_ASSERT(ret == 0, "echo succeeds");
    TEST_ASSERT(strstr(out.data, "hello virtual world\n") != NULL, "echo output matches");
    vstream_free(&out);
    vstream_free(&err);

    /* Test redirection */
    vstream_init(&out);
    vstream_init(&err);
    ret = virtual_shell_execute(sh, "echo foo > generated.txt ; cat < generated.txt", &out, &err);
    TEST_ASSERT(ret == 0, "redirect > and < succeeds");
    TEST_ASSERT(strstr(out.data, "foo\n") != NULL, "cat output contains foo");
    vstream_free(&out);
    vstream_free(&err);

    /* Test pipeline */
    vstream_init(&out);
    vstream_init(&err);
    ret = virtual_shell_execute(sh, "cat data/fruits.txt | sort | uniq", &out, &err);
    TEST_ASSERT(ret == 0, "cat | sort | uniq succeeds");
    TEST_ASSERT(strstr(out.data, "apple\n") != NULL, "fruits pipeline contains apple");
    TEST_ASSERT(strstr(out.data, "banana\n") != NULL, "fruits pipeline contains banana");
    vstream_free(&out);
    vstream_free(&err);

    /* Test unknown command allowlist rejection (PRD 4.8) */
    vstream_init(&out);
    vstream_init(&err);
    ret = virtual_shell_execute(sh, "rmdir /", &out, &err);
    TEST_ASSERT(ret == 127, "unknown command returns 127");
    TEST_ASSERT(strstr(err.data, "command not available in browser demo") != NULL, "safe error message");
    vstream_free(&out);
    vstream_free(&err);

    virtual_shell_destroy(sh);
}

int main(void) {
    printf("=== Running Virtual Shell & VFS Unit Tests ===\n");
    test_vfs_basic();
    test_virtual_commands();

    printf("Virtual Shell Tests: %d/%d passed\n", g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}

