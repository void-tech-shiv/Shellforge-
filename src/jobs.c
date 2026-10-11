#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include "jobs.h"

static job_t table[MAX_JOBS];
static int job_count = 0;
static int next_id = 1;
static pid_t shell_pgid = 0;
static int interactive_shell = 0;

static void prompt_sigint(int sig)
{
    (void)sig;
    const char nl = '\n';
    write(STDOUT_FILENO, &nl, 1);
}

void jobs_init(void)
{
    memset(table, 0, sizeof(table));
    job_count = 0;
    next_id = 1;
}

void jobs_setup_shell(void)
{
    interactive_shell = isatty(STDIN_FILENO);
    shell_pgid = getpid();

    if (!interactive_shell)
        return;

    while (tcgetpgrp(STDIN_FILENO) != getpgrp())
        kill(-getpgrp(), SIGTTIN);

    if (setpgid(shell_pgid, shell_pgid) < 0 && errno != EACCES && errno != EPERM)
        perror("shellforge: setpgid");
    shell_pgid = getpgrp();

    if (tcsetpgrp(STDIN_FILENO, shell_pgid) < 0)
        perror("shellforge: tcsetpgrp");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = prompt_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

pid_t jobs_shell_pgid(void) { return shell_pgid; }
int jobs_is_interactive(void) { return interactive_shell; }

static job_t *find_by_pid(pid_t pid)
{
    for (int i = 0; i < job_count; i++)
        for (int j = 0; j < table[i].process_count; j++)
            if (table[i].pids[j] == pid)
                return &table[i];
    return NULL;
}

static void update_specific_job(job_t *job, pid_t pid, int status)
{
    if (!job) return;

    int index = -1;
    for (int i = 0; i < job->process_count; i++) {
        if (job->pids[i] == pid) { index = i; break; }
    }
    if (index < 0) return;

    if (WIFSTOPPED(status)) {
        job->state = JOB_STOPPED;
        return;
    }
    if (WIFCONTINUED(status)) {
        if (job->state != JOB_DONE) job->state = JOB_RUNNING;
        return;
    }
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        job->pids[index] = 0;
        if (job->live_count > 0) job->live_count--;
        if (job->live_count == 0) job->state = JOB_DONE;
    }
}

int jobs_add(pid_t pgid, const pid_t *pids, int count, const char *command, job_state_t state)
{
    if (job_count >= MAX_JOBS) {
        fprintf(stderr, "shellforge: jobs: table full\n");
        return -1;
    }

    job_t *job = &table[job_count++];
    memset(job, 0, sizeof(*job));
    job->id = next_id++;
    job->pgid = pgid;
    job->process_count = count;
    job->live_count = count;
    job->state = state;

    for (int i = 0; i < count && i < MAX_JOB_PROCESSES; i++)
        job->pids[i] = pids[i];

    strncpy(job->command, command ? command : "", sizeof(job->command) - 1);
    return job->id;
}

job_t *jobs_find(int id)
{
    for (int i = 0; i < job_count; i++)
        if (table[i].id == id) return &table[i];
    return NULL;
}

void jobs_remove(int id)
{
    for (int i = 0; i < job_count; i++) {
        if (table[i].id != id) continue;
        for (int j = i; j < job_count - 1; j++)
            table[j] = table[j + 1];
        job_count--;
        memset(&table[job_count], 0, sizeof(table[job_count]));
        return;
    }
}

void jobs_update_wait_status(pid_t pid, int status)
{
    job_t *job = find_by_pid(pid);
    update_specific_job(job, pid, status);
}

void jobs_reap(void)
{
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0)
        jobs_update_wait_status(pid, status);
}

void jobs_print(void)
{
    jobs_reap();
    if (job_count == 0) {
        puts("No active jobs.");
        return;
    }

    for (int i = 0; i < job_count; ) {
        job_t *job = &table[i];
        const char *state = job->state == JOB_RUNNING ? "Running" :
                            job->state == JOB_STOPPED ? "Stopped" : "Done";
        printf("[%d] %-8s pgid=%d %s\n", job->id, state, (int)job->pgid, job->command);
        if (job->state == JOB_DONE) {
            int id = job->id;
            jobs_remove(id);
        } else {
            i++;
        }
    }
}

static int wait_group_internal(job_t *job, int *last_status)
{
    int status = 0;
    int result = 0;

    while (job->live_count > 0) {
        pid_t pid = waitpid(-job->pgid, &status, WUNTRACED);
        if (pid < 0) {
            if (errno == EINTR) continue;
            if (errno == ECHILD) break;
            perror("shellforge: waitpid");
            result = 1;
            break;
        }

        update_specific_job(job, pid, status);
        if (WIFSTOPPED(status)) {
            job->state = JOB_STOPPED;
            result = 128 + WSTOPSIG(status);
            break;
        }

        if (WIFEXITED(status)) result = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) result = 128 + WTERMSIG(status);
    }

    if (last_status) *last_status = result;
    return result;
}

int jobs_wait_foreground_group(pid_t pgid, job_t *existing_job, const pid_t *pids, int count, const char *command, int *last_status)
{
    job_t temp;
    job_t *job = existing_job;

    if (!job) {
        memset(&temp, 0, sizeof(temp));
        temp.pgid = pgid;
        temp.process_count = count;
        temp.live_count = count;
        temp.state = JOB_RUNNING;
        for (int i = 0; i < count; i++) temp.pids[i] = pids[i];
        strncpy(temp.command, command ? command : "", sizeof(temp.command) - 1);
        job = &temp;
    }

    if (interactive_shell)
        tcsetpgrp(STDIN_FILENO, pgid);

    int result = wait_group_internal(job, last_status);

    if (interactive_shell)
        tcsetpgrp(STDIN_FILENO, shell_pgid);

    if (!existing_job && job->state == JOB_STOPPED) {
        int id = jobs_add(job->pgid, job->pids, job->process_count, job->command, JOB_STOPPED);
        job_t *saved = jobs_find(id);
        if (saved) {
            saved->live_count = job->live_count;
            saved->stopped_count = job->stopped_count;
        }
        printf("[%d] Stopped %s\n", id, job->command);
    }

    return result;
}

int jobs_foreground(int id)
{
    jobs_reap();
    job_t *job = jobs_find(id);
    if (!job) {
        fprintf(stderr, "fg: no such job [%d]\n", id);
        return 1;
    }
    if (job->state == JOB_DONE) {
        jobs_remove(id);
        fprintf(stderr, "fg: job already completed\n");
        return 1;
    }

    printf("%s\n", job->command);
    if (job->state == JOB_STOPPED) {
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("fg: SIGCONT");
            return 1;
        }
        job->state = JOB_RUNNING;
        job->stopped_count = 0;
    }

    if (interactive_shell)
        tcsetpgrp(STDIN_FILENO, job->pgid);
    int status = 0;
    wait_group_internal(job, &status);
    if (interactive_shell)
        tcsetpgrp(STDIN_FILENO, shell_pgid);

    if (job->state == JOB_DONE)
        jobs_remove(id);
    return status;
}

int jobs_background(int id)
{
    jobs_reap();
    job_t *job = jobs_find(id);
    if (!job) {
        fprintf(stderr, "bg: no such job [%d]\n", id);
        return 1;
    }
    if (kill(-job->pgid, SIGCONT) < 0) {
        perror("bg: SIGCONT");
        return 1;
    }
    job->state = JOB_RUNNING;
    job->stopped_count = 0;
    printf("[%d] %s &\n", job->id, job->command);
    return 0;
}

int jobs_signal(int id, int sig)
{
    jobs_reap();
    job_t *job = jobs_find(id);
    if (!job) {
        fprintf(stderr, "stop: no such job [%d]\n", id);
        return 1;
    }
    if (kill(-job->pgid, sig) < 0) {
        perror("stop");
        return 1;
    }
    return 0;
}
