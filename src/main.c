#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "executor.h"
#include "expand.h"
#include "history.h"
#include "jobs.h"
#include "lexer.h"
#include "parser.h"

int main(void)
{
    jobs_init();
    jobs_setup_shell();
    history_init();

    if (isatty(STDIN_FILENO)) {
        puts("============================================");
        puts(" ShellForge - OS & System Programming Shell");
        puts(" Type 'help' or 'aliases' to begin.");
        puts("============================================");
    }

    char *line = NULL;
    size_t capacity = 0;
    int last_status = 0;
    int request_exit = 0;
    int exit_status = 0;

    while (!request_exit) {
        jobs_reap();

        if (isatty(STDIN_FILENO)) {
            char cwd[4096];
            if (getcwd(cwd, sizeof(cwd))) printf("shellforge:%s$ ", cwd);
            else printf("shellforge$ ");
            fflush(stdout);
        }

        errno = 0;
        ssize_t n = getline(&line, &capacity, stdin);
        if (n < 0) {
            if (errno == EINTR) { clearerr(stdin); continue; }
            break;
        }

        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        char *input = line;
        while (isspace((unsigned char)*input)) input++;
        if (!*input) continue;

        history_add_line(input);

        token_list_t tokens;
        pipeline_t pipeline;
        if (!lexer(input, &tokens)) { last_status = 2; continue; }
        if (!parser(&tokens, &pipeline)) { last_status = 2; continue; }

        expand_pipeline(&pipeline, last_status);
        last_status = execute_pipeline(&pipeline, input, &request_exit, &exit_status);
    }

    free(line);
    return request_exit ? exit_status : last_status;
}
