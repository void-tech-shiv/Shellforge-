# ShellForge

**ShellForge** is a small Unix-style command shell written in C for an **Operating Systems & System Programming** project. It demonstrates process creation, program execution, pipes, file-descriptor redirection, signals, environment variables, background jobs, and persistent command history.

The shell also provides simple, readable custom command names so common Linux commands are easier to remember.

## Custom command names

| ShellForge | Linux command | Purpose |
|---|---|---|
| `list` | `ls` | List files and folders |
| `path` | `pwd` | Show current directory |
| `goto` | `cd` | Change directory |
| `make` | `mkdir` | Create directory |
| `new` | `touch` | Create empty file |
| `show` | `cat` | Show file contents |
| `copy` | `cp` | Copy files/directories |
| `move` | `mv` | Move or rename |
| `remove` | `rm` | Remove files/directories |
| `clean` | `clear` | Clear terminal |
| `hist` | `history` | Show ShellForge history |
| `print` | `echo` | Print text |
| `who` | `whoami` | Show current user |
| `time` | `date` | Show date/time |
| `quit` | `exit` | Exit ShellForge |
| `tasks` | `jobs` | Show background jobs |
| `stop` | `kill` | Send a signal to a process |
| `find` | `grep` | Search text |
| `first` | `head` | Show first lines |
| `last` | `tail` | Show last lines |

Normal Linux command names still work too.

## Features

- Interactive `shellforge:<cwd>$` prompt
- `fork()` + `execvp()` external command execution
- Multi-stage pipelines: `list -l | find ".c" | wc -l`
- Input/output redirection: `<`, `>`, `>>`
- stderr redirection: `2>`, `2>>`, `2>&1`
- Background execution with `&`
- Basic background-job list with `tasks`
- Environment expansion: `$HOME`, `$PATH`, `$?`, `$$`, `${NAME}`
- `export` and `unset`
- Persistent history in `~/.shellforge_history`
- Single/double quotes and backslash escapes
- `Ctrl+C` does not terminate the shell
- Built-in `help` and `aliases`

## Build on Ubuntu/Linux

```bash
sudo apt update
sudo apt install build-essential -y
make
```

Run:

```bash
./shellforge
```

Or:

```bash
make run
```

## Quick demo

```text
shellforge$ aliases
shellforge$ path
shellforge$ make demo
shellforge$ goto demo
shellforge$ new notes.txt
shellforge$ print "hello shellforge" > notes.txt
shellforge$ show notes.txt
shellforge$ show notes.txt | find hello
shellforge$ sleep 10 &
shellforge$ tasks
shellforge$ print $?
shellforge$ quit
```

## OS concepts demonstrated

| Concept | Implementation |
|---|---|
| Process creation | `fork()` |
| Program execution | `execvp()` |
| Process synchronization | `waitpid()` |
| IPC | `pipe()` |
| File descriptors | `dup2()`, `open()`, `close()` |
| Signals | `SIGINT`, `SIGQUIT`, `SIGTSTP` handling |
| Environment | `getenv()`, `setenv()`, `unsetenv()` |
| Background processes | `&` + non-blocking `waitpid()` |
| Parsing | Quote-aware tokenizer + pipeline/redirection parser |

## Tests

```bash
make test
```

## Project structure

```text
ShellForge/
├── src/
│   └── main.c
├── tests/
│   └── test_shellforge.sh
├── Makefile
├── README.md
├── LICENSE
└── .gitignore
```

## Notes

ShellForge is an educational shell, not a complete replacement for Bash/Zsh. Its focus is showing core OS and system-programming concepts clearly in a compact C project.
