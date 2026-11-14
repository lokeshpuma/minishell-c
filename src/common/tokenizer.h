#ifndef MINISHELL_TOKENIZER_H
#define MINISHELL_TOKENIZER_H

#include <stddef.h>
#include <stdbool.h>

#define MINISHELL_MAX_LINE_LEN 4096
#define MINISHELL_MAX_TOKENS   512

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,                /* | */
    TOKEN_REDIRECT_IN,         /* < */
    TOKEN_REDIRECT_OUT,        /* > */
    TOKEN_REDIRECT_APPEND,     /* >> */
    TOKEN_REDIRECT_ERR,        /* 2> */
    TOKEN_REDIRECT_ERR_APPEND, /* 2>> */
    TOKEN_SEMICOLON,           /* ; */
    TOKEN_EOF
} token_type_t;

typedef struct {
    token_type_t type;
    char *value; /* Dynamically allocated for TOKEN_WORD, NULL for operators */
} token_t;

typedef struct {
    token_t *tokens;
    size_t count;
    size_t capacity;
} token_list_t;

typedef struct {
    int last_exit_status;
    const char *(*get_env_var)(const char *name, void *user_data);
    void *user_data;
} expand_context_t;

/* Tokenizer functions */
token_list_t *token_list_create(void);
void token_list_free(token_list_t *list);

/*
 * Tokenizes the input command line.
 * If ctx is non-NULL, variables ($VAR, ${VAR}, $?) are expanded.
 * If err_buf is non-NULL and an error occurs, an error message is written into err_buf.
 * Returns 0 on success, non-zero on failure.
 */
int tokenize(const char *input, const expand_context_t *ctx,
             token_list_t *out_list, char *err_buf, size_t err_buf_size);

const char *token_type_to_string(token_type_t type);

#endif /* MINISHELL_TOKENIZER_H */

