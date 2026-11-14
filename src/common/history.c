#define _POSIX_C_SOURCE 200809L
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

history_t *history_create(void) {
    history_t *hist = (history_t *)malloc(sizeof(history_t));
    if (!hist) return NULL;
    hist->capacity = 32;
    hist->count = 0;
    hist->entries = (char **)malloc(sizeof(char *) * hist->capacity);
    if (!hist->entries) {
        free(hist);
        return NULL;
    }
    return hist;
}

void history_free(history_t *hist) {
    if (!hist) return;
    if (hist->entries) {
        for (size_t i = 0; i < hist->count; i++) {
            if (hist->entries[i]) {
                free(hist->entries[i]);
            }
        }
        free(hist->entries);
    }
    free(hist);
}

int history_add(history_t *hist, const char *line) {
    if (!hist || !line) return -1;
    /* Skip empty lines */
    if (line[0] == '\0') return 0;

    if (hist->count >= hist->capacity) {
        size_t new_cap = hist->capacity * 2;
        char **new_entries = (char **)realloc(hist->entries, sizeof(char *) * new_cap);
        if (!new_entries) return -1;
        hist->entries = new_entries;
        hist->capacity = new_cap;
    }

    hist->entries[hist->count] = strdup(line);
    if (!hist->entries[hist->count]) return -1;
    hist->count++;
    return 0;
}

size_t history_count(const history_t *hist) {
    return hist ? hist->count : 0;
}

const char *history_get(const history_t *hist, size_t index) {
    if (!hist || index >= hist->count) return NULL;
    return hist->entries[index];
}

void history_print(const history_t *hist) {
    if (!hist) return;
    for (size_t i = 0; i < hist->count; i++) {
        printf("  %4zu  %s\n", i + 1, hist->entries[i]);
    }
}
