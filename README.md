# MiniShell — Mini Operating System Shell in C (C11) & WebAssembly

[![C11](https://img.shields.io/badge/Language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![WebAssembly](https://img.shields.io/badge/WebAssembly-Emscripten-654ff0.svg)](https://webassembly.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub Pages](https://img.shields.io/badge/Demo-GitHub%20Pages-brightgreen.svg)](https://lokeshpuma.github.io/minishell-c/)

**MiniShell** is an educational, production-grade Unix-like command interpreter written in ISO C11. It demystifies shell internals by implementing core operating system primitives from scratch: lexical tokenization, recursive AST parsing, process creation (`fork`), binary execution (`execvp`), pipeline plumbing (`pipe`/`dup2`), file descriptor redirection, signal dispatching (`SIGINT`), and exit-status handling.

The project features two distinct deployment targets:
1. **Native POSIX Shell**: A standalone executable for macOS and Linux that interfaces directly with host OS system calls.
2. **Browser WebAssembly Demo**: A sandboxed, static demo compiled with Emscripten featuring an in-memory Virtual Filesystem (VFS) and stream-based command pipelines hosted on GitHub Pages.

🌐 **Live Browser Demo**: [https://lokeshpuma.github.io/minishell-c/](https://lokeshpuma.github.io/minishell-c/)

---

## Table of Contents

- [How MiniShell Works](#how-minishell-works)
- [Architecture & Target Separation](#architecture--target-separation)
- [How to Run on Your Own Computer](#how-to-run-on-your-own-computer)
  - [Prerequisites](#prerequisites)
  - [Option 1: Build & Run Native POSIX Shell (Make)](#option-1-build--run-native-posix-shell-make)
  - [Option 2: Build with CMake](#option-2-build-with-cmake)
  - [Option 3: Run the WebAssembly Browser Demo Locally](#option-3-run-the-webassembly-browser-demo-locally)
- [Supported Syntax & Commands](#supported-syntax--commands)
- [Automated Testing](#automated-testing)
- [GitHub Pages Deployment Guide](#github-pages-deployment-guide)
- [Repository Structure](#repository-structure)
- [Security & Reliability Guarantees](#security--reliability-guarantees)
- [License](#license)

---

## How MiniShell Works

MiniShell implements a classic Unix command execution pipeline:

```
+-----------------------------------------------------------------------+
|                             User Input                                |
|             "echo 'hello world' | tr a-z A-Z > output.txt"            |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                        1. Lexical Tokenizer                           |
|  - Tokenizes words, quotes ('...', "..."), escapes (\x), & operators  |
|  - Expands variables ($VAR, ${VAR}, $?) while preserving single quotes |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                          2. Parser & AST                              |
|  - Separates sequential pipelines by semicolon (;)                   |
|  - Divides multi-stage pipelines by pipe (|)                          |
|  - Associates redirections (<, >, >>, 2>, 2>>) with commands         |
|  - Validates syntax (rejects missing targets, double pipes, etc.)     |
+-----------------------------------------------------------------------+
                                   |
         +-------------------------+-------------------------+
         |                                                   |
         v                                                   v
+---------------------------------+   +---------------------------------+
|     3A. Native POSIX Engine     |   |      3B. WebAssembly Engine     |
|                                 |   |                                 |
| - Standalone state-changers     |   | - In-memory Virtual Filesystem  |
|   (cd, export, unset, exit) run |   |   (VFS) nodes & content buffers |
|   in parent process             |   |                                 |
| - fork() child processes        |   | - Byte stream pipelining        |
| - pipe() & dup2() fd plumbing   |   |   Stage 1 Out -> Stage 2 In     |
| - open() & dup2() redirections  |   |                                 |
| - execvp() PATH resolution      |   | - Safe command allowlist        |
| - waitpid() status collection   |   |   (no arbitrary host access)    |
+---------------------------------+   +---------------------------------+
```

### 1. Tokenizer & Quoting Rules
- **Single Quotes (`'...'`)**: Treats all enclosed characters literally. Variable expansion and backslash escaping are disabled.
- **Double Quotes (`"..."`)**: Preserves whitespace and literal characters while expanding environment variables (`$VAR`, `${VAR}`) and exit codes (`$?`). Escaped characters (`\"`, `\\`, `\$`) are evaluated.
- **Backslash Outside Quotes (`\x`)**: Treats `x` as a literal character (e.g. `\ ` preserves an argument space).
- **Operators**: Accurately isolates control operators `|`, `<`, `>`, `>>`, `2>`, `2>>`, and `;`.

### 2. Pipelines & File Descriptor Hygiene
For an $N$-stage pipeline ($N \ge 2$):
- $N - 1$ unidirectional UNIX pipes are created with `pipe()`.
- Each child connects standard input from `pipes[i - 1][0]` (for $i > 0$) and standard output to `pipes[i][1]` (for $i < N - 1$) using `dup2()`.
- **Zero FD Leaks & Deadlock Prevention**: Every unused pipe descriptor is closed immediately in both child and parent processes. If a parent process leaves write ends open, downstream readers never receive an `EOF` signal and deadlock.
- The parent waits for all child PIDs with `waitpid()` and returns the exit status of the final pipeline command.

### 3. State-Changing Built-in Isolation
- Built-ins that alter shell state (`cd`, `export`, `unset`, `exit`) execute directly inside the **parent shell** when invoked standalone.
- When invoked inside a pipeline (e.g. `cd subdir | echo "done"`), they execute in an isolated **child subshell**, guaranteeing that subshell mutations do not bleed into parent shell state.

### 4. Signal Handling
- The interactive shell installs a custom `sigaction` handler for `SIGINT` (Ctrl+C).
- Foreground child processes run with `SIG_DFL`, allowing Ctrl+C to terminate running child jobs without killing the parent shell.
- Interrupted foreground commands record a status of `130` ($128 + \text{SIGINT}$).

---

## Architecture & Target Separation

| Capability | Native POSIX Shell | Browser WebAssembly Demo |
|---|---|---|
| **Compilation** | Clang / GCC (C11) | Emscripten (`emcc`) to `.wasm` |
| **Execution** | Kernel processes (`fork`/`execvp`) | In-memory simulated execution |
| **Pipelines** | Kernel file descriptors (`pipe`/`dup2`)| In-memory byte streaming |
| **Redirection** | POSIX `open()` & `dup2()` | Virtual Filesystem (VFS) inodes |
| **Filesystem Access**| Host operating system storage | Isolated in-memory VFS (transient) |
| **Host Security** | User permissions boundary | Sandboxed within browser tab |
| **Available Binaries**| Any executable found in `$PATH` | Strict allowlisted POSIX utilities |

---

## How to Run on Your Own Computer

### Prerequisites

For Native Shell:
- macOS or Linux
- C11-compatible compiler (`clang` or `gcc`)
- `make` or `cmake` (3.12+)

For WebAssembly Build (optional, pre-compiled binaries are already included in `web/public/wasm/`):
- Emscripten SDK (`emsdk`)
- Node.js (for local test running)

---

### Option 1: Build & Run Native POSIX Shell (Make)

From the project root directory:

```bash
# 1. Compile the native executable
make

# 2. Launch the interactive shell
./bin/minishell
```

#### Example Session in MiniShell:
```bash
minishell:/home/user$ pwd
minishell:/home/user$ ls -la | grep src
minishell:/home/user$ echo "Hello from MiniShell" > test.txt
minishell:/home/user$ cat < test.txt
Hello from MiniShell
minishell:/home/user$ echo "Appended content" >> test.txt
minishell:/home/user$ cat test.txt
Hello from MiniShell
Appended content
minishell:/home/user$ export MY_NAME="Alice"
minishell:/home/user$ echo "Welcome, $MY_NAME!"
Welcome, Alice!
minishell:/home/user$ type ls
ls is /bin/ls
minishell:/home/user$ type cd
cd is a shell builtin
minishell:/home/user$ exit 0
```

#### Execute Non-Interactive Commands (`-c` flag):
```bash
./bin/minishell -c "printf 'apple\nbanana\ncherry\n' | wc -l"
```

#### Run All Automated Tests:
```bash
make test
```

#### Clean Build Artifacts:
```bash
make clean
```

---

### Option 2: Build with CMake

```bash
# Create build directory and compile
mkdir build && cd build
cmake ..
cmake --build .

# Run the test suite
ctest --output-on-failure

# Launch executable
./minishell
```

---

### Option 3: Run the WebAssembly Browser Demo Locally

The WebAssembly binary (`minishell.wasm`) and JavaScript bridge (`minishell.js`) are pre-compiled and bundled in `web/public/wasm/`.

Because web browsers restrict loading WebAssembly binaries over the `file://` protocol due to CORS, serve the `web/` folder with any static web server:

```bash
# Using Python 3 (recommended)
cd web && python3 -m http.server 8000
```
*(Or with Node: `npx serve web`)*

Open **[http://localhost:8000](http://localhost:8000)** in your browser.

#### Recompiling WebAssembly from Source (Optional):
```bash
# Activate your Emscripten SDK environment
source /path/to/emsdk/emsdk_env.sh

# Compile WebAssembly target into web/public/wasm/
make wasm
```

---

## Supported Syntax & Commands

### Control Operators
| Syntax | Operator | Description | Native | WebAssembly |
|---|---|---|---|---|
| `cmd1 \| cmd2` | Pipe | Passes stdout of `cmd1` to stdin of `cmd2` | Yes | Yes |
| `cmd < file` | Stdin Redirect | Reads input from file | Yes | Yes |
| `cmd > file` | Stdout Redirect | Overwrites/creates file with stdout | Yes | Yes |
| `cmd >> file` | Stdout Append | Appends stdout to file | Yes | Yes |
| `cmd 2> file` | Stderr Redirect | Overwrites/creates file with stderr | Yes | Yes |
| `cmd 2>> file` | Stderr Append | Appends stderr to file | Yes | Yes |
| `cmd1 ; cmd2` | Semicolon | Executes `cmd1` then `cmd2` sequentially | Yes | Yes |

### Built-in Commands
- `help`: Display help and syntax reference.
- `pwd`: Print current working directory.
- `cd [path]`: Change directory (defaults to `$HOME`).
- `echo [args...]`: Print text separated by spaces.
- `clear`: Clear screen display (`\033[H\033[2J\033[3J`).
- `history`: Display current session command history.
- `env`: Display environment variables.
- `export NAME=value`: Set or update environment variable.
- `unset NAME`: Unset environment variable.
- `type COMMAND`: Categorize command as a built-in or locate path in `$PATH`.
- `exit [code]`: Terminate shell session with optional status code.

### WebAssembly Virtual File Utilities
Operating on the in-memory Virtual Filesystem:
- `ls [-l] [-a] [path]`, `cat [files...]`, `head [-n N] [file]`, `tail [-n N] [file]`
- `wc [-l] [-w] [-c] [file]`, `grep [-v] [-i] PATTERN [file]`
- `sort [-r] [file]`, `uniq [-c] [file]`, `mkdir [path]`, `touch [file]`
- `rm [-r] [path]`, `cp [src] [dst]`, `mv [src] [dst]`

---

## Automated Testing

MiniShell includes an automated test harness covering both unit-level behavior and full end-to-end integration:

```bash
make test
```

### Test Coverage:
1. **Tokenizer Unit Tests (`tests/test_tokenizer.c`)**: 43/43 tests passing.
   - Word splitting, repeated whitespace, escaped spaces, control operator isolation, single/double quotes, unclosed quotes detection, token limits, and `$VAR` / `${VAR}` / `$?` expansion.
2. **Parser Unit Tests (`tests/test_parser.c`)**: 32/32 tests passing.
   - AST tree creation, multi-command pipelines, all redirection permutations, semicolon separation, leading/trailing/double pipe rejection, and missing redirection target detection.
3. **Built-ins Unit Tests (`tests/test_builtins.c`)**: 32/32 tests passing.
   - Parent vs child execution checks, environment exports/unsets, identifier validation, exit code defaults, and directory changing.
4. **Virtual Engine Tests (`tests/test_virtual.c`)**: 16/16 tests passing.
   - VFS file creation, reading, path canonicalization, virtual pipelines, and unknown command allowlist rejection.
5. **Native Integration Suite (`tests/test_native.sh`)**: 18/18 tests passing.
   - Real subprocess pipelines (`printf 'a\nb\n' | wc -l`), file descriptor cleanups, stderr redirections (`2>`), external program execution (`true`, `false`, `127` not found), and subshell isolation checks (verifying `cd dir | ls` does NOT modify the parent shell).

---

## GitHub Pages Deployment Guide

MiniShell is configured for automated deployment to GitHub Pages via GitHub Actions.

### Setup Instructions:
1. In your GitHub repository ([lokeshpuma/minishell-c](https://github.com/lokeshpuma/minishell-c)), go to **Settings** > **Pages**.
2. Under **Build and deployment** > **Source**, choose **GitHub Actions**.
3. Push your commits to `main` branch:
   ```bash
   git push origin main
   ```
4. The GitHub Actions workflow (`.github/workflows/deploy-pages.yml`) will automatically:
   - Run native C11 unit and integration tests.
   - Set up the Emscripten SDK and compile the WebAssembly target (`make wasm`).
   - Validate distribution assets (`index.html`, `styles.css`, `main.js`, `minishell.wasm`, `minishell.js`).
   - Deploy the static site to GitHub Pages.
5. Visit your live site at: **`https://lokeshpuma.github.io/minishell-c/`**

---

## Repository Structure

```text
minishell-c/
├── src/
│   ├── common/             # Shared ISO C11 syntax and parsing layer
│   │   ├── tokenizer.c/h   # Lexical tokenizer, quote escaping & variable expansion
│   │   ├── parser.c/h      # AST parser for pipelines & redirections
│   │   ├── command.c/h     # AST memory models & argument handling
│   │   ├── builtins.c/h    # Shared built-in command handlers
│   │   └── history.c/h     # Session history management
│   ├── native/             # Native POSIX execution layer
│   │   ├── main.c          # Interactive prompt & REPL loop
│   │   ├── executor_posix.c# Real fork(), execvp(), pipe(), dup2() execution
│   │   └── signals.c/h     # Async-signal-safe handling for SIGINT
│   └── wasm/               # WebAssembly backend
│       ├── virtual_fs.c/h  # In-memory virtual filesystem (VFS)
│       ├── executor_virtual.c # In-memory stream pipeline executor
│       └── wasm_entry.c    # Exported C API for JavaScript WebAssembly bridge
├── web/                    # Static Web Frontend for GitHub Pages
│   ├── index.html          # Responsive terminal web portfolio UI
│   ├── src/
│   │   ├── main.js         # JavaScript bridge to WebAssembly module
│   │   └── styles.css      # Dark terminal UI stylesheet
│   └── public/wasm/        # Compiled WebAssembly binary artifacts
│       ├── minishell.js    # Emscripten JS module loader
│       └── minishell.wasm  # Compiled WebAssembly binary
├── tests/                  # Automated test suite
│   ├── test_tokenizer.c    # Unit tests for tokenizer
│   ├── test_parser.c       # Unit tests for parser
│   ├── test_builtins.c     # Unit tests for built-ins
│   ├── test_virtual.c      # Unit tests for VFS and virtual pipelines
│   └── test_native.sh      # End-to-end POSIX integration tests
├── docs/                   # Documentation
│   ├── architecture.md     # Deep-dive internal architecture specification
│   └── command-reference.md# Complete syntax and command reference
├── .github/workflows/
│   └── deploy-pages.yml    # GitHub Actions workflow for Pages deployment
├── CMakeLists.txt          # Modern CMake configuration
├── Makefile                # Strict POSIX Makefile
├── LICENSE                 # MIT License
└── README.md               # Project documentation
```

---

## Security & Reliability Guarantees

- **No `system()` Calls**: External programs are executed strictly via `fork()` and `execvp()`. User input is never concatenated into shell command strings.
- **No Browser `eval()`**: The WebAssembly UI bridge strictly invokes compiled C functions without `eval()` or `new Function()`.
- **Zero Host File Access in Browser**: The WebAssembly engine operates exclusively on an isolated in-memory VFS.
- **Resource Cleanup**: Every allocated AST node, token, and string buffer is freed on all execution and error paths. All opened pipe and file descriptors are closed to eliminate resource leaks.
- **Strict Compilation**: Built with `-std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L`.

---

## License

This project is licensed under the [MIT License](LICENSE).
