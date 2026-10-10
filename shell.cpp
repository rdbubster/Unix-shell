// shell.cpp - Mini Unix Shell
// Features: external commands (fork/execvp/waitpid), builtins (cd, pwd, exit),
// I/O redirection (<, >, >>), one pipe (cmd1 | cmd2), quote-aware tokenizer.

#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace std;

// ---------- Data types ----------

struct Token {
    string text;
    bool isOp;  // true for unquoted |, <, >, >>
};

struct Command {
    vector<string> args;
    string inFile;
    string outFile;
    bool append = false;  // true for ">>"
};

static int lastStatus = 0;

static void report(const string& msg) {
    cerr << "mini-shell: " << msg << "\n";
}

// ---------- Tokenizer ----------
// Splits a line into words and operators. Handles 'single' and "double"
// quotes, and recognises operators even without surrounding spaces (ls>out).
static bool tokenize(const string& line, vector<Token>& out, string& err) {
    string cur;
    bool inToken = false;
    size_t i = 0, n = line.size();

    auto flushWord = [&]() {
        if (inToken) {
            out.push_back({cur, false});
            cur.clear();
            inToken = false;
        }
    };

    while (i < n) {
        char c = line[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            flushWord();
            i++;
        } else if (c == '\'' || c == '"') {
            char quote = c;
            i++;
            inToken = true;
            while (i < n && line[i] != quote) cur += line[i++];
            if (i >= n) { err = "syntax error: unterminated quote"; return false; }
            i++;  // skip closing quote
        } else if (c == '|' || c == '<') {
            flushWord();
            out.push_back({string(1, c), true});
            i++;
        } else if (c == '>') {
            flushWord();
            if (i + 1 < n && line[i + 1] == '>') {
                out.push_back({">>", true});
                i += 2;
            } else {
                out.push_back({">", true});
                i++;
            }
        } else {
            cur += c;
            inToken = true;
            i++;
        }
    }
    flushWord();
    return true;
}

// ---------- Parser ----------
// Turns tokens[begin, end) into one Command (args + redirections).
static bool parseCommand(const vector<Token>& t, size_t begin, size_t end,
                         Command& cmd, string& err) {
    for (size_t i = begin; i < end; i++) {
        if (!t[i].isOp) {
            cmd.args.push_back(t[i].text);
            continue;
        }
        const string& op = t[i].text;
        if (i + 1 >= end || t[i + 1].isOp) {
            err = "syntax error: missing filename after '" + op + "'";
            return false;
        }
        const string& name = t[++i].text;
        if (op == "<") {
            cmd.inFile = name;
        } else {
            cmd.outFile = name;
            cmd.append = (op == ">>");
        }
    }
    if (cmd.args.empty()) {
        err = "syntax error: empty command";
        return false;
    }
    return true;
}

// Splits on a single '|'. Fills cmd1, and cmd2 if a pipe exists.
static bool parseLine(const vector<Token>& t, Command& cmd1, Command& cmd2,
                      bool& hasPipe, string& err) {
    vector<size_t> pipes;
    for (size_t i = 0; i < t.size(); i++)
        if (t[i].isOp && t[i].text == "|") pipes.push_back(i);

    if (pipes.size() > 1) {
        err = "only one pipe is supported";
        return false;
    }
    if (pipes.empty()) {
        hasPipe = false;
        return parseCommand(t, 0, t.size(), cmd1, err);
    }
    hasPipe = true;
    return parseCommand(t, 0, pipes[0], cmd1, err) &&
           parseCommand(t, pipes[0] + 1, t.size(), cmd2, err);
}

// ---------- Builtins ----------

static bool isBuiltin(const Command& c) {
    const string& a = c.args[0];
    return a == "cd" || a == "pwd" || a == "exit";
}

// cd and exit must run in the shell process itself: a forked child would
// change its own directory / exit itself and the shell would never notice.
static bool mustRunInParent(const Command& c) {
    return c.args[0] == "cd" || c.args[0] == "exit";
}

static int runBuiltin(const Command& c, bool& shouldExit) {
    const string& name = c.args[0];

    if (name == "exit") {
        shouldExit = true;
        return c.args.size() > 1 ? atoi(c.args[1].c_str()) : lastStatus;
    }
    if (name == "pwd") {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) {
            report(string("pwd: ") + strerror(errno));
            return 1;
        }
        cout << cwd << "\n";
        return 0;
    }
    // cd
    const char* target = nullptr;
    if (c.args.size() > 1) {
        target = c.args[1].c_str();
    } else {
        target = getenv("HOME");
        if (!target) { report("cd: HOME not set"); return 1; }
    }
    if (chdir(target) != 0) {
        report(string("cd: ") + target + ": " + strerror(errno));
        return 1;
    }
    return 0;
}

// ---------- Execution ----------

// Runs in the CHILD, after fork() and before exec(). exec keeps open file
// descriptors, so whatever stdin/stdout point to here is what the new program
// inherits.
static void applyRedirection(const Command& c) {
    if (!c.inFile.empty()) {
        int fd = open(c.inFile.c_str(), O_RDONLY);
        if (fd < 0) {
            report(c.inFile + ": " + strerror(errno));
            _exit(1);
        }
        if (dup2(fd, STDIN_FILENO) < 0) { report(strerror(errno)); _exit(1); }
        close(fd);
    }
    if (!c.outFile.empty()) {
        int flags = O_WRONLY | O_CREAT | (c.append ? O_APPEND : O_TRUNC);
        int fd = open(c.outFile.c_str(), flags, 0644);
        if (fd < 0) {
            report(c.outFile + ": " + strerror(errno));
            _exit(1);
        }
        if (dup2(fd, STDOUT_FILENO) < 0) { report(strerror(errno)); _exit(1); }
        close(fd);
    }
}

// Everything a child process does after fork(). Never returns.
[[noreturn]] static void runChild(const Command& c) {
    signal(SIGINT, SIG_DFL);  // the shell ignores Ctrl+C, its children must not
    applyRedirection(c);

    if (isBuiltin(c)) {  // e.g. "pwd > file" or "pwd | cat"
        bool ignored = false;
        int st = runBuiltin(c, ignored);
        cout.flush();
        _exit(st);
    }

    vector<char*> argv;
    for (const string& s : c.args) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());
    // Only reached if exec failed.
    report(c.args[0] + ": " + strerror(errno));
    _exit(errno == ENOENT ? 127 : 126);
}

// Waits for pid (retrying if interrupted by a signal) and decodes the status.
static int waitFor(pid_t pid) {
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) { report(string("waitpid: ") + strerror(errno)); return 1; }
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

static int runSingle(const Command& c) {
    cout.flush();  // don't let buffered output be duplicated into the child
    pid_t pid = fork();
    if (pid < 0) { report(string("fork: ") + strerror(errno)); return 1; }
    if (pid == 0) runChild(c);
    return waitFor(pid);
}

// cmd1 | cmd2. pipe() gives fds[0] = read end, fds[1] = write end. Every copy
// of each end that a process doesn't use MUST be closed (in both children and
// in the parent), otherwise cmd2 never sees EOF and the pipeline hangs.
static int runPipeline(const Command& a, const Command& b) {
    int fds[2];
    if (pipe(fds) < 0) { report(string("pipe: ") + strerror(errno)); return 1; }

    cout.flush();
    pid_t p1 = fork();
    if (p1 < 0) {
        report(string("fork: ") + strerror(errno));
        close(fds[0]); close(fds[1]);
        return 1;
    }
    if (p1 == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]); close(fds[1]);
        runChild(a);
    }

    pid_t p2 = fork();
    if (p2 < 0) {
        report(string("fork: ") + strerror(errno));
        close(fds[0]); close(fds[1]);
        waitFor(p1);
        return 1;
    }
    if (p2 == 0) {
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]); close(fds[1]);
        runChild(b);
    }

    close(fds[0]);
    close(fds[1]);
    waitFor(p1);
    return waitFor(p2);  // like bash, the pipeline's status is the last command's
}

// ---------- REPL ----------

int main() {
    signal(SIGINT, SIG_IGN);  // Ctrl+C should not kill the shell itself
    const bool interactive = isatty(STDIN_FILENO);
    string line;

    while (true) {
        if (interactive) { cout << "mini-shell> " << flush; }
        if (!getline(cin, line)) {
            if (interactive) cout << "\n";
            break;  // EOF (Ctrl+D)
        }

        vector<Token> tokens;
        string err;
        if (!tokenize(line, tokens, err)) { report(err); lastStatus = 2; continue; }
        if (tokens.empty()) continue;  // blank line

        Command c1, c2;
        bool hasPipe = false;
        if (!parseLine(tokens, c1, c2, hasPipe, err)) { report(err); lastStatus = 2; continue; }

        if (hasPipe) {
            lastStatus = runPipeline(c1, c2);
        } else if (mustRunInParent(c1)) {
            bool shouldExit = false;
            lastStatus = runBuiltin(c1, shouldExit);
            if (shouldExit) { cout.flush(); return lastStatus; }
        } else {
            lastStatus = runSingle(c1);
        }
    }
    return lastStatus;
}
