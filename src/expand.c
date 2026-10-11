#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "expand.h"

static void append_text(char *out, size_t outsz, size_t *used, const char *text)
{
    while (*text && *used + 1 < outsz)
        out[(*used)++] = *text++;
    out[*used] = '\0';
}

static void append_char(char *out, size_t outsz, size_t *used, char c)
{
    if (*used + 1 < outsz) {
        out[(*used)++] = c;
        out[*used] = '\0';
    }
}

static void expand_one(const char *in, char *out, size_t outsz, int last_status)
{
    size_t i = 0, used = 0;
    out[0] = '\0';

    if (in[0] == '~' && (in[1] == '/' || in[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) append_text(out, outsz, &used, home);
        i = 1;
    }

    while (in[i] != '\0' && used + 1 < outsz) {
        if (in[i] != '$') {
            append_char(out, outsz, &used, in[i++]);
            continue;
        }

        if (in[i + 1] == '?') {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%d", last_status);
            append_text(out, outsz, &used, tmp);
            i += 2;
            continue;
        }

        if (in[i + 1] == '$') {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%ld", (long)getpid());
            append_text(out, outsz, &used, tmp);
            i += 2;
            continue;
        }

        if (in[i + 1] == '{') {
            size_t start = i + 2, end = start;
            while (in[end] && in[end] != '}') end++;
            if (in[end] == '}') {
                char name[256];
                size_t len = end - start;
                if (len >= sizeof(name)) len = sizeof(name) - 1;
                memcpy(name, in + start, len);
                name[len] = '\0';
                const char *value = getenv(name);
                if (value) append_text(out, outsz, &used, value);
                i = end + 1;
                continue;
            }
        }

        size_t start = i + 1, end = start;
        if (isalpha((unsigned char)in[end]) || in[end] == '_') {
            end++;
            while (isalnum((unsigned char)in[end]) || in[end] == '_') end++;
            char name[256];
            size_t len = end - start;
            if (len >= sizeof(name)) len = sizeof(name) - 1;
            memcpy(name, in + start, len);
            name[len] = '\0';
            const char *value = getenv(name);
            if (value) append_text(out, outsz, &used, value);
            i = end;
            continue;
        }

        append_char(out, outsz, &used, in[i++]);
    }
}

void expand_pipeline(pipeline_t *pipeline, int last_status)
{
    for (int i = 0; i < pipeline->command_count; i++) {
        command_t *cmd = &pipeline->commands[i];
        for (int j = 0; j < cmd->argc; j++) {
            char tmp[MAX_ARG_LEN];
            expand_one(cmd->argv[j], tmp, sizeof(tmp), last_status);
            strncpy(cmd->arg_storage[j], tmp, MAX_ARG_LEN - 1);
            cmd->arg_storage[j][MAX_ARG_LEN - 1] = '\0';
            cmd->argv[j] = cmd->arg_storage[j];
        }
    }
}
