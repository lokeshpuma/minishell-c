# MiniShell Command Reference

This document details all supported commands, syntax structures, I/O operators, and platform differences between the native POSIX shell and the WebAssembly demo.

---

## 1. Syntax and Operators

| Syntax | Operator | Description | Target Support |
|---|---|---|---|
| `cmd1 \| cmd2` | Pipeline | Feeds standard output of `cmd1` to standard input of `cmd2` | Native (pipes) & Web (in-memory) |
| `cmd < file` | Input Redirect | Redirects standard input from specified file | Native & Web (VFS) |
| `cmd > file` | Output Truncate | Redirects stdout, creating or truncating file | Native & Web (VFS) |
| `cmd >> file` | Output Append | Redirects stdout, creating or appending to file | Native & Web (VFS) |
| `cmd 2> file` | Stderr Truncate | Redirects standard error, creating or truncating file | Native & Web (VFS) |
| `cmd 2>> file` | Stderr Append | Redirects standard error, creating or appending to file | Native & Web (VFS) |
| `cmd1 ; cmd2` | Sequential | Executes `cmd1` followed by `cmd2` | Native & Web |

---

## 2. Quoting and Expansion Rules

- **Single Quotes (`'...'`)**:
  - Encloses literal text.
  - Variable expansion and backslash escaping are disabled.
  - Example: `'hello $NAME'` prints `hello $NAME`.
- **Double Quotes (`"..."`)**:
  - Preserves whitespace and literal characters while enabling parameter expansion (`$VAR`, `${VAR}`, `$?`).
  - Supports backslash escape sequences: `\"`, `\\`, and `\$`.
  - Example: `"hello $USER"` prints `hello <username>`.
- **Parameter Expansion**:
  - `$NAME` or `${NAME}` expands environment variable. If unset, expands to empty string.
  - `$?` expands to the exit code of the preceding command.

---

## 3. Built-in Commands

### `help`
Displays supported built-in commands, operators, and syntax tips.
```sh
help
```

### `pwd`
Prints current working directory.
```sh
pwd
```

### `cd [path]`
Changes current directory. If no path is supplied, changes to `$HOME`.
- **Native**: Standalone `cd` changes parent shell directory via `chdir()`. When executed within a pipeline (`cd dir | ls`), changes are confined to the child subshell.
- **WebAssembly**: Updates virtual working directory in the Virtual Filesystem.
```sh
cd /tmp
cd ..
cd
```

### `echo [args...]`
Prints arguments separated by a single space, followed by a newline.
```sh
echo "Current directory:" $PWD
```

### `clear`
Clears terminal screen. In native terminal, transmits ANSI escape sequences `\033[H\033[2J\033[3J`.
```sh
clear
```

### `history`
Prints numbered list of commands executed during the current session.
```sh
history
```

### `env`
Lists current environment variables.
```sh
env
```

### `export NAME=value`
Sets or updates environment variables.
```sh
export PROJECT=minishell
export DEBUG=1
```

### `unset NAME`
Removes specified variable from the environment.
```sh
unset DEBUG
```

### `type COMMAND`
Reports whether `COMMAND` is a shell built-in or locates its path in `PATH`.
```sh
type cd       # cd is a shell builtin
type ls       # ls is /bin/ls
```

### `exit [status]`
Exits the shell with an optional integer exit status (default: status of previous command or 0).
```sh
exit 0
exit 42
```

---

## 4. WebAssembly Virtual Filesystem Commands

In addition to core built-ins, the WebAssembly target includes an allowlisted suite of POSIX file utilities operating on in-memory files:

- `ls [-l] [-a] [path]`: Lists files and directories with metadata.
- `cat [file...]`: Concatenates and displays files or standard input.
- `head [-n N] [file]`: Displays first $N$ lines (default: 10).
- `tail [-n N] [file]`: Displays last $N$ lines (default: 10).
- `wc [-l] [-w] [-c] [file]`: Counts lines, words, and characters.
- `grep [-v] [-i] PATTERN [file]`: Matches literal substrings line by line.
- `sort [-r] [file]`: Lexicographically sorts lines.
- `uniq [-c] [file]`: Filters adjacent duplicate lines with optional counts.
- `mkdir [path]`: Creates virtual directory.
- `touch [file]`: Creates empty virtual file.
- `rm [-r] [path]`: Deletes virtual file or directory.
- `cp [src] [dst]`: Copies virtual file.
- `mv [src] [dst]`: Renames or moves virtual file.

---

## 5. Exit Status Codes

- `0`: Success.
- `1`: General command error or failed condition.
- `2`: Syntax or tokenization error.
- `126`: Command invoked cannot execute (permission denied).
- `127`: Command not found in `$PATH` or command not available in browser demo.
- `130`: Command terminated by `SIGINT` (Ctrl+C).

