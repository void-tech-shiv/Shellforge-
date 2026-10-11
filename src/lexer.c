#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "lexer.h"

static int is_operator_start(const char *s)
{
    return s[0] == '|' || s[0] == '<' || s[0] == '>' || s[0] == '&' ||
           (s[0] == '2' && s[1] == '>');
}

int lexer(const char *input, token_list_t *list)
{
    token_list_init(list);
    size_t i = 0;

    while (input[i] != '\0') {
        while (isspace((unsigned char)input[i])) i++;
        if (input[i] == '\0') break;

        if (strncmp(input + i, "2>&1", 4) == 0) {
            if (token_add(list, TOKEN_ERROR_TO_OUTPUT, "2>&1") < 0) return 0;
            i += 4;
            continue;
        }
        if (strncmp(input + i, "2>>", 3) == 0) {
            if (token_add(list, TOKEN_ERROR_APPEND, "2>>") < 0) return 0;
            i += 3;
            continue;
        }
        if (strncmp(input + i, "2>", 2) == 0) {
            if (token_add(list, TOKEN_ERROR, "2>") < 0) return 0;
            i += 2;
            continue;
        }
        if (strncmp(input + i, ">>", 2) == 0) {
            if (token_add(list, TOKEN_APPEND, ">>") < 0) return 0;
            i += 2;
            continue;
        }
        if (input[i] == '|') { if (token_add(list, TOKEN_PIPE, "|") < 0) return 0; i++; continue; }
        if (input[i] == '<') { if (token_add(list, TOKEN_INPUT, "<") < 0) return 0; i++; continue; }
        if (input[i] == '>') { if (token_add(list, TOKEN_OUTPUT, ">") < 0) return 0; i++; continue; }
        if (input[i] == '&') { if (token_add(list, TOKEN_BACKGROUND, "&") < 0) return 0; i++; continue; }

        char word[MAX_TOKEN_LEN];
        size_t j = 0;
        int single = 0, dbl = 0;

        while (input[i] != '\0') {
            char c = input[i];

            if (!single && !dbl && (isspace((unsigned char)c) || is_operator_start(input + i)))
                break;

            if (!dbl && c == '\'') {
                single = !single;
                i++;
                continue;
            }
            if (!single && c == '"') {
                dbl = !dbl;
                i++;
                continue;
            }
            if (!single && c == '\\' && input[i + 1] != '\0') {
                i++;
                c = input[i];
            }

            if (j + 1 < sizeof(word))
                word[j++] = c;
            i++;
        }

        if (single || dbl) {
            fprintf(stderr, "shellforge: lexer: unclosed quote\n");
            return 0;
        }

        word[j] = '\0';
        if (token_add(list, TOKEN_WORD, word) < 0) {
            fprintf(stderr, "shellforge: lexer: too many tokens\n");
            return 0;
        }
    }

    if (token_add(list, TOKEN_END, "") < 0)
        return 0;
    return 1;
}
