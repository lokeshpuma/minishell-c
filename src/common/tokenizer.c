#define _POSIX_C_SOURCE 200809L
#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} str_builder_t;

static void sb_init(str_builder_t *sb) {
    sb->cap = 64;
    sb->len = 0;
    sb->data = (char *)malloc(sb->cap);
    if (sb->data) {
        sb->data[0] = '\0';
    }
}

static void sb_append_char(str_builder_t *sb, char c) {
    if (!sb->data) return;
    if (sb->len + 2 > sb->cap) {
        sb->cap *= 2;
        char *new_data = (char *)realloc(sb->data, sb->cap);
        if (!new_data) {
            free(sb->data);
            sb->data = NULL;
            return;
        }
        sb->data = new_data;
    }
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

static void sb_append_str(str_builder_t *sb, const char *str) {
    if (!str || !sb->data) return;
    size_t slen = strlen(str);
    if (sb->len + slen + 1 > sb->cap) {
        while (sb->len + slen + 1 > sb->cap) {
            sb->cap *= 2;
        }
        char *new_data = (char *)realloc(sb->data, sb->cap);
        if (!new_data) {
            free(sb->data);
            sb->data = NULL;
            return;
        }
        sb->data = new_data;
    }
    memcpy(sb->data + sb->len, str, slen);
    sb->len += slen;
    sb->data[sb->len] = '\0';
}

static void sb_free(str_builder_t *sb) {
    if (sb->data) {
        free(sb->data);
        sb->data = NULL;
    }
    sb->len = 0;
    sb->cap = 0;
}

token_list_t *token_list_create(void) {
    token_list_t *list = (token_list_t *)malloc(sizeof(token_list_t));
    if (!list) return NULL;
    list->capacity = 16;
    list->count = 0;
    list->tokens = (token_t *)malloc(sizeof(token_t) * list->capacity);
    if (!list->tokens) {
        free(list);
        return NULL;
    }
    return list;
}

void token_list_free(token_list_t *list) {
    if (!list) return;
    if (list->tokens) {
        for (size_t i = 0; i < list->count; i++) {
            if (list->tokens[i].value) {
                free(list->tokens[i].value);
            }
        }
        free(list->tokens);
    }
    free(list);
}

static int token_list_add(token_list_t *list, token_type_t type, const char *value) {
    if (list->count >= MINISHELL_MAX_TOKENS) {
        return -1;
    }
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity * 2;
        token_t *new_tokens = (token_t *)realloc(list->tokens, sizeof(token_t) * new_cap);
        if (!new_tokens) return -1;
        list->tokens = new_tokens;
        list->capacity = new_cap;
    }
    list->tokens[list->count].type = type;
    if (value) {
        list->tokens[list->count].value = strdup(value);
        if (!list->tokens[list->count].value) return -1;
    } else {
        list->tokens[list->count].value = NULL;
    }
    list->count++;
    return 0;
}

const char *token_type_to_string(token_type_t type) {
    switch (type) {
        case TOKEN_WORD:                return "WORD";
        case TOKEN_PIPE:                return "|";
        case TOKEN_REDIRECT_IN:         return "<";
        case TOKEN_REDIRECT_OUT:        return ">";
        case TOKEN_REDIRECT_APPEND:     return ">>";
        case TOKEN_REDIRECT_ERR:        return "2>";
        case TOKEN_REDIRECT_ERR_APPEND: return "2>>";
        case TOKEN_SEMICOLON:           return ";";
        case TOKEN_EOF:                 return "EOF";
        default:                        return "UNKNOWN";
    }
}

static void handle_var_expansion(const char *input, size_t *idx, const expand_context_t *ctx, str_builder_t *sb) {
    (*idx)++; /* Skip '$' */
    if (input[*idx] == '?') {
        (*idx)++;
        if (ctx) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", ctx->last_exit_status);
            sb_append_str(sb, buf);
        } else {
            sb_append_str(sb, "0");
        }
        return;
    }

    if (input[*idx] == '{') {
        (*idx)++; /* Skip '{' */
        size_t start = *idx;
        while (input[*idx] != '\0' && input[*idx] != '}') {
            (*idx)++;
        }
        if (input[*idx] == '}') {
            size_t var_len = *idx - start;
            char var_name[256];
            if (var_len < sizeof(var_name)) {
                memcpy(var_name, input + start, var_len);
                var_name[var_len] = '\0';
                const char *val = NULL;
                if (ctx && ctx->get_env_var) {
                    val = ctx->get_env_var(var_name, ctx->user_data);
                } else {
                    val = getenv(var_name);
                }
                if (val) {
                    sb_append_str(sb, val);
                }
            }
            (*idx)++; /* Skip '}' */
        }
        return;
    }

    if (isalpha((unsigned char)input[*idx]) || input[*idx] == '_') {
        size_t start = *idx;
        while (isalnum((unsigned char)input[*idx]) || input[*idx] == '_') {
            (*idx)++;
        }
        size_t var_len = *idx - start;
        char var_name[256];
        if (var_len < sizeof(var_name)) {
            memcpy(var_name, input + start, var_len);
            var_name[var_len] = '\0';
            const char *val = NULL;
            if (ctx && ctx->get_env_var) {
                val = ctx->get_env_var(var_name, ctx->user_data);
            } else {
                val = getenv(var_name);
            }
            if (val) {
                sb_append_str(sb, val);
            }
        }
        return;
    }

    /* Lone '$' or followed by punctuation/digit */
    sb_append_char(sb, '$');
}

int tokenize(const char *input, const expand_context_t *ctx,
             token_list_t *out_list, char *err_buf, size_t err_buf_size) {
    if (!input || !out_list) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "invalid arguments to tokenizer");
        }
        return -1;
    }

    size_t input_len = strlen(input);
    if (input_len > MINISHELL_MAX_LINE_LEN) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "minishell: command line exceeds maximum length of %d", MINISHELL_MAX_LINE_LEN);
        }
        return -1;
    }

    size_t i = 0;
    while (i < input_len) {
        /* Skip leading whitespace */
        while (i < input_len && (input[i] == ' ' || input[i] == '\t' || input[i] == '\r' || input[i] == '\n')) {
            i++;
        }
        if (i >= input_len) {
            break;
        }

        /* Check for 2>> or 2> */
        if (input[i] == '2' && input[i + 1] == '>') {
            if (input[i + 2] == '>') {
                if (token_list_add(out_list, TOKEN_REDIRECT_ERR_APPEND, NULL) != 0) {
                    if (err_buf && err_buf_size > 0) {
                        snprintf(err_buf, err_buf_size, "minishell: maximum token limit exceeded");
                    }
                    return -1;
                }
                i += 3;
                continue;
            } else {
                if (token_list_add(out_list, TOKEN_REDIRECT_ERR, NULL) != 0) {
                    if (err_buf && err_buf_size > 0) {
                        snprintf(err_buf, err_buf_size, "minishell: maximum token limit exceeded");
                    }
                    return -1;
                }
                i += 2;
                continue;
            }
        }

        /* Check for >> or > */
        if (input[i] == '>') {
            if (input[i + 1] == '>') {
                if (token_list_add(out_list, TOKEN_REDIRECT_APPEND, NULL) != 0) {
                    if (err_buf && err_buf_size > 0) {
                        snprintf(err_buf, err_buf_size, "minishell: maximum token limit exceeded");
                    }
                    return -1;
                }
                i += 2;
            } else {
                if (token_list_add(out_list, TOKEN_REDIRECT_OUT, NULL) != 0) {
                    if (err_buf && err_buf_size > 0) {
                        snprintf(err_buf, err_buf_size, "minishell: maximum token limit exceeded");
                    }
                    return -1;
                }
                i++;
            }
            continue;
        }

        /* Check for < */
        if (input[i] == '<') {
            if (token_list_add(out_list, TOKEN_REDIRECT_IN, NULL) != 0) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "minishell: maximum token limit exceeded");
                }
                return -1;
            }
            i++;
            continue;
        }

        /* Check for | */
        if (input[i] == '|') {
            if (token_list_add(out_list, TOKEN_PIPE, NULL) != 0) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "minishell: maximum token limit exceeded");
                }
                return -1;
            }
            i++;
            continue;
        }

        /* Check for ; */
        if (input[i] == ';') {
            if (token_list_add(out_list, TOKEN_SEMICOLON, NULL) != 0) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "minishell: maximum token limit exceeded");
                }
                return -1;
            }
            i++;
            continue;
        }

        /* Parse a WORD (can contain quotes and escaped characters) */
        str_builder_t sb;
        sb_init(&sb);
        bool has_chars = false;

        while (i < input_len) {
            char c = input[i];

            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                break;
            }
            if (c == '|' || c == '<' || c == '>' || c == ';') {
                break;
            }
            if (c == '2' && input[i + 1] == '>') {
                break;
            }

            if (c == '\'') {
                /* Single quote: preserve everything literally until next single quote */
                i++;
                has_chars = true;
                bool terminated = false;
                while (i < input_len) {
                    if (input[i] == '\'') {
                        terminated = true;
                        i++;
                        break;
                    }
                    sb_append_char(&sb, input[i]);
                    i++;
                }
                if (!terminated) {
                    sb_free(&sb);
                    if (err_buf && err_buf_size > 0) {
                        snprintf(err_buf, err_buf_size, "minishell: syntax error: unterminated single quote");
                    }
                    return -1;
                }
            } else if (c == '"') {
                /* Double quote: preserve spaces, handle backslash and variable expansion */
                i++;
                has_chars = true;
                bool terminated = false;
                while (i < input_len) {
                    if (input[i] == '"') {
                        terminated = true;
                        i++;
                        break;
                    } else if (input[i] == '\\') {
                        i++;
                        if (i < input_len) {
                            char escaped = input[i];
                            if (escaped == '"' || escaped == '\\' || escaped == '$' || escaped == '`') {
                                sb_append_char(&sb, escaped);
                            } else {
                                sb_append_char(&sb, '\\');
                                sb_append_char(&sb, escaped);
                            }
                            i++;
                        } else {
                            sb_append_char(&sb, '\\');
                        }
                    } else if (input[i] == '$') {
                        handle_var_expansion(input, &i, ctx, &sb);
                    } else {
                        sb_append_char(&sb, input[i]);
                        i++;
                    }
                }
                if (!terminated) {
                    sb_free(&sb);
                    if (err_buf && err_buf_size > 0) {
                        snprintf(err_buf, err_buf_size, "minishell: syntax error: unterminated double quote");
                    }
                    return -1;
                }
            } else if (c == '\\') {
                /* Backslash escape outside quotes */
                i++;
                has_chars = true;
                if (i < input_len) {
                    sb_append_char(&sb, input[i]);
                    i++;
                }
            } else if (c == '$') {
                has_chars = true;
                handle_var_expansion(input, &i, ctx, &sb);
            } else {
                has_chars = true;
                sb_append_char(&sb, c);
                i++;
            }
        }

        if (has_chars) {
            if (token_list_add(out_list, TOKEN_WORD, sb.data ? sb.data : "") != 0) {
                sb_free(&sb);
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "minishell: maximum token limit exceeded");
                }
                return -1;
            }
        }
        sb_free(&sb);
    }

    return 0;
}
