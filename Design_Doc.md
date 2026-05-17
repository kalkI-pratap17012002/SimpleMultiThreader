# SimpleShell — Technical Design Document

**Project:** Unix shell implemented in C using POSIX APIs  
**Author:** Ayush Kumar  
**Stack:** C · POSIX · fork/exec · pipe/dup2 · signal handling · Linux

---

## Table of Contents

1. [Problem Statement](#1-problem-statement)
2. [Design Goals](#2-design-goals)
3. [Architecture](#3-architecture)
4. [Input & Parsing Pipeline](#4-input--parsing-pipeline)
5. [Process Execution Model](#5-process-execution-model)
6. [Pipeline Implementation](#6-pipeline-implementation)
7. [Built-in Command Handling](#7-built-in-command-handling)
8. [History & Execution Tracking](#8-history--execution-tracking)
9. [Signal Handling](#9-signal-handling)
10. [Design Trade-offs](#10-design-trade-offs)
11. [Testing Strategy](#11-testing-strategy)
12. [Known Limitations & Future Work](#12-known-limitations--future-work)

---

## 1. Problem Statement

A Unix shell is the primary interface between a user and the OS kernel. Building one from scratch requires a deep understanding of process management, file descriptor manipulation, and the semantics of the fork-exec model. The core challenge is that many shell behaviors that appear simple on the surface — `cd` changing directories, pipelines passing data between processes, `Ctrl+C` not killing the shell — each require precise understanding of process isolation and signal propagation.

SimpleShell implements this execution model in C using only POSIX APIs, handling the full lifecycle: read → parse → fork → exec → wait → record → repeat.

---

## 2. Design Goals

| Priority | Goal | Rationale |
|---|---|---|
| P0 | **Correct fork-exec model** | Child must exec; parent must wait; file descriptors must not leak |
| P0 | **Pipeline correctness** | Data must flow through `N-1` pipes for an `N`-stage pipeline without deadlock |
| P0 | **Built-ins in parent process** | `cd` and `pwd` must mutate the shell's own state, not a child's |
| P1 | **History with timing** | PID + timestamps give users visibility into command performance |
| P1 | **Clean signal handling** | `Ctrl+C` must interrupt the foreground command, not the shell |
| P2 | **Exit summary** | Print full session history on exit for auditability |

---

## 3. Architecture

```
┌────────────────────────────────────────────────────────────┐
│                        Shell Loop                          │
│  print prompt → getline() → parse → dispatch → record     │
└───────────────┬────────────────────────────────────────────┘
                │
        ┌───────┴────────┐
        │                │
        ▼                ▼
┌──────────────┐  ┌──────────────────────────────────────────┐
│   Built-ins  │  │          External / Pipeline             │
│  (in parent) │  │                                          │
│  cd, pwd,    │  │  parse_pipeline() → N stages             │
│  echo, date, │  │  create N-1 pipes                        │
│  history,    │  │  fork() × N children                     │
│  help, clear,│  │  dup2() stdin/stdout per child           │
│  exit        │  │  execvp() each stage                     │
└──────────────┘  │  waitpid() all children                  │
                  └──────────────────────────────────────────┘
                                    │
                                    ▼
                         ┌─────────────────┐
                         │  History Logger  │
                         │  record_history()│
                         │  PID, timestamps │
                         │  duration        │
                         └─────────────────┘
```

---

## 4. Input & Parsing Pipeline

### Reading Input

```c
getline(&line, &len, stdin);
```

`getline()` handles arbitrary line lengths by dynamically resizing its buffer. This avoids fixed-size input buffers and the truncation bugs they cause.

### Pipeline Splitting

```c
strtok_r(line, "|", &saveptr);
```

`strtok_r()` (reentrant variant) splits the input on `|` into pipeline stages. The reentrant version is used because parsing is called recursively and a global strtok state would be clobbered.

### Argument Splitting

Each stage is passed to `parse_command()` which splits on whitespace into `argv[]` — the format `execvp()` expects. Whitespace trimming is applied to each token to handle spacing around `|`.

```
Input:  "cat simpleshell.c | grep int | wc -l"
        │
        ├── Stage 0: argv = ["cat", "simpleshell.c", NULL]
        ├── Stage 1: argv = ["grep", "int", NULL]
        └── Stage 2: argv = ["wc", "-l", NULL]
```

---

## 5. Process Execution Model

For every external command the shell:

1. Calls `fork()` — creates an identical child process
2. In the child: sets up file descriptors (see Pipeline section), then calls `execvp()` which replaces the child's image with the target binary
3. In the parent: calls `waitpid()` with the child's PID to collect its exit status and prevent zombie processes
4. Records the command in history with the PID and timing

```c
pid_t pid = fork();
if (pid == 0) {
    // child
    dup2(pipe_in,  STDIN_FILENO);
    dup2(pipe_out, STDOUT_FILENO);
    execvp(argv[0], argv);
    perror("execvp failed");
    exit(1);
} else {
    // parent
    waitpid(pid, &status, 0);
    record_history(command, pid, start, end);
}
```

---

## 6. Pipeline Implementation

### Pipe Creation

For an N-stage pipeline, N-1 pipes are created upfront:

```c
int pipes[N-1][2];
for (int i = 0; i < N-1; i++)
    pipe(pipes[i]);
```

### File Descriptor Wiring

Each child's stdin/stdout is connected to the appropriate pipe ends using `dup2()`:

```
Stage 0:  stdout → pipes[0][1]
Stage 1:  stdin  ← pipes[0][0],  stdout → pipes[1][1]
Stage 2:  stdin  ← pipes[1][0]
```

After `dup2()`, all original pipe file descriptors are closed in the child. This is critical — if the write end of a pipe stays open in any process, the reader never sees EOF and deadlocks.

### Parent Cleanup

After spawning all children, the parent closes all pipe ends in its own process (again, to prevent the deadlock described above) and then calls `waitpid()` for each child.

---

## 7. Built-in Command Handling

Built-ins are identified before any `fork()` call and executed directly in the parent process.

**Why this matters for `cd`:**  
`chdir()` changes the working directory of the calling process. If `cd` were executed in a forked child, the child's directory would change — but the child is a separate process. When it exits, the parent's directory is unchanged. Running `cd` in the parent is the only correct implementation.

The same logic applies to `exit` (must print summary and terminate the shell process itself, not a child).

```
Command received
      │
      ├── is_builtin(argv[0]) ?
      │       │
      │       └── YES → execute in parent → return
      │
      └── NO → fork() → execvp() in child → waitpid() in parent
```

---

## 8. History & Execution Tracking

### Data Structure

```c
#define MAX_HISTORY 100

typedef struct {
    char   command[MAX_CMD_LEN];
    pid_t  pid;
    time_t start_time;
    time_t end_time;
} HistoryEntry;

HistoryEntry history[MAX_HISTORY];
int history_count = 0;
```

### Recording

`record_history()` is called after every command — both built-ins and external commands — capturing the full command string, PID (0 for built-ins), start, and end timestamps.

### Exit Summary

`print_summary()` iterates the history array and formats each entry:

```
Session Summary:
[1]  PID=2341  ls -l              start=14:03:01  duration=0.012s
[2]  PID=2342  cat file | wc -l  start=14:03:05  duration=0.008s
[3]  PID=0     cd /tmp            start=14:03:10  duration=0.000s
```

---

## 9. Signal Handling

`Ctrl+C` sends `SIGINT` to the foreground process group. Without explicit handling, this would also terminate the shell. The shell installs a custom `SIGINT` handler that ignores the signal in the parent process — so `Ctrl+C` kills the foreground child without killing the shell itself.

`Ctrl+D` (EOF on stdin) is detected when `getline()` returns -1. The shell calls `print_summary()` and exits cleanly — same behavior as the `exit` built-in.

---

## 10. Design Trade-offs

| Decision | Chosen | Rejected | Reason |
|---|---|---|---|
| Input reading | `getline()` | `fgets()` with fixed buffer | Handles arbitrarily long commands without truncation |
| Pipeline splitting | `strtok_r()` | Manual pointer walking | Cleaner; reentrant-safe for nested parsing calls |
| Built-in execution | In parent process | Fork + exec | Semantic correctness for state-mutating commands |
| Pipe count | `N-1` pipes upfront | Lazy creation | Simplifies wiring logic; easier to reason about FD lifetimes |
| History storage | Global static array | Dynamic allocation | Sufficient for session scope; no allocation failures |
| Timing | `time()` wall clock | `clock_gettime()` | Simpler; sufficient precision for shell command durations |

---

## 11. Testing Strategy

### Built-in Tests

```bash
pwd                          # verify correct directory printed
cd /tmp && pwd               # verify cd + pwd interaction
echo hello world             # verify whitespace-separated args
date                         # verify system date output
history                      # verify commands appear with metadata
```

### External Command Tests

```bash
ls
ls -l
wc -l simpleshell.c
grep main simpleshell.c
```

### Pipeline Tests

```bash
cat simpleshell.c | wc -l
cat simpleshell.c | grep int | wc -l   # 2-stage and 3-stage pipelines
```

### Custom Executable Tests

```bash
./helloworld          # verify fork+exec path
./fib 10              # fast; verify correct output
./fib 40              # slow; verify history records long duration correctly
```

### Signal Tests

- `Ctrl+C` during `./fib 40` — shell must survive, return prompt
- `Ctrl+D` — shell must print summary and exit

### History Verification

- Commands appear in order with correct PIDs
- Durations are plausible (fib 40 >> fib 10)
- Built-ins appear with PID=0 and near-zero duration

---

## 12. Known Limitations & Future Work

**Current Limitations**

- No quoted string support — `echo "hello world"` treats the quotes as literal characters
- No I/O redirection — `>`, `<`, `>>` are not parsed
- No background execution — `&` is not supported
- No command substitution — `` `cmd` `` or `$(cmd)` not supported
- No escape sequences — `\n`, `\\` treated literally
- History capped at 100 entries per session (static array)

**Future Improvements**

- **I/O Redirection** — detect `>` and `<` tokens during parsing; open the target file and `dup2()` before `execvp()`
- **Background Jobs** — skip `waitpid()` for commands ending with `&`; maintain a job table; reap with `SIGCHLD`
- **Quote Parsing** — stateful tokenizer that respects quoted boundaries
- **Readline Integration** — arrow-key history navigation, tab completion

---

*SimpleShell demonstrates a practical understanding of OS process management: fork-exec model, pipe-based IPC, file descriptor lifecycle, and the semantics that make built-in commands fundamentally different from external ones.*
