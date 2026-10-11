#ifndef SHELLFORGE_BUILTIN_H
#define SHELLFORGE_BUILTIN_H

#include "parser.h"

int is_builtin_name(const char *name);
int execute_builtin(command_t *cmd, int parent_context, int *request_exit, int *exit_status);

#endif
