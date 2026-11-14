#ifndef MINISHELL_PARSER_H
#define MINISHELL_PARSER_H

#include "tokenizer.h"
#include "command.h"

/*
 * Parses a token list into a command_line_t AST.
 * If err_buf is non-NULL and an error occurs, an error message is written into err_buf.
 * Returns command_line_t pointer on success, NULL on syntax error.
 */
command_line_t *parse(const token_list_t *tokens, char *err_buf, size_t err_buf_size);

#endif /* MINISHELL_PARSER_H */

