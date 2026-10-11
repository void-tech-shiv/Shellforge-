#ifndef SHELLFORGE_JOBS_H
#define SHELLFORGE_JOBS_H

#include <sys/types.h>
#include "parser.h"

#define MAX_JOBS 64
#define MAX_JOB_PROCESSES MAX_COMMANDS
#define MAX_JOB_COMMAND 512

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} job_state_t;

typedef struct {
    int id;
    pid_t pgid;
    pid_t pids[MAX_JOB_PROCESSES];
    int process_count;
    int live_count;
    int stopped_count;
    job_state_t state;
    char command[MAX_JOB_COMMAND];
} job_t;

void jobs_init(void);
void jobs_setup_shell(void);
pid_t jobs_shell_pgid(void);
int jobs_is_interactive(void);

int jobs_add(pid_t pgid, const pid_t *pids, int count, const char *command, job_state_t state);
job_t *jobs_find(int id);
void jobs_remove(int id);
void jobs_reap(void);
void jobs_print(void);

int jobs_foreground(int id);
int jobs_background(int id);
int jobs_signal(int id, int sig);

void jobs_update_wait_status(pid_t pid, int status);
int jobs_wait_foreground_group(pid_t pgid, job_t *existing_job, const pid_t *pids, int count, const char *command, int *last_status);

#endif
