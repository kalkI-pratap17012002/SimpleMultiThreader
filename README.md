# SimpleShell

> A Unix shell implemented in C from scratch — supporting pipelines, built-in commands, external process execution, and a persistent history log with PID tracking and execution timing.

---

## Overview

SimpleShell replicates the core execution model of a Unix shell: read a command, parse it, fork a child, exec the binary, wait, repeat. Beyond the basics, it handles multi-stage pipelines, a set of built-in commands that run inside the parent process, and a per-session history log that records every command alongside its PID, start time, end time, and duration. On exit, a full execution summary is printed automatically.

```
$ ls -l | grep main | wc -l
       3
$ history
[1]  PID=1234  ls -l | grep main | wc -l  (0.003s)
```

---

## Features

- **Pipeline execution** — arbitrary `cmd1 | cmd2 | cmd3` chains via `pipe()` + `dup2()` + `fork()`
- **Built-in commands** — run inside the parent process so state-mutating commands (`cd`, `pwd`) correctly affect the shell itself
- **External commands** — any binary on `$PATH` via `fork()` + `execvp()`
- **History log** — every command recorded with full command string, PID, timestamps, and wall-clock duration
- **Exit summary** — `print_summary()` called on `exit` or `Ctrl+D`, showing all session history
- **Signal handling** — `Ctrl+C` and `Ctrl+D` handled cleanly without crashing

---

## Built-in Commands

| Command | Behavior |
|---|---|
| `cd <dir>` | Change working directory (affects parent process) |
| `pwd` | Print current working directory |
| `echo <args>` | Print arguments to stdout |
| `history` | Display session command history with PIDs and timings |
| `help` | List available built-ins |
| `date` | Print current date and time |
| `clear` | Clear the terminal screen |
| `exit` | Print execution summary and exit shell |

---

## Build & Run

**Requirements:** GCC, GNU Make, Linux/Unix (POSIX)

```bash
make          # compile SimpleShell
./simpleshell # launch the shell
```

Manual compile:
```bash
gcc -O2 -o simpleshell simpleshell.c
```

---

## Usage Examples

```bash
# External commands
$ ls -l
$ grep main simpleshell.c

# Pipelines
$ cat simpleshell.c | wc -l
$ cat simpleshell.c | grep int | wc -l

# Built-ins
$ cd /tmp
$ pwd
$ echo hello world
$ date

# Custom executables
$ ./helloworld
$ ./fib 10

# Exit with session summary
$ exit
```

---

## Implementation Details

### Input & Parsing

- `getline()` reads a full line of input
- `strtok_r()` splits on `|` to identify pipeline stages
- `parse_command()` splits each stage into an `argv[]` array suitable for `execvp()`

### Pipeline Execution

For an `N`-stage pipeline, `N-1` pipes are created. Each stage runs in a child process with `dup2()` wiring its stdin/stdout to the appropriate pipe ends. The shell waits for all children before returning the prompt.

```
cmd1 → pipe[0] → cmd2 → pipe[1] → cmd3 → stdout
```

### Built-in Handling

Built-ins are detected before forking. They execute directly in the parent process — this is what makes `cd` work correctly (a child process `chdir()` would not affect the parent's working directory).

### History

```c
struct HistoryEntry {
    char   command[MAX_CMD_LEN];
    pid_t  pid;
    time_t start_time;
    time_t end_time;
};
```

`record_history()` is called after every command. `print_summary()` iterates the array and formats each entry with duration on exit.

---

## Known Limitations

- No support for quoted strings (`"hello world"` treated as two tokens)
- No escape sequences (`\`)
- No I/O redirection (`>`, `<`, `>>`)
- No background process execution (`&`)
- No command substitution (`` `command` ``)
- History capped at `MAX_HISTORY` (100) entries per session

---

## Project Structure

```
.
├── simpleshell.c    # Complete shell implementation
├── helloworld.c     # Test executable
├── fib.c            # Fibonacci test executable (recursive, deliberately slow for timing tests)
└── Makefile
```

---

## Tech Stack

`C` · `POSIX` · `fork() / execvp()` · `pipe() / dup2()` · `waitpid()` · `signal handling` · `Linux`

---

## Repository

[github.com/kalkI-pratap17012002/SimpleShell](https://github.com/kalkI-pratap17012002/SimpleShell.git)
