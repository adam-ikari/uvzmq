#!/bin/bash
# End-to-End tests for UVZMQ

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build/examples"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Test function
run_test() {
    local test_name=$1
    local server_cmd=$2
    local client_cmd=$3
    local timeout=${4:-5}

    TESTS_RUN=$((TESTS_RUN + 1))
    log_info "Running test: $test_name"

    # Start server in background
    eval "$server_cmd" > /tmp/e2e_server_$$.log 2>&1 &
    local server_pid=$!

    # Wait for server to start
    sleep 1

    # Run client
    set +e
    eval "$client_cmd" > /tmp/e2e_client_$$.log 2>&1
    local client_exit_code=$?
    set -e

    # Wait for server to finish (with timeout)
    local count=0
    while kill -0 $server_pid 2>/dev/null; do
        sleep 0.1
        count=$((count + 1))
        if [ $count -gt $((timeout * 10)) ]; then
            log_warn "Server timeout, killing..."
            kill $server_pid 2>/dev/null || true
            break
        fi
    done

    wait $server_pid 2>/dev/null || true

    # Check results
    if [ $client_exit_code -eq 0 ]; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_info "✓ Test passed: $test_name"
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_error "✗ Test failed: $test_name"
        echo "Server log:"
        cat /tmp/e2e_server_$$.log
        echo "Client log:"
        cat /tmp/e2e_client_$$.log
    fi

    # Cleanup
    rm -f /tmp/e2e_server_$$.log /tmp/e2e_client_$$.log
}

# Test 1: Simple REQ/REP test
test_req_rep() {
    # This test requires a simple server implementation
    # For now, we'll just test that the client can be built and run
    log_info "Test: REQ/REP (build verification)"
    if [ -f "$BUILD_DIR/req_rep" ]; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_info "✓ req_rep executable exists"
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_error "✗ req_rep executable not found"
    fi
}

# Test 2: PUB/SUB test
test_pub_sub() {
    log_info "Test: PUB/SUB (build verification)"
    if [ -f "$BUILD_DIR/pub_sub" ]; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_info "✓ pub_sub executable exists"
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_error "✗ pub_sub executable not found"
    fi
}

# Test 3: PUSH/PULL test
test_push_pull() {
    log_info "Test: PUSH/PULL (build verification)"
    if [ -f "$BUILD_DIR/test_push_pull" ]; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_info "✓ test_push_pull executable exists"
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_error "✗ test_push_pull executable not found"
    fi
}

# Test 4: Multi-threaded test
test_multi_thread() {
    log_info "Test: Multi-threaded (build verification)"
    if [ -f "$BUILD_DIR/multi_thread" ]; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_info "✓ multi_thread executable exists"
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_error "✗ multi_thread executable not found"
    fi
}

# Test 5: API compatibility test
test_api_compatibility() {
    log_info "Test: API compatibility"

    # Test that uvzmq_socket_new accepts 5 parameters
    cat > /tmp/api_test_$$.c << 'EOF'
#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"
#include <zmq.h>
#include <uv.h>

void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    zmq_msg_close(msg);
}

int main(void) {
    uv_loop_t loop;
    uv_loop_init(&loop);
    void* zmq_sock = zmq_socket(zmq_ctx_new(), ZMQ_SUB);
    uvzmq_socket_t* socket = NULL;

    // Test new 5-parameter API
    int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &socket);
    if (rc == 0 && socket != NULL) {
        uvzmq_socket_free(socket);
        zmq_close(zmq_sock);
        uv_loop_close(&loop);
        return 0;
    }
    return 1;
}
EOF

    set +e
    g++ -o /tmp/api_test_$$ /tmp/api_test_$$.c \
        -I"$PROJECT_ROOT/include" \
        -I"$PROJECT_ROOT/third_party/libuv/include" \
        -I"$PROJECT_ROOT/third_party/zmq/include" \
        -L"$PROJECT_ROOT/build/third_party/libuv" \
        -L"$PROJECT_ROOT/build/third_party/zmq/lib" \
        -lzmq -luv \
        -lpthread -lstdc++ \
        -Wl,-rpath,"$PROJECT_ROOT/build/third_party/libuv" \
        -Wl,-rpath,"$PROJECT_ROOT/build/third_party/zmq/lib" \
        2>&1
    local compile_result=$?
    set -e

    if [ $compile_result -eq 0 ]; then
        /tmp/api_test_$$
        local run_result=$?
        if [ $run_result -eq 0 ]; then
            TESTS_PASSED=$((TESTS_PASSED + 1))
            log_info "✓ API compatibility test passed"
        else
            TESTS_FAILED=$((TESTS_FAILED + 1))
            log_error "✗ API compatibility test failed at runtime"
        fi
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_error "✗ API compatibility test failed to compile"
    fi

    rm -f /tmp/api_test_$$ /tmp/api_test_$$.c
}

# Test 6: Error handling test
test_error_handling() {
    log_info "Test: Error handling"

    cat > /tmp/error_test_$$.c << 'EOF'
#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"
#include <zmq.h>
#include <uv.h>

void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    zmq_msg_close(msg);
}

int main(void) {
    uv_loop_t loop;
    uv_loop_init(&loop);

    // Test NULL parameters return error
    uvzmq_socket_t* socket = NULL;
    int rc = uvzmq_socket_new(NULL, NULL, on_recv, NULL, &socket);
    if (rc != 0) {
        uv_loop_close(&loop);
        return 0;  // Expected to fail
    }
    return 1;  // Should have failed
}
EOF

    set +e
    g++ -o /tmp/error_test_$$ /tmp/error_test_$$.c \
        -I"$PROJECT_ROOT/include" \
        -I"$PROJECT_ROOT/third_party/libuv/include" \
        -I"$PROJECT_ROOT/third_party/zmq/include" \
        -L"$PROJECT_ROOT/build/third_party/libuv" \
        -L"$PROJECT_ROOT/build/third_party/zmq/lib" \
        -lzmq -luv \
        -lpthread -lstdc++ \
        -Wl,-rpath,"$PROJECT_ROOT/build/third_party/libuv" \
        -Wl,-rpath,"$PROJECT_ROOT/build/third_party/zmq/lib" \
        2>&1
    local compile_result=$?
    set -e

    if [ $compile_result -eq 0 ]; then
        /tmp/error_test_$$
        local run_result=$?
        if [ $run_result -eq 0 ]; then
            TESTS_PASSED=$((TESTS_PASSED + 1))
            log_info "✓ Error handling test passed"
        else
            TESTS_FAILED=$((TESTS_FAILED + 1))
            log_error "✗ Error handling test failed"
        fi
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_error "✗ Error handling test failed to compile"
    fi

    rm -f /tmp/error_test_$$ /tmp/error_test_$$.c
}

# Main execution
main() {
    log_info "Starting UVZMQ End-to-End Tests"
    log_info "Build directory: $BUILD_DIR"
    echo ""

    # Check if build directory exists
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found. Please run 'make build' first."
        exit 1
    fi

    # Run tests
    test_req_rep
    test_pub_sub
    test_push_pull
    test_multi_thread
    test_api_compatibility
    test_error_handling

    # Print summary
    echo ""
    log_info "======================================"
    log_info "Test Summary"
    log_info "======================================"
    echo "Total tests: $TESTS_RUN"
    echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
    echo -e "${RED}Failed: $TESTS_FAILED${NC}"
    echo ""

    if [ $TESTS_FAILED -eq 0 ]; then
        log_info "All tests passed!"
        exit 0
    else
        log_error "Some tests failed!"
        exit 1
    fi
}

# Run main
main