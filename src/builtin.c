#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include "alias.h"
#include "builtin.h"
#include "history.h"
#include "jobs.h"

int is_builtin_name(const char *name)
{
    static const char *names[] = {
        "cd", "pwd", "echo", "clear", "whoami", "date", "uname", "env",
        "sleep", "mkdir", "touch", "history", "jobs", "fg", "bg", "kill",
        "export", "unset", "help", "aliases", "exit"
    };
    const char *canon = builtin_canonical_name(name);
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); i++)
        if (strcmp(canon, names[i]) == 0) return 1;
    return 0;
}

static int builtin_cd(command_t *cmd)
{
    const char *dir = NULL;
    char previous[4096] = {0};
    getcwd(previous, sizeof(previous));

    if (cmd->argc == 1) {
        dir = getenv("HOME");
        if (!dir) dir = "/";
    } else if (cmd->argc == 2) {
        if (strcmp(cmd->argv[1], "-") == 0) {
            dir = getenv("OLDPWD");
            if (!dir) { fprintf(stderr, "goto: OLDPWD not set\n"); return 1; }
            printf("%s\n", dir);
        } else {
            dir = cmd->argv[1];
        }
    } else {
        fprintf(stderr, "goto: too many arguments\n");
        return 1;
    }

    if (chdir(dir) < 0) {
        perror("goto");
        return 1;
    }

    if (previous[0]) setenv("OLDPWD", previous, 1);
    char now[4096];
    if (getcwd(now, sizeof(now))) setenv("PWD", now, 1);
    return 0;
}

static int builtin_pwd(command_t *cmd)
{
    if (cmd->argc > 1) { fprintf(stderr, "path: too many arguments\n"); return 1; }
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) { perror("path"); return 1; }
    puts(cwd);
    return 0;
}

static int builtin_echo(command_t *cmd)
{
    int start = 1;
    int newline = 1;
    if (cmd->argc > 1 && strcmp(cmd->argv[1], "-n") == 0) { newline = 0; start = 2; }
    for (int i = start; i < cmd->argc; i++) {
        if (i > start) putchar(' ');
        fputs(cmd->argv[i], stdout);
    }
    if (newline) putchar('\n');
    return 0;
}

static int builtin_clear(command_t *cmd)
{
    if (cmd->argc > 1) { fprintf(stderr, "clean: too many arguments\n"); return 1; }
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
    return 0;
}

static int builtin_whoami(command_t *cmd)
{
    if (cmd->argc > 1) { fprintf(stderr, "who: too many arguments\n"); return 1; }
    struct passwd *pw = getpwuid(geteuid());
    if (pw) puts(pw->pw_name); else printf("%d\n", (int)geteuid());
    return 0;
}

static int builtin_date(command_t *cmd)
{
    if (cmd->argc > 1) { fprintf(stderr, "time: too many arguments\n"); return 1; }
    time_t now = time(NULL);
    struct tm tmv;
    if (!localtime_r(&now, &tmv)) { perror("time"); return 1; }
    char out[128];
    if (!strftime(out, sizeof(out), "%a %b %d %H:%M:%S %Z %Y", &tmv)) return 1;
    puts(out);
    return 0;
}

static int builtin_uname(command_t *cmd)
{
    if (cmd->argc > 1) { fprintf(stderr, "uname: options are not implemented\n"); return 1; }
    struct utsname u;
    if (uname(&u) < 0) { perror("uname"); return 1; }
    printf("%s %s %s %s\n", u.sysname, u.release, u.version, u.machine);
    return 0;
}

static int builtin_env(command_t *cmd)
{
    if (cmd->argc > 1) { fprintf(stderr, "env: arguments are not implemented\n"); return 1; }
    extern char **environ;
    for (char **p = environ; *p; p++) puts(*p);
    return 0;
}

static int builtin_sleep(command_t *cmd)
{
    if (cmd->argc != 2) { fprintf(stderr, "sleep: usage: sleep <seconds>\n"); return 1; }
    char *end = NULL;
    double sec = strtod(cmd->argv[1], &end);
    if (!end || *end || sec < 0) { fprintf(stderr, "sleep: invalid time\n"); return 1; }
    struct timespec req;
    req.tv_sec = (time_t)sec;
    req.tv_nsec = (long)((sec - (double)req.tv_sec) * 1000000000.0);
    while (nanosleep(&req, &req) < 0 && errno == EINTR) {}
    return 0;
}

static int builtin_mkdir(command_t *cmd)
{
    if (cmd->argc < 2) { fprintf(stderr, "make: usage: make <directory>...\n"); return 1; }
    int rc = 0;
    for (int i = 1; i < cmd->argc; i++) {
        if (mkdir(cmd->argv[i], 0755) < 0) { perror(cmd->argv[i]); rc = 1; }
    }
    return rc;
}

static int builtin_touch(command_t *cmd)
{
    if (cmd->argc < 2) { fprintf(stderr, "new: usage: new <file>...\n"); return 1; }
    int rc = 0;
    for (int i = 1; i < cmd->argc; i++) {
        int fd = open(cmd->argv[i], O_WRONLY | O_CREAT, 0644);
        if (fd < 0) { perror(cmd->argv[i]); rc = 1; continue; }
        if (futimens(fd, NULL) < 0) { perror(cmd->argv[i]); rc = 1; }
        close(fd);
    }
    return rc;
}

static int builtin_export(command_t *cmd)
{
    int rc = 0;
    for (int i = 1; i < cmd->argc; i++) {
        char *eq = strchr(cmd->argv[i], '=');
        if (!eq) { fprintf(stderr, "export: expected NAME=value\n"); rc = 1; continue; }
        *eq = '\0';
        if (setenv(cmd->argv[i], eq + 1, 1) < 0) { perror("export"); rc = 1; }
        *eq = '=';
    }
    return rc;
}

static int builtin_unset(command_t *cmd)
{
    for (int i = 1; i < cmd->argc; i++)
        if (unsetenv(cmd->argv[i]) < 0) perror("unset");
    return 0;
}

static int builtin_kill(command_t *cmd)
{
    if (cmd->argc < 2 || cmd->argc > 3) {
        fprintf(stderr, "stop: usage: stop <pid|%%job> [signal]\n");
        return 1;
    }
    int sig = SIGTERM;
    if (cmd->argc == 3) {
        sig = atoi(cmd->argv[2]);
        if (sig <= 0) sig = SIGTERM;
    }
    if (cmd->argv[1][0] == '%') {
        int id = atoi(cmd->argv[1] + 1);
        return jobs_signal(id, sig);
    }
    pid_t pid = (pid_t)atoi(cmd->argv[1]);
    if (pid <= 0 || kill(pid, sig) < 0) { perror("stop"); return 1; }
    return 0;
}

static void print_help(void)
{
    puts("ShellForge - modular Unix-style shell\n"
         "\nCustom commands:\n"
         "  list path goto make new show copy move remove clean\n"
         "  hist print who time quit tasks stop find first last\n"
         "\nBuilt-ins implemented inside ShellForge:\n"
         "  cd/pwd/echo/clear/whoami/date/uname/env/sleep/mkdir/touch\n"
         "  history/jobs/fg/bg/kill/export/unset/help/aliases/exit\n"
         "\nShell features:\n"
         "  pipelines |     redirection < > >> 2> 2>> 2>&1\n"
         "  background &    job control: jobs, fg <id>, bg <id>\n"
         "  variables: $VAR ${VAR} $? $$ and ~ expansion");
}

int execute_builtin(command_t *cmd, int parent_context, int *request_exit, int *exit_status)
{
    if (!cmd || cmd->argc == 0) return 0;
    const char *n = builtin_canonical_name(cmd->argv[0]);

    if (strcmp(n, "cd") == 0) return builtin_cd(cmd);
    if (strcmp(n, "pwd") == 0) return builtin_pwd(cmd);
    if (strcmp(n, "echo") == 0) return builtin_echo(cmd);
    if (strcmp(n, "clear") == 0) return builtin_clear(cmd);
    if (strcmp(n, "whoami") == 0) return builtin_whoami(cmd);
    if (strcmp(n, "date") == 0) return builtin_date(cmd);
    if (strcmp(n, "uname") == 0) return builtin_uname(cmd);
    if (strcmp(n, "env") == 0) return builtin_env(cmd);
    if (strcmp(n, "sleep") == 0) return builtin_sleep(cmd);
    if (strcmp(n, "mkdir") == 0) return builtin_mkdir(cmd);
    if (strcmp(n, "touch") == 0) return builtin_touch(cmd);
    if (strcmp(n, "history") == 0) return history_print();
    if (strcmp(n, "jobs") == 0) { jobs_print(); return 0; }
    if (strcmp(n, "fg") == 0) {
        if (!parent_context) return 1;
        if (cmd->argc != 2) { fprintf(stderr, "fg: usage: fg <job_id>\n"); return 1; }
        return jobs_foreground(atoi(cmd->argv[1]));
    }
    if (strcmp(n, "bg") == 0) {
        if (!parent_context) return 1;
        if (cmd->argc != 2) { fprintf(stderr, "bg: usage: bg <job_id>\n"); return 1; }
        return jobs_background(atoi(cmd->argv[1]));
    }
    if (strcmp(n, "kill") == 0) return builtin_kill(cmd);
    if (strcmp(n, "export") == 0) return builtin_export(cmd);
    if (strcmp(n, "unset") == 0) return builtin_unset(cmd);
    if (strcmp(n, "help") == 0) { print_help(); return 0; }
    if (strcmp(n, "aliases") == 0) { print_aliases(); return 0; }
    if (strcmp(n, "exit") == 0) {
        int code = cmd->argc > 1 ? atoi(cmd->argv[1]) : 0;
        if (parent_context && request_exit) *request_exit = 1;
        if (exit_status) *exit_status = code;
        return code;
    }

    return 127;
}
