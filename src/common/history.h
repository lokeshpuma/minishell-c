#ifndef MINISHELL_HISTORY_H
#define MINISHELL_HISTORY_H

#include <stddef.h>

typedef struct {
    char **entries;
    size_t count;
    size_t capacity;
} history_t;

history_t *history_create(void);
void history_free(history_t *hist);
int history_add(history_t *hist, const char *line);
size_t history_count(const history_t *hist);
const char *history_get(const history_t *hist, size_t index);
void history_print(const history_t *hist);

#endif /* MINISHELL_HISTORY_H */

