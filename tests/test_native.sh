#!/usr/bin/env bash
set -e

# Path to minishell binary
SHELL_BIN_ARG="${1:-./bin/minishell}"

# Resolve SHELL_BIN to absolute path
if [[ "$SHELL_BIN_ARG" = /* ]]; then
    SHELL_BIN="$SHELL_BIN_ARG"
else
    SHELL_BIN="$(pwd)/$SHELL_BIN_ARG"
fi

if [ ! -x "$SHELL_BIN" ]; then
    echo "Error: minishell binary '$SHELL_BIN' not found or not executable"
    exit 1
fi

echo "=== Running Native MiniShell Integration Tests ==="
echo "Testing binary: $SHELL_BIN"

# Create temporary directory for isolated tests
TEST_DIR=$(mktemp -d /tmp/minishell_test_XXXXXX)
trap 'rm -rf "$TEST_DIR"' EXIT
cd "$TEST_DIR"

TOTAL=0
PASSED=0

assert_success() {
    local desc="$1"
    local cmd="$2"
    TOTAL=$((TOTAL + 1))
    echo "Test $TOTAL: $desc"
    if "$SHELL_BIN" -c "$cmd" > test_stdout.log 2> test_stderr.log; then
        echo "  [PASS]"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] Command failed unexpectedly: $cmd"
        cat test_stderr.log
    fi
}

assert_output_contains() {
    local desc="$1"
    local cmd="$2"
    local expected="$3"
    TOTAL=$((TOTAL + 1))
    echo "Test $TOTAL: $desc"
    "$SHELL_BIN" -c "$cmd" > test_stdout.log 2> test_stderr.log || true
    if grep -q "$expected" test_stdout.log; then
        echo "  [PASS]"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] Expected '$expected' not found in stdout:"
        cat test_stdout.log
        cat test_stderr.log
    fi
}

assert_exit_code() {
    local desc="$1"
    local cmd="$2"
    local expected_code="$3"
    TOTAL=$((TOTAL + 1))
    echo "Test $TOTAL: $desc"
    set +e
    "$SHELL_BIN" -c "$cmd" > test_stdout.log 2> test_stderr.log
    local code=$?
    set -e
    if [ "$code" -eq "$expected_code" ]; then
        echo "  [PASS] Exit code $code as expected"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] Expected exit code $expected_code, got $code"
        cat test_stderr.log
    fi
}

# 1. PRD Core verification suite
assert_output_contains "pwd prints current directory" "pwd" "$TEST_DIR"

assert_output_contains "Pipeline with wc -l" "printf 'alpha\nbeta\n' | wc -l" "2"

assert_success "Output redirection >" "echo hello > output.txt"
assert_output_contains "Input redirection <" "cat < output.txt" "hello"

assert_success "Append redirection >>" "echo second >> output.txt"
assert_output_contains "Verify append content" "cat output.txt" "second"

assert_exit_code "true returns 0" "true" 0
assert_exit_code "false returns 1" "false" 1

# 2. Command not found
assert_exit_code "Command not found returns 127" "non_existent_command_xyz" 127

# 3. cd standalone vs in pipeline
TOTAL=$((TOTAL + 1))
echo "Test $TOTAL: Standalone cd changes directory in parent"
mkdir -p "$TEST_DIR/subdir"
"$SHELL_BIN" -c "cd subdir; pwd" > test_stdout.log 2> test_stderr.log
if grep -q "$TEST_DIR/subdir" test_stdout.log; then
    echo "  [PASS]"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Standalone cd did not change parent directory"
fi

TOTAL=$((TOTAL + 1))
echo "Test $TOTAL: cd in pipeline does NOT change parent directory (PRD 4.2 & 9.2)"
"$SHELL_BIN" -c "cd subdir | echo pipeline; pwd" > test_stdout.log 2> test_stderr.log
# In the above, pwd should print TEST_DIR, NOT TEST_DIR/subdir!
if grep -q "$TEST_DIR/subdir" test_stdout.log; then
    echo "  [FAIL] cd in pipeline incorrectly modified parent directory state!"
else
    echo "  [PASS] cd in pipeline remained isolated to child process"
    PASSED=$((PASSED + 1))
fi

# 4. Multi-stage pipelines and descriptor leak check
assert_output_contains "Multi-stage pipeline (3 stages)" "echo -e 'c\na\nb' | sort | head -n 1" "a"

# 5. Stderr redirection
TOTAL=$((TOTAL + 1))
echo "Test $TOTAL: Stderr redirection 2>"
"$SHELL_BIN" -c "ls /non_existent_dir_12345 2> err.txt" || true
if [ -s err.txt ]; then
    echo "  [PASS] Stderr redirected to file"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Stderr redirection 2> failed"
fi

# 6. Semicolon sequential execution
assert_output_contains "Semicolon sequential execution" "echo first ; echo second" "second"

# 7. Built-in export and env
assert_output_contains "Export and echo variable" "export MY_FOO=bar123 ; echo \$MY_FOO" "bar123"

# 8. Type command
assert_output_contains "Type built-in check" "type cd" "cd is a shell builtin"
assert_output_contains "Type external command check" "type ls" "ls is"

# 9. Exit with custom code
assert_exit_code "Exit command with custom code 42" "exit 42" 42

echo ""
echo "=========================================="
echo "Integration Tests: $PASSED / $TOTAL passed"
echo "=========================================="

if [ "$PASSED" -eq "$TOTAL" ]; then
    echo "ALL NATIVE INTEGRATION TESTS PASSED!"
    exit 0
else
    echo "SOME TESTS FAILED!"
    exit 1
fi
