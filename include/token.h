#ifndef SHELLFORGE_TOKEN_H
#define SHELLFORGE_TOKEN_H

#define MAX_TOKENS 256
#define MAX_TOKEN_LEN 1024

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_INPUT,
    TOKEN_OUTPUT,
    TOKEN_APPEND,
    TOKEN_ERROR,
    TOKEN_ERROR_APPEND,
    TOKEN_ERROR_TO_OUTPUT,
    TOKEN_BACKGROUND,
    TOKEN_END
} token_type_t;

typedef struct {
    token_type_t type;
    char text[MAX_TOKEN_LEN];
} token_t;

typedef struct {
    token_t tokens[MAX_TOKENS];
    int count;
} token_list_t;

void token_list_init(token_list_t *list);
int token_add(token_list_t *list, token_type_t type, const char *text);

#endif
