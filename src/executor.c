#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "alias.h"
#include "builtin.h"
#include "executor.h"
#include "jobs.h"

static int apply_redirection(const command_t *cmd)
{
    int fd;

    if (cmd->input[0]) {
        fd = open(cmd->input, O_RDONLY);
        if (fd < 0) { perror(cmd->input); return -1; }
        if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 input"); close(fd); return -1; }
        close(fd);
    }

    if (cmd->output[0]) {
        int flags = O_WRONLY | O_CREAT | (cmd->append_output ? O_APPEND : O_TRUNC);
        fd = open(cmd->output, flags, 0644);
        if (fd < 0) { perror(cmd->output); return -1; }
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 output"); close(fd); return -1; }
        close(fd);
    }

    if (cmd->error[0]) {
        int flags = O_WRONLY | O_CREAT | (cmd->append_error ? O_APPEND : O_TRUNC);
        fd = open(cmd->error, flags, 0644);
        if (fd < 0) { perror(cmd->error); return -1; }
        if (dup2(fd, STDERR_FILENO) < 0) { perror("dup2 error"); close(fd); return -1; }
        close(fd);
    }

    if (cmd->error_to_output && dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
        perror("dup2 2>&1");
        return -1;
    }
    return 0;
}

static void child_default_signals(void)
{
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
}

static int execute_parent_builtin(command_t *cmd, int *request_exit, int *exit_status)
{
    int in = dup(STDIN_FILENO);
    int out = dup(STDOUT_FILENO);
    int err = dup(STDERR_FILENO);
    if (in < 0 || out < 0 || err < 0) {
        perror("shellforge: dup");
        return 1;
    }

    int status = 1;
    if (apply_redirection(cmd) == 0)
        status = execute_builtin(cmd, 1, request_exit, exit_status);

    fflush(NULL);
    dup2(in, STDIN_FILENO);
    dup2(out, STDOUT_FILENO);
    dup2(err, STDERR_FILENO);
    close(in); close(out); close(err);
    return status;
}

int execute_pipeline(pipeline_t *pipeline, const char *raw_line, int *request_exit, int *exit_status)
{
    if (!pipeline || pipeline->command_count <= 0) return 0;

    if (pipeline->command_count == 1 && !pipeline->background &&
        is_builtin_name(pipeline->commands[0].argv[0])) {
        return execute_parent_builtin(&pipeline->commands[0], request_exit, exit_status);
    }

    pid_t pids[MAX_COMMANDS] = {0};
    pid_t pgid = 0;
    int previous_read = -1;

    for (int i = 0; i < pipeline->command_count; i++) {
        int pipefd[2] = {-1, -1};
        if (i < pipeline->command_count - 1 && pipe(pipefd) < 0) {
            perror("shellforge: pipe");
            return 1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("shellforge: fork");
            if (pipefd[0] >= 0) close(pipefd[0]);
            if (pipefd[1] >= 0) close(pipefd[1]);
            return 1;
        }

        if (pid == 0) {
            child_default_signals();

            if (pgid == 0) pgid = getpid();
            if (setpgid(0, pgid) < 0 && errno != EACCES)
                perror("shellforge: child setpgid");

            if (previous_read >= 0 && dup2(previous_read, STDIN_FILENO) < 0) {
                perror("shellforge: dup2 pipe input");
                _exit(1);
            }
            if (i < pipeline->command_count - 1 && dup2(pipefd[1], STDOUT_FILENO) < 0) {
                perror("shellforge: dup2 pipe output");
                _exit(1);
            }

            if (previous_read >= 0) close(previous_read);
            if (pipefd[0] >= 0) close(pipefd[0]);
            if (pipefd[1] >= 0) close(pipefd[1]);

            command_t *cmd = &pipeline->commands[i];
            if (apply_redirection(cmd) < 0) _exit(1);

            if (is_builtin_name(cmd->argv[0])) {
                int child_exit = 0, child_code = 0;
                int rc = execute_builtin(cmd, 0, &child_exit, &child_code);
                fflush(NULL);
                _exit(rc);
            }

            cmd->argv[0] = (char *)alias_resolve(cmd->argv[0]);
            execvp(cmd->argv[0], cmd->argv);
            fprintf(stderr, "shellforge: %s: %s\n", cmd->argv[0], strerror(errno));
            _exit(127);
        }

        pids[i] = pid;
        if (pgid == 0) pgid = pid;
        if (setpgid(pid, pgid) < 0 && errno != EACCES && errno != ESRCH)
            perror("shellforge: parent setpgid");

        if (previous_read >= 0) close(previous_read);
        if (pipefd[1] >= 0) close(pipefd[1]);
        previous_read = pipefd[0];
    }

    if (previous_read >= 0) close(previous_read);

    if (pipeline->background) {
        int id = jobs_add(pgid, pids, pipeline->command_count, raw_line, JOB_RUNNING);
        if (id > 0) printf("[%d] %d\n", id, (int)pgid);
        return 0;
    }

    int last_status = 0;
    jobs_wait_foreground_group(pgid, NULL, pids, pipeline->command_count,
                               raw_line, &last_status);
    return last_status;
}
