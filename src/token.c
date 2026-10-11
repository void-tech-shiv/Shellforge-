#include <string.h>
#include "token.h"

void token_list_init(token_list_t *list)
{
    list->count = 0;
}

int token_add(token_list_t *list, token_type_t type, const char *text)
{
    if (list->count >= MAX_TOKENS)
        return -1;

    token_t *t = &list->tokens[list->count++];
    t->type = type;
    strncpy(t->text, text ? text : "", MAX_TOKEN_LEN - 1);
    t->text[MAX_TOKEN_LEN - 1] = '\0';
    return 0;
}
