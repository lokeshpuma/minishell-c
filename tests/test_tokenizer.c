#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/common/tokenizer.h"

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

static void test_simple_words(void) {
    token_list_t *list = token_list_create();
    char err[256] = {0};
    int ret = tokenize("ls -l /tmp", NULL, list, err, sizeof(err));
    TEST_ASSERT(ret == 0, "simple words tokenization succeeds");
    TEST_ASSERT(list->count == 3, "token count is 3");
    TEST_ASSERT(strcmp(list->tokens[0].value, "ls") == 0, "token 0 is ls");
    TEST_ASSERT(strcmp(list->tokens[1].value, "-l") == 0, "token 1 is -l");
    TEST_ASSERT(strcmp(list->tokens[2].value, "/tmp") == 0, "token 2 is /tmp");
    token_list_free(list);
}

static void test_repeated_whitespace(void) {
    token_list_t *list = token_list_create();
    char err[256] = {0};
    int ret = tokenize("   echo   \t  hello    world   \n", NULL, list, err, sizeof(err));
    TEST_ASSERT(ret == 0, "repeated whitespace succeeds");
    TEST_ASSERT(list->count == 3, "token count is 3");
    TEST_ASSERT(strcmp(list->tokens[0].value, "echo") == 0, "token 0 is echo");
    TEST_ASSERT(strcmp(list->tokens[1].value, "hello") == 0, "token 1 is hello");
    TEST_ASSERT(strcmp(list->tokens[2].value, "world") == 0, "token 2 is world");
    token_list_free(list);
}

static void test_single_and_double_quotes(void) {
    token_list_t *list = token_list_create();
    char err[256] = {0};
    int ret = tokenize("echo 'hello world' \"foo bar\"", NULL, list, err, sizeof(err));
    TEST_ASSERT(ret == 0, "quoted strings succeed");
    TEST_ASSERT(list->count == 3, "token count is 3");
    TEST_ASSERT(strcmp(list->tokens[1].value, "hello world") == 0, "single quote preserves spaces");
    TEST_ASSERT(strcmp(list->tokens[2].value, "foo bar") == 0, "double quote preserves spaces");
    token_list_free(list);
}

static void test_escaped_characters(void) {
    token_list_t *list = token_list_create();
    char err[256] = {0};
    int ret = tokenize("echo hello\\ world \\| \\> file", NULL, list, err, sizeof(err));
    TEST_ASSERT(ret == 0, "escaped chars succeed");
    TEST_ASSERT(list->count == 5, "token count is 5");
    TEST_ASSERT(strcmp(list->tokens[1].value, "hello world") == 0, "escaped space in word");
    TEST_ASSERT(strcmp(list->tokens[2].value, "|") == 0, "escaped pipe is a word");
    TEST_ASSERT(strcmp(list->tokens[3].value, ">") == 0, "escaped redirect is a word");
    token_list_free(list);
}

static void test_operators(void) {
    token_list_t *list = token_list_create();
    char err[256] = {0};
    int ret = tokenize("cmd1 | cmd2 < in.txt > out.txt >> append.txt 2> err.txt 2>> err_app.txt ; cmd3",
                       NULL, list, err, sizeof(err));
    TEST_ASSERT(ret == 0, "operators tokenization succeeds");
    TEST_ASSERT(list->tokens[1].type == TOKEN_PIPE, "pipe operator");
    TEST_ASSERT(list->tokens[3].type == TOKEN_REDIRECT_IN, "input redirect operator");
    TEST_ASSERT(list->tokens[5].type == TOKEN_REDIRECT_OUT, "output redirect operator");
    TEST_ASSERT(list->tokens[7].type == TOKEN_REDIRECT_APPEND, "append redirect operator");
    TEST_ASSERT(list->tokens[9].type == TOKEN_REDIRECT_ERR, "stderr redirect operator");
    TEST_ASSERT(list->tokens[11].type == TOKEN_REDIRECT_ERR_APPEND, "stderr append redirect operator");
    TEST_ASSERT(list->tokens[13].type == TOKEN_SEMICOLON, "semicolon operator");
    token_list_free(list);
}

static void test_unterminated_quotes(void) {
    token_list_t *list = token_list_create();
    char err[256] = {0};
    int ret = tokenize("echo 'unterminated", NULL, list, err, sizeof(err));
    TEST_ASSERT(ret != 0, "unterminated single quote fails");
    TEST_ASSERT(strstr(err, "unterminated single quote") != NULL, "error message specifies single quote");

    memset(err, 0, sizeof(err));
    ret = tokenize("echo \"unterminated", NULL, list, err, sizeof(err));
    TEST_ASSERT(ret != 0, "unterminated double quote fails");
    TEST_ASSERT(strstr(err, "unterminated double quote") != NULL, "error message specifies double quote");

    token_list_free(list);
}

static void test_empty_input(void) {
    token_list_t *list = token_list_create();
    char err[256] = {0};
    int ret = tokenize("", NULL, list, err, sizeof(err));
    TEST_ASSERT(ret == 0, "empty input succeeds");
    TEST_ASSERT(list->count == 0, "empty input produces 0 tokens");

    ret = tokenize("   \t  \n  ", NULL, list, err, sizeof(err));
    TEST_ASSERT(ret == 0, "whitespace input succeeds");
    TEST_ASSERT(list->count == 0, "whitespace input produces 0 tokens");
    token_list_free(list);
}

static void test_variable_expansion(void) {
    expand_context_t ctx;
    ctx.last_exit_status = 42;
    ctx.get_env_var = NULL;
    ctx.user_data = NULL;

    setenv("TEST_VAR", "expanded_val", 1);

    token_list_t *list = token_list_create();
    char err[256] = {0};
    int ret = tokenize("echo $TEST_VAR ${TEST_VAR} $? '$TEST_VAR'", &ctx, list, err, sizeof(err));
    TEST_ASSERT(ret == 0, "variable expansion succeeds");
    TEST_ASSERT(list->count == 5, "token count is 5");
    TEST_ASSERT(strcmp(list->tokens[1].value, "expanded_val") == 0, "$TEST_VAR expanded");
    TEST_ASSERT(strcmp(list->tokens[2].value, "expanded_val") == 0, "${TEST_VAR} expanded");
    TEST_ASSERT(strcmp(list->tokens[3].value, "42") == 0, "$? expanded to 42");
    TEST_ASSERT(strcmp(list->tokens[4].value, "$TEST_VAR") == 0, "single quote prevents expansion");

    token_list_free(list);
}

static void test_limits(void) {
    /* Exceed max line length */
    char long_line[MINISHELL_MAX_LINE_LEN + 10];
    memset(long_line, 'a', sizeof(long_line) - 1);
    long_line[sizeof(long_line) - 1] = '\0';

    token_list_t *list = token_list_create();
    char err[256] = {0};
    int ret = tokenize(long_line, NULL, list, err, sizeof(err));
    TEST_ASSERT(ret != 0, "exceeding max line length fails");
    TEST_ASSERT(strstr(err, "exceeds maximum length") != NULL, "error message specifies line length");
    token_list_free(list);
}

int main(void) {
    printf("=== Running Tokenizer Unit Tests ===\n");
    test_simple_words();
    test_repeated_whitespace();
    test_single_and_double_quotes();
    test_escaped_characters();
    test_operators();
    test_unterminated_quotes();
    test_empty_input();
    test_variable_expansion();
    test_limits();

    printf("Tokenizer Tests: %d/%d passed\n", g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}

