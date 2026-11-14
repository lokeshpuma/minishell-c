#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/common/tokenizer.h"
#include "../src/common/parser.h"

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

static command_line_t *tokenize_and_parse(const char *input, char *err_buf, size_t err_size) {
    token_list_t *tokens = token_list_create();
    if (!tokens) return NULL;
    int ret = tokenize(input, NULL, tokens, err_buf, err_size);
    if (ret != 0) {
        token_list_free(tokens);
        return NULL;
    }
    command_line_t *cmd_line = parse(tokens, err_buf, err_size);
    token_list_free(tokens);
    return cmd_line;
}

static void test_simple_command(void) {
    char err[256] = {0};
    command_line_t *cl = tokenize_and_parse("ls -la /tmp", err, sizeof(err));
    TEST_ASSERT(cl != NULL, "simple command parses");
    TEST_ASSERT(cl->count == 1, "1 pipeline");
    TEST_ASSERT(cl->pipelines[0].count == 1, "1 command in pipeline");
    command_t *cmd = &cl->pipelines[0].commands[0];
    TEST_ASSERT(cmd->argc == 3, "argc is 3");
    TEST_ASSERT(strcmp(cmd->argv[0], "ls") == 0, "argv[0] is ls");
    TEST_ASSERT(strcmp(cmd->argv[1], "-la") == 0, "argv[1] is -la");
    TEST_ASSERT(strcmp(cmd->argv[2], "/tmp") == 0, "argv[2] is /tmp");
    command_line_free(cl);
}

static void test_pipeline(void) {
    char err[256] = {0};
    command_line_t *cl = tokenize_and_parse("ps aux | grep ssh | wc -l", err, sizeof(err));
    TEST_ASSERT(cl != NULL, "pipeline parses");
    TEST_ASSERT(cl->count == 1, "1 pipeline");
    TEST_ASSERT(cl->pipelines[0].count == 3, "3 commands in pipeline");
    TEST_ASSERT(strcmp(cl->pipelines[0].commands[0].argv[0], "ps") == 0, "first cmd ps");
    TEST_ASSERT(strcmp(cl->pipelines[0].commands[1].argv[0], "grep") == 0, "second cmd grep");
    TEST_ASSERT(strcmp(cl->pipelines[0].commands[2].argv[0], "wc") == 0, "third cmd wc");
    command_line_free(cl);
}

static void test_redirections(void) {
    char err[256] = {0};
    command_line_t *cl = tokenize_and_parse("cat < input.txt | grep foo > out.txt 2>> err.log", err, sizeof(err));
    TEST_ASSERT(cl != NULL, "redirections parse");
    TEST_ASSERT(cl->pipelines[0].count == 2, "2 commands in pipeline");

    command_t *cmd1 = &cl->pipelines[0].commands[0];
    TEST_ASSERT(cmd1->stdin_file != NULL && strcmp(cmd1->stdin_file, "input.txt") == 0, "stdin redirect");

    command_t *cmd2 = &cl->pipelines[0].commands[1];
    TEST_ASSERT(cmd2->stdout_file != NULL && strcmp(cmd2->stdout_file, "out.txt") == 0, "stdout redirect");
    TEST_ASSERT(cmd2->stdout_append == 0, "stdout truncate");
    TEST_ASSERT(cmd2->stderr_file != NULL && strcmp(cmd2->stderr_file, "err.log") == 0, "stderr redirect");
    TEST_ASSERT(cmd2->stderr_append == 1, "stderr append");

    command_line_free(cl);
}

static void test_semicolon_separation(void) {
    char err[256] = {0};
    command_line_t *cl = tokenize_and_parse("echo one ; echo two ; echo three", err, sizeof(err));
    TEST_ASSERT(cl != NULL, "semicolon separation parses");
    TEST_ASSERT(cl->count == 3, "3 pipelines");
    TEST_ASSERT(strcmp(cl->pipelines[0].commands[0].argv[1], "one") == 0, "pipeline 1 echo one");
    TEST_ASSERT(strcmp(cl->pipelines[1].commands[0].argv[1], "two") == 0, "pipeline 2 echo two");
    TEST_ASSERT(strcmp(cl->pipelines[2].commands[0].argv[1], "three") == 0, "pipeline 3 echo three");
    command_line_free(cl);
}

static void test_missing_redirection_target(void) {
    char err[256] = {0};
    command_line_t *cl = tokenize_and_parse("ls >", err, sizeof(err));
    TEST_ASSERT(cl == NULL, "missing redirect target fails");
    TEST_ASSERT(strstr(err, "syntax error near unexpected token") != NULL, "syntax error reported");

    memset(err, 0, sizeof(err));
    cl = tokenize_and_parse("cat < | grep foo", err, sizeof(err));
    TEST_ASSERT(cl == NULL, "pipe as redirect target fails");
    TEST_ASSERT(strstr(err, "syntax error near unexpected token") != NULL, "syntax error reported");
}

static void test_empty_pipeline_stage(void) {
    char err[256] = {0};
    command_line_t *cl = tokenize_and_parse("| ls", err, sizeof(err));
    TEST_ASSERT(cl == NULL, "leading pipe fails");

    memset(err, 0, sizeof(err));
    cl = tokenize_and_parse("ls |", err, sizeof(err));
    TEST_ASSERT(cl == NULL, "trailing pipe fails");

    memset(err, 0, sizeof(err));
    cl = tokenize_and_parse("ls || grep", err, sizeof(err));
    TEST_ASSERT(cl == NULL, "double pipe fails");
}

int main(void) {
    printf("=== Running Parser Unit Tests ===\n");
    test_simple_command();
    test_pipeline();
    test_redirections();
    test_semicolon_separation();
    test_missing_redirection_target();
    test_empty_pipeline_stage();

    printf("Parser Tests: %d/%d passed\n", g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}

