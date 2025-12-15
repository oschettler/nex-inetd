#!/bin/bash

# Test suite for nexd server
# This script tests the Nex protocol server functionality

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Print test result
pass() {
    echo -e "${GREEN}✓ PASS:${NC} $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
    echo -e "${RED}✗ FAIL:${NC} $1"
    echo -e "  Expected: $2"
    echo -e "  Got: $3"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

info() {
    echo -e "${YELLOW}ℹ INFO:${NC} $1"
}

# Setup test environment
setup() {
    info "Setting up test environment..."
    
    # Create test directory
    TEST_DIR="test_data"
    rm -rf "$TEST_DIR"
    mkdir -p "$TEST_DIR"
    
    # Create test files
    echo "Hello, Nex!" > "$TEST_DIR/hello.txt"
    echo "<html><body>Test HTML</body></html>" > "$TEST_DIR/index.html"
    echo "# Gemini Test" > "$TEST_DIR/test.gmi"
    echo '{"test": "json"}' > "$TEST_DIR/data.json"
    
    # Create subdirectory with files
    mkdir -p "$TEST_DIR/subdir"
    echo "Subdirectory file" > "$TEST_DIR/subdir/file.txt"
    echo "Another file" > "$TEST_DIR/subdir/another.txt"
    
    # Create nested directory
    mkdir -p "$TEST_DIR/subdir/nested"
    echo "Nested content" > "$TEST_DIR/subdir/nested/deep.txt"
    
    # Create file with spaces in name
    echo "File with spaces" > "$TEST_DIR/test file.txt"
    
    info "Test environment ready"
}

# Clean up test environment
cleanup() {
    info "Cleaning up test environment..."
    rm -rf "$TEST_DIR"
}

# Run a single test
run_test() {
    local test_name="$1"
    local request="$2"
    local expected_status="$3"
    local check_content="$4"
    
    TESTS_RUN=$((TESTS_RUN + 1))
    
    # Run the server with the request
    local output=$(echo -ne "$request\r\n" | ./nexd "$TEST_DIR" 2>/dev/null)
    local status_line=$(echo "$output" | head -n1)
    local status_code=$(echo "$status_line" | cut -d' ' -f1)
    local content=$(echo "$output" | tail -n +2)
    
    # Check status code
    if [ "$status_code" != "$expected_status" ]; then
        fail "$test_name" "Status $expected_status" "Status $status_code"
        return 1
    fi
    
    # Check content if specified
    if [ -n "$check_content" ]; then
        if echo "$content" | grep -q "$check_content"; then
            pass "$test_name"
            return 0
        else
            fail "$test_name" "Content containing '$check_content'" "Content not found"
            return 1
        fi
    fi
    
    pass "$test_name"
    return 0
}

# Run all tests
run_tests() {
    info "Running tests..."
    echo
    
    # Test 1: Serve a simple text file
    run_test "Serve text file" "/hello.txt" "2" "Hello, Nex!"
    
    # Test 2: Serve HTML file (check MIME type)
    local html_test=$(echo -ne "/index.html\r\n" | ./nexd "$TEST_DIR" 2>/dev/null | head -n1)
    TESTS_RUN=$((TESTS_RUN + 1))
    if echo "$html_test" | grep -q "2 text/html"; then
        pass "Serve HTML file with correct MIME type"
    else
        fail "Serve HTML file with correct MIME type" "2 text/html" "$html_test"
    fi
    
    # Test 3: Serve Gemini file
    local gmi_test=$(echo -ne "/test.gmi\r\n" | ./nexd "$TEST_DIR" 2>/dev/null | head -n1)
    TESTS_RUN=$((TESTS_RUN + 1))
    if echo "$gmi_test" | grep -q "2 text/gemini"; then
        pass "Serve Gemini file with correct MIME type"
    else
        fail "Serve Gemini file with correct MIME type" "2 text/gemini" "$gmi_test"
    fi
    
    # Test 4: Serve JSON file
    local json_test=$(echo -ne "/data.json\r\n" | ./nexd "$TEST_DIR" 2>/dev/null | head -n1)
    TESTS_RUN=$((TESTS_RUN + 1))
    if echo "$json_test" | grep -q "2 application/json"; then
        pass "Serve JSON file with correct MIME type"
    else
        fail "Serve JSON file with correct MIME type" "2 application/json" "$json_test"
    fi
    
    # Test 5: Directory listing (root)
    run_test "Directory listing (root)" "/" "2" "hello.txt"
    
    # Test 6: Directory listing (subdirectory)
    run_test "Directory listing (subdirectory)" "/subdir" "2" "file.txt"
    
    # Test 7: Directory listing shows subdirectories with trailing slash
    local dir_test=$(echo -ne "/subdir\r\n" | ./nexd "$TEST_DIR" 2>/dev/null)
    TESTS_RUN=$((TESTS_RUN + 1))
    if echo "$dir_test" | grep -q "nested/"; then
        pass "Directory listing shows subdirectories with /"
    else
        fail "Directory listing shows subdirectories with /" "nested/" "Not found"
    fi
    
    # Test 8: File in subdirectory
    run_test "File in subdirectory" "/subdir/file.txt" "2" "Subdirectory file"
    
    # Test 9: Nested directory file
    run_test "Nested directory file" "/subdir/nested/deep.txt" "2" "Nested content"
    
    # Test 10: Non-existent file (404)
    run_test "Non-existent file" "/nonexistent.txt" "5" ""
    
    # Test 11: Non-existent directory
    run_test "Non-existent directory" "/nosuchdir" "5" ""
    
    # Test 12: Directory traversal attempt (../)
    run_test "Directory traversal attack (../)" "/../etc/passwd" "5" ""
    
    # Test 13: Directory traversal in subdirectory
    run_test "Directory traversal attack (subdir/../..)" "/subdir/../../etc/passwd" "5" ""
    
    # Test 14: Directory listing doesn't show hidden files
    touch "$TEST_DIR/.hidden"
    local hidden_test=$(echo -ne "/\r\n" | ./nexd "$TEST_DIR" 2>/dev/null)
    TESTS_RUN=$((TESTS_RUN + 1))
    if ! echo "$hidden_test" | grep -q ".hidden"; then
        pass "Directory listing hides hidden files"
    else
        fail "Directory listing hides hidden files" "No .hidden" ".hidden found"
    fi
    rm "$TEST_DIR/.hidden"
    
    # Test 15: Empty path (serves root directory)
    run_test "Empty path serves root" "" "2" "hello.txt"
    
    # Test 16: Root path
    run_test "Root path" "/" "2" "hello.txt"
    
    # Test 17: Path without leading slash
    run_test "Path without leading slash" "hello.txt" "2" "Hello, Nex!"
    
    # Test 18: Directory listing is sorted
    local sort_test=$(echo -ne "/subdir\r\n" | ./nexd "$TEST_DIR" 2>/dev/null | tail -n +2)
    TESTS_RUN=$((TESTS_RUN + 1))
    local first_entry=$(echo "$sort_test" | head -n1)
    if echo "$first_entry" | grep -q "another.txt"; then
        pass "Directory listing is sorted alphabetically"
    else
        fail "Directory listing is sorted alphabetically" "another.txt first" "$first_entry"
    fi
}

# Main execution
main() {
    echo "========================================"
    echo "  Nex Server Test Suite"
    echo "========================================"
    echo
    
    # Check if nexd binary exists
    if [ ! -f "./nexd" ]; then
        echo -e "${RED}ERROR:${NC} nexd binary not found. Run 'make' first."
        exit 1
    fi
    
    setup
    run_tests
    cleanup
    
    echo
    echo "========================================"
    echo "  Test Results"
    echo "========================================"
    echo "Tests run:    $TESTS_RUN"
    echo -e "${GREEN}Tests passed: $TESTS_PASSED${NC}"
    if [ $TESTS_FAILED -gt 0 ]; then
        echo -e "${RED}Tests failed: $TESTS_FAILED${NC}"
        exit 1
    else
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    fi
}

main
