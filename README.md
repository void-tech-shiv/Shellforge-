# ShellForge

ShellForge is a modular Unix-style shell written in C for an **Operating Systems & System Programming** project.

This version combines three core goals:

1. **Modular architecture** — lexer, parser, executor, built-ins, job control, history, expansion and aliases are separate modules.
2. **Built-ins implemented inside ShellForge** — several common commands use C/POSIX APIs instead of launching the Linux utility.
3. **Real job-control foundations** — pipelines get their own process group and interactive foreground jobs receive terminal control with `tcsetpgrp()`.

## Build

```bash
sudo apt update
sudo apt install build-essential -y
make
./shellforge
```

No Readline dependency is required.

## Project structure

```text
ShellForge/
├── include/
│   ├── alias.h
│   ├── builtin.h
│   ├── executor.h
│   ├── expand.h
│   ├── history.h
│   ├── jobs.h
│   ├── lexer.h
│   ├── parser.h
│   └── token.h
├── src/
│   ├── alias.c
│   ├── builtin.c
│   ├── executor.c
│   ├── expand.c
│   ├── history.c
│   ├── jobs.c
│   ├── lexer.c
│   ├── main.c
│   ├── parser.c
│   └── token.c
├── tests/
│   └── test_shellforge.sh
├── Makefile
└── .github/workflows/build.yml
```

## Custom command names

| ShellForge | Linux-style name | Implementation |
|---|---|---|
| `path` | `pwd` | built-in using `getcwd()` |
| `goto` | `cd` | built-in using `chdir()` |
| `make` | `mkdir` | built-in using `mkdir()` |
| `new` | `touch` | built-in using `open()` + `futimens()` |
| `print` | `echo` | built-in |
| `clean` | `clear` | built-in ANSI terminal control |
| `who` | `whoami` | built-in using UID lookup |
| `time` | `date` | built-in using `time()` |
| `hist` | `history` | built-in persistent history |
| `tasks` | `jobs` | built-in job table |
| `stop` | `kill` | built-in using `kill()` |
| `quit` | `exit` | built-in |
| `list` | `ls` | external program alias |
| `show` | `cat` | external program alias |
| `copy` | `cp` | external program alias |
| `move` | `mv` | external program alias |
| `remove` | `rm` | external program alias |
| `find` | `grep` | external program alias |
| `first` | `head` | external program alias |
| `last` | `tail` | external program alias |

Normal Linux-style names also work for built-ins.

## Job control

```bash
sleep 30 &
tasks
fg 1
# press Ctrl+Z
bg 1
stop %1 15
```

Internally ShellForge uses `fork()`, `setpgid()`, `tcsetpgrp()`, `waitpid()`, `SIGCONT` and `kill()`.

## Pipes and redirection

```bash
list -l | find ".c" | wc -l
print hello > out.txt
show < out.txt
print again >> out.txt
ls missing 2> errors.txt
command > all.txt 2>&1
```

## Variable expansion

```bash
print $HOME
print ${HOME}
print $?
print $$
```

## Tests

```bash
make test
```
