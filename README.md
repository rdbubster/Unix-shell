# Mini Unix Shell

[![CI](https://github.com/rdbubster/Unix-shell/actions/workflows/ci.yml/badge.svg)](https://github.com/rdbubster/Unix-shell/actions/workflows/ci.yml)

A lightweight Unix-style command-line shell built from scratch in **C++17** using POSIX system calls. This project demonstrates process creation and management, command execution, file-descriptor redirection, inter-process communication, and automated testing.

## Features

- **External command execution:** Launch programs available on `PATH` using `fork()`, `execvp()`, and `waitpid()`.
- **Built-in commands:** Implement `cd`, `pwd`, and `exit`.
- **Input and output redirection:** Support `<`, `>`, and `>>` for file input, output, and appending.
- **Command pipelines:** Connect two commands using `|`.
- **Quote-aware tokenization:** Handle quoted arguments and prevent quoted text from being interpreted as shell syntax.
- **Error handling:** Report invalid commands and malformed input while allowing the shell to continue where appropriate.
- **Exit-status management:** Track command results, including command-not-found status and signal-related termination.
- **Automated tests:** Validate shell behavior across normal commands, error conditions, redirection, pipelines, and repeated execution.
- **Continuous integration:** Use GitHub Actions to build the project and run the test suite automatically.

## Architecture

The shell processes each input line through a command-processing and execution pipeline.

1. **Input:** Read a command line from the user.
2. **Tokenization:** Convert the input into arguments while respecting supported quoting rules.
3. **Parsing:** Identify built-in commands, redirection operators, and pipeline syntax.
4. **Execution:** Run built-ins in the shell process when required, or create child processes for external commands.
5. **File-descriptor management:** Configure input, output, and pipe connections using POSIX file operations.
6. **Process synchronization:** Wait for child processes to finish and collect their exit status.
7. **Error handling:** Report failures without unnecessarily terminating the interactive shell.

### Key system calls

| System call | Purpose |
|---|---|
| `fork()` | Create a child process |
| `execvp()` | Execute an external program using `PATH` lookup |
| `waitpid()` | Wait for child-process completion and collect status |
| `pipe()` | Create a communication channel between processes |
| `dup2()` | Redirect standard input or output |
| `open()` | Open files for input or output redirection |
| `close()` | Release file descriptors |
| `chdir()` | Change the shell's working directory |

## Example Usage

Start the shell:

```bash
./mini-shell
```

Example commands inside the shell:

```bash
pwd
echo "Hello World"
cd /tmp
pwd
```

Redirect output to a file:

```bash
echo hello > output.txt
cat < output.txt
echo world >> output.txt
```

Connect two commands with a pipeline:

```bash
echo hello | wc -c
```

Exit the shell:

```bash
exit
```

The exact output of commands such as `pwd` depends on the environment. External utilities such as `echo`, `cat`, and `wc` must be available on `PATH`.

## Build and Run

### Prerequisites

- Linux or a compatible POSIX environment
- GNU C++ compiler (`g++`) with C++17 support
- GNU Make
- Bash for the automated test script

### Clone the repository

```bash
git clone https://github.com/rdbubster/Unix-shell.git
cd Unix-shell
```

### Compile

```bash
make
```

### Run

```bash
./mini-shell
```

### Run automated tests

```bash
make test
```

### Remove build artifacts

```bash
make clean
```

## Testing and Continuous Integration

The automated test suite covers:

- Built-in commands and external command execution
- Input, output, and append redirection
- Valid pipelines and malformed pipeline syntax
- Missing commands and error reporting
- Quoted input and unterminated quotes
- Exit codes and exit-status handling
- End-of-file behavior and shell recovery
- Repeated command execution
- Pipeline stress testing for hangs and file-descriptor leaks

**Local test result: 40 passed, 0 failed.**

The GitHub Actions workflow automatically builds the shell and executes the test suite. See the [Actions tab](https://github.com/rdbubster/Unix-shell/actions) for workflow results.

## Limitations

This project focuses on core shell and operating-system concepts rather than reproducing all Bash functionality.

- Supports one pipeline operator per command line.
- Does not implement background jobs or job control.
- Does not implement full shell variable expansion or wildcard expansion.
- Does not provide shell history or tab completion.
- Does not aim to support every Bash or POSIX shell feature.

## Technologies

**C++17 · Linux · POSIX system calls · GNU Make · Bash · GitHub Actions**

## Learning Outcomes

This project provides practical experience with:

- Process creation, execution, and synchronization
- Inter-process communication through pipes
- File-descriptor management and I/O redirection
- Command-line parsing and shell control flow
- Error handling and exit-status propagation
- Automated testing and continuous integration

## Repository

Source code, build instructions, tests, and CI configuration are available in this repository:

[github.com/rdbubster/Unix-shell](https://github.com/rdbubster/Unix-shell)
