# MiniShell Architecture and Internals

## 1. Overview and Core Philosophy

MiniShell is designed with a strict decoupling between:
1. **Frontend / Syntax Layer**: Tokenization, AST construction, command validation, and session history (implemented in ISO C11 in `src/common/`).
2. **Native POSIX Execution Backend**: Direct operating system interaction via `fork()`, `execvp()`, `pipe()`, `dup2()`, and `sigaction()` (located in `src/native/`).
3. **WebAssembly Virtual Backend**: In-memory byte streaming and a hierarchical Virtual Filesystem (VFS) with an allowlisted command dispatcher for static web deployment (located in `src/wasm/`).

```
                +----------------------------+
                |  User Input (Line Buffer)  |
                +----------------------------+
                              |
                              v
                +----------------------------+
                |         Tokenizer          |
                |  Quotes, Escapes, Tokens   |
                +----------------------------+
                              |
                              v
                +----------------------------+
                |         AST Parser         |
                | Pipelines, Commands, Redir |
                +----------------------------+
                              |
               +--------------+--------------+
               |                             |
               v                             v
     [Native Target]                 [WebAssembly Target]
  POSIX Executor (fork/pipe)     In-Memory Virtual Pipeline
               |                             |
               v                             v
      Host OS Kernel Execution       Virtual In-Memory FS
```

---

## 2. Tokenization and Parsing

### Tokenizer (`src/common/tokenizer.c`)
- **Dynamic buffer collection**: Accumulates characters dynamically without static buffer limits.
- **Quote Processing**:
  - *Single Quotes (`'...'`)*: Preserves all characters verbatim, disabling escape characters and variable expansion.
  - *Double Quotes (`"..."`)*: Preserves whitespace while evaluating escape sequences (`\"`, `\\`, `\$`) and parameter expansions (`$VAR`, `${VAR}`, `$?`).
  - *Unquoted Escapes*: `\` escapes the immediate subsequent character.
- **Control Operators**: Recognizes `|`, `<`, `>`, `>>`, `2>`, `2>>`, and `;`.
- **Validation**: Enforces maximum line length (4096 bytes) and maximum token limit (512 tokens).

### Parser (`src/common/parser.c`)
- Produces a two-tier Abstract Syntax Tree (AST):
  - **`command_line_t`**: Array of pipelines separated by `;`.
  - **`pipeline_t`**: Array of sequential `command_t` stages connected via pipes.
  - **`command_t`**: Contains `argc`, `argv`, stdin redirect (`<`), stdout redirect (`>`, `>>`), and stderr redirect (`2>`, `2>>`).
- Rigorously validates syntax errors such as missing redirection targets, leading/trailing pipes, or double pipes (`||`).

---

## 3. Native POSIX Execution Mechanics

### Process Management
- Standalone state-changing built-ins (`cd`, `export`, `unset`, `exit`) execute directly inside the parent shell process.
- All external programs and pipeline commands execute inside child processes created via `fork()`.
- Program execution is dispatched with `execvp(argv[0], argv)` searching the system `PATH`.
- Return codes: Command not found yields exit code `127`; permission denied yields `126`.

### Pipelines and File Descriptors
For an $N$-stage pipeline ($N \ge 2$):
1. Exactly $N - 1$ unidirectional pipes are allocated with `pipe()`.
2. Each stage child redirects its standard input from `pipes[i - 1][0]` (when $i > 0$) using `dup2()`.
3. Each stage child redirects its standard output to `pipes[i][1]` (when $i < N - 1$) using `dup2()`.
4. **Leak & Deadlock Prevention**: Every unused descriptor is closed immediately in both child and parent contexts. If the parent leaves write-ends open, downstream readers never receive `EOF` and hang indefinitely.
5. The parent awaits each child PID using `waitpid()` and adopts the termination status of the final pipeline stage.

### Signals
- Interactive shell configures `sigaction` for `SIGINT` (Ctrl+C).
- Foreground child processes run with `SIG_DFL` so user interrupts cleanly terminate active foreground commands without killing the parent shell.
- Child termination by signal yields an exit status of $128 + \text{signum}$ (e.g. 130 for `SIGINT`).

---

## 4. WebAssembly Virtual Backend

### Virtual Filesystem (`src/wasm/virtual_fs.c`)
- Complete in-memory hierarchical tree of files and directories.
- Dynamically allocates file content buffers.
- Canonical path resolution resolves relative paths, `.`, and `..` without host syscalls.
- Includes pre-populated sample files (`/home/user/welcome.txt`, `/home/user/data/sample.csv`, `/etc/os-release`).

### In-Memory Pipeline Streaming
- In WebAssembly, multi-command pipelines execute as in-memory byte buffers:
  $$\text{Stage}_1 \xrightarrow{\text{out buffer}} \text{Stage}_2 \xrightarrow{\text{out buffer}} \text{Stage}_3$$
- Virtual file redirections write or read byte slices from virtual inodes.
- Safe allowlist strictly prevents arbitrary code execution or host inspection.

