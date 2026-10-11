#ifndef SHELLFORGE_EXECUTOR_H
#define SHELLFORGE_EXECUTOR_H

#include "parser.h"

int execute_pipeline(pipeline_t *pipeline, const char *raw_line, int *request_exit, int *exit_status);

#endif
