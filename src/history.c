#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "history.h"

static char history_path[4096];

void history_init(void)
{
    const char *home = getenv("HOME");
    snprintf(history_path, sizeof(history_path), "%s/.shellforge_history", home ? home : ".");
}

void history_add_line(const char *line)
{
    FILE *f = fopen(history_path, "a");
    if (!f) return;
    fprintf(f, "%s\n", line);
    fclose(f);
}

int history_print(void)
{
    FILE *f = fopen(history_path, "r");
    if (!f) {
        puts("History is empty.");
        return 0;
    }

    char *line = NULL;
    size_t cap = 0;
    int n = 1;
    while (getline(&line, &cap, f) >= 0)
        printf("%4d  %s", n++, line);

    free(line);
    fclose(f);
    return 0;
}
