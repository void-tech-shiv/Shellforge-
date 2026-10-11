#include <stdio.h>
#include <string.h>
#include "alias.h"

typedef struct {
    const char *custom;
    const char *real;
    const char *description;
} alias_t;

static const alias_t aliases[] = {
    {"list",   "ls",      "list files and folders"},
    {"path",   "pwd",     "show current directory (built-in)"},
    {"goto",   "cd",      "change directory (built-in)"},
    {"make",   "mkdir",   "create directory (built-in)"},
    {"new",    "touch",   "create/update file (built-in)"},
    {"show",   "cat",     "show file contents"},
    {"copy",   "cp",      "copy files/directories"},
    {"move",   "mv",      "move or rename"},
    {"remove", "rm",      "remove files/directories"},
    {"clean",  "clear",   "clear terminal (built-in)"},
    {"hist",   "history", "show persistent history (built-in)"},
    {"print",  "echo",    "print text (built-in)"},
    {"who",    "whoami",  "show current user (built-in)"},
    {"time",   "date",    "show date/time (built-in)"},
    {"quit",   "exit",    "exit ShellForge (built-in)"},
    {"tasks",  "jobs",    "list jobs (built-in)"},
    {"stop",   "kill",    "send a signal (built-in)"},
    {"find",   "grep",    "search text"},
    {"first",  "head",    "show first lines"},
    {"last",   "tail",    "show last lines"}
};

const char *alias_resolve(const char *name)
{
    for (size_t i = 0; i < sizeof(aliases)/sizeof(aliases[0]); i++)
        if (strcmp(name, aliases[i].custom) == 0)
            return aliases[i].real;
    return name;
}

const char *builtin_canonical_name(const char *name)
{
    const char *resolved = alias_resolve(name);
    return resolved;
}

void print_aliases(void)
{
    puts("ShellForge custom commands:");
    for (size_t i = 0; i < sizeof(aliases)/sizeof(aliases[0]); i++)
        printf("  %-8s -> %-8s %s\n", aliases[i].custom, aliases[i].real, aliases[i].description);
}
