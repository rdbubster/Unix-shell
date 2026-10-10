#!/usr/bin/env bash
# Automated tests for mini-shell. Exits nonzero if any test fails.
# Usage: bash tests/test_shell.sh ./mini-shell

BIN="$(realpath "${1:-./mini-shell}")"
WORK="$(mktemp -d)"
ERRFILE="$(mktemp)"
trap 'rm -rf "$WORK" "$ERRFILE"' EXIT

PASS=0
FAIL=0

# run "<commands separated by \n>" -> prints the shell's stdout, stderr goes to $ERRFILE.
# timeout guards against hangs (e.g. a pipe end that was never closed).
run() {
    printf '%b' "$1" | (cd "$WORK" && timeout 10 "$BIN" 2>"$ERRFILE")
}

reset_workdir() { find "$WORK" -mindepth 1 -delete; }

check() {  # check <name> <expected> <actual>
    if [ "$2" == "$3" ]; then
        echo "PASS: $1"; PASS=$((PASS + 1))
    else
        echo "FAIL: $1"; echo "  expected: [$2]"; echo "  actual:   [$3]"
        FAIL=$((FAIL + 1))
    fi
}

check_stderr_contains() {  # check_stderr_contains <name> <substring>
    if grep -qF -- "$2" "$ERRFILE"; then
        echo "PASS: $1"; PASS=$((PASS + 1))
    else
        echo "FAIL: $1 (stderr missing '$2'): $(cat "$ERRFILE")"; FAIL=$((FAIL + 1))
    fi
}

# ---- basic execution ----
reset_workdir
check "simple command"   "hello"     "$(run 'echo hello\n')"
check "command arguments" "a b c"    "$(run 'echo a b c\n')"
check "pwd builtin"      "$(cd "$WORK" && pwd -P)" "$(run 'pwd\n')"
check "cd builtin"       "/tmp"      "$(run 'cd /tmp\npwd\n')"
check "blank lines ignored" "ok"     "$(run '\n\n   \necho ok\n')"

# ---- quoting / tokenizer ----
check "double quotes keep spaces" "hello   world" "$(run 'echo "hello   world"\n')"
check "single quotes"    "a|b"       "$(run "echo 'a|b'\n")"
check "operators without spaces" "hi" "$(reset_workdir; run 'echo hi>f\ncat f\n')"

# ---- redirection ----
reset_workdir
check "output redirect"  "hello"     "$(run 'echo hello > out.txt\ncat out.txt\n')"
check "> truncates"      "two"       "$(reset_workdir; run 'echo one > f\necho two > f\ncat f\n')"
check ">> appends"       $'a\nb'     "$(reset_workdir; run 'echo a > f\necho b >> f\ncat f\n')"
reset_workdir; printf 'b\na\n' > "$WORK/in.txt"
check "input redirect"   $'a\nb'     "$(run 'sort < in.txt\n')"
check "input + output redirect" $'a\nb' "$(run 'sort < in.txt > sorted.txt\ncat sorted.txt\n')"
check "builtin with redirect" "$(cd "$WORK" && pwd -P)" "$(run 'pwd > p.txt\ncat p.txt\n')"

# ---- pipes ----
reset_workdir; printf 'b\na\n' > "$WORK/in.txt"
check "pipe"             $'a\nb'     "$(run 'cat in.txt | sort\n')"
check "pipe transforms"  "HELLO"     "$(run 'echo hello | tr a-z A-Z\n')"
check "pipe + output redirect" $'a\nb' "$(run 'cat in.txt | sort > o.txt\ncat o.txt\n')"
check "input redirect + pipe"  $'a\nb' "$(run 'sort < in.txt | cat\n')"
check "builtin in pipe"  "$(cd "$WORK" && pwd -P)" "$(run 'pwd | cat\n')"
check "pipe without spaces" "x"      "$(run 'echo x|cat\n')"

# ---- errors: shell must report and keep running ----
reset_workdir
check "invalid command, shell survives" "still-alive" "$(run 'no_such_cmd\necho still-alive\n')"
check_stderr_contains "invalid command reports error" "no_such_cmd"
check "missing input file, shell survives" "ok" "$(run 'cat < missing.txt\necho ok\n')"
check_stderr_contains "missing input file reports error" "missing.txt"
check "unwritable output, shell survives" "ok" "$(run 'echo x > /no/such/dir/f\necho ok\n')"
check "cd to missing dir, shell survives" "ok" "$(run 'cd /no/such/dir\necho ok\n')"
check_stderr_contains "cd error reported" "cd: /no/such/dir"

# ---- malformed input ----
check "missing redirect filename"  "ok" "$(run 'cat <\necho ok\n')"
check_stderr_contains "missing filename reported" "missing filename"
check "trailing pipe"              "ok" "$(run 'cat |\necho ok\n')"
check "leading pipe"               "ok" "$(run '| cat\necho ok\n')"
check "two pipes rejected"         "ok" "$(run 'echo a | cat | cat\necho ok\n')"
check_stderr_contains "two pipes reported" "only one pipe"
check "unterminated quote"         "ok" "$(run 'echo "oops\necho ok\n')"

# ---- exit status ----
run 'exit 3\n' > /dev/null; check "exit with code" "3" "$?"
run 'exit\n'   > /dev/null; check "exit default"   "0" "$?"
run 'no_such_cmd\nexit\n' > /dev/null 2>&1; check "exit reuses last status (127)" "127" "$?"
run 'echo hi\n' > /dev/null; check "EOF ends shell cleanly" "0" "$?"

# ---- robustness: repeated use, no hangs, no descriptor leaks ----
reset_workdir
many_pwd="$(for _ in $(seq 50); do printf 'pwd\n'; done)"
check "50 repeated commands" "50" "$(run "$many_pwd\n" | wc -l | tr -d ' ')"

many_pipes="$(for _ in $(seq 300); do printf 'echo x | cat\n'; done)"
# ulimit -n 30: if the shell leaked a descriptor per pipeline it would die long before 300.
out="$(printf '%s\n' "$many_pipes" | (cd "$WORK"; ulimit -n 30; timeout 20 "$BIN" 2>/dev/null) | wc -l | tr -d ' ')"
check "300 pipelines, no fd leak or hang" "300" "$out"

echo
echo "Passed: $PASS   Failed: $FAIL"
[ "$FAIL" -eq 0 ]
