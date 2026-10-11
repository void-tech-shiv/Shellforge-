#include <stdio.h>
#include <string.h>
#include "parser.h"

static void command_init(command_t *cmd)
{
    memset(cmd, 0, sizeof(*cmd));
}

static int copy_arg(command_t *cmd, const char *text)
{
    if (cmd->argc >= MAX_ARGS - 1) {
        fprintf(stderr, "shellforge: parser: too many arguments\n");
        return 0;
    }
    strncpy(cmd->arg_storage[cmd->argc], text, MAX_ARG_LEN - 1);
    cmd->arg_storage[cmd->argc][MAX_ARG_LEN - 1] = '\0';
    cmd->argv[cmd->argc] = cmd->arg_storage[cmd->argc];
    cmd->argc++;
    cmd->argv[cmd->argc] = NULL;
    return 1;
}

static int need_filename(const token_list_t *tokens, int i)
{
    return i + 1 < tokens->count && tokens->tokens[i + 1].type == TOKEN_WORD;
}

int parser(const token_list_t *tokens, pipeline_t *pipeline)
{
    memset(pipeline, 0, sizeof(*pipeline));
    pipeline->command_count = 1;
    command_init(&pipeline->commands[0]);
    int current = 0;

    for (int i = 0; i < tokens->count; i++) {
        const token_t *t = &tokens->tokens[i];
        command_t *cmd = &pipeline->commands[current];

        switch (t->type) {
        case TOKEN_WORD:
            if (!copy_arg(cmd, t->text)) return 0;
            break;

        case TOKEN_PIPE:
            if (cmd->argc == 0) {
                fprintf(stderr, "shellforge: parser: command expected before |\n");
                return 0;
            }
            if (current + 1 >= MAX_COMMANDS) {
                fprintf(stderr, "shellforge: parser: too many commands in pipeline\n");
                return 0;
            }
            current++;
            pipeline->command_count++;
            command_init(&pipeline->commands[current]);
            break;

        case TOKEN_INPUT:
        case TOKEN_OUTPUT:
        case TOKEN_APPEND:
        case TOKEN_ERROR:
        case TOKEN_ERROR_APPEND:
            if (!need_filename(tokens, i)) {
                fprintf(stderr, "shellforge: parser: filename expected after redirection\n");
                return 0;
            }
            i++;
            if (t->type == TOKEN_INPUT) {
                strncpy(cmd->input, tokens->tokens[i].text, sizeof(cmd->input) - 1);
            } else if (t->type == TOKEN_OUTPUT || t->type == TOKEN_APPEND) {
                strncpy(cmd->output, tokens->tokens[i].text, sizeof(cmd->output) - 1);
                cmd->append_output = (t->type == TOKEN_APPEND);
            } else {
                strncpy(cmd->error, tokens->tokens[i].text, sizeof(cmd->error) - 1);
                cmd->append_error = (t->type == TOKEN_ERROR_APPEND);
            }
            break;

        case TOKEN_ERROR_TO_OUTPUT:
            cmd->error_to_output = 1;
            break;

        case TOKEN_BACKGROUND:
            if (tokens->tokens[i + 1].type != TOKEN_END) {
                fprintf(stderr, "shellforge: parser: & must be at end of command\n");
                return 0;
            }
            pipeline->background = 1;
            break;

        case TOKEN_END:
            break;
        }
    }

    if (pipeline->commands[current].argc == 0) {
        fprintf(stderr, "shellforge: parser: empty command\n");
        return 0;
    }
    return 1;
}
