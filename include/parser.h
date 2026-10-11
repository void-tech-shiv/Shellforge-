#ifndef SHELLFORGE_PARSER_H
#define SHELLFORGE_PARSER_H

#include "token.h"

#define MAX_COMMANDS 32
#define MAX_ARGS 128
#define MAX_PATH_LEN 4096
#define MAX_ARG_LEN 1024

typedef struct {
    char *argv[MAX_ARGS];
    char arg_storage[MAX_ARGS][MAX_ARG_LEN];
    int argc;

    char input[MAX_PATH_LEN];
    char output[MAX_PATH_LEN];
    char error[MAX_PATH_LEN];
    int append_output;
    int append_error;
    int error_to_output;
} command_t;

typedef struct {
    command_t commands[MAX_COMMANDS];
    int command_count;
    int background;
} pipeline_t;

int parser(const token_list_t *tokens, pipeline_t *pipeline);

#endif
