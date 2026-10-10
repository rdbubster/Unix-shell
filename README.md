# Mini Unix Shell
[![CI](https://github.com/rdbubster/Unix-shell/actions/workflows/ci.yml/badge.svg)](https://github.com/rdbubster/Unix-shell/actions/workflows/ci.yml)

A small Unix shell in C++17 built directly on POSIX system calls: process
creation, file-descriptor redirection, and inter-process pipes, with an
automated test suite and CI.

## Features
- Runs any program on PATH (`fork` + `execvp` + `waitpid`)
- Built-ins: `cd`, `pwd`, `exit [code]`
- Redirection: `<`, `>` (truncate), `>>` (append), also without spaces (`ls>out`)
- One pipe per line: `cmd1 | cmd2`, combinable with redirection
- Quote-aware tokenizer (`echo "a   b"`, `echo 'a|b'`)
- Errors are reported and the shell keeps running; exit status is tracked
  (127 = command not found, 128+N = killed by signal N)

## Architecture

