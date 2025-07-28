#!/bin/bash
# Test script for Inkscape socket server startup and basic functionality

set -e

# Configuration
PORT=8080
TEST_DOCUMENT="test-document.svg"
PYTHON_SCRIPT="socket_test_client.py"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to cleanup background processes
cleanup() {
    print_status "Cleaning up..."
    if [ ! -z "$INKSCAPE_PID" ]; then
        kill $INKSCAPE_PID 2>/dev/null || true
    fi
    # Wait a bit for port to be released
    sleep 2
}

# Set up cleanup on script exit
trap cleanup EXIT

# Test 1: Check if port is available
print_status "Test 1: Checking if port $PORT is available..."
if lsof -i :$PORT >/dev/null 2>&1; then
    print_error "Port $PORT is already in use"
    exit 1
fi
print_status "Port $PORT is available"

# Test 2: Start Inkscape with socket server
print_status "Test 2: Starting Inkscape with socket server on port $PORT..."
inkscape --socket=$PORT --without-gui &
INKSCAPE_PID=$!

# Wait for Inkscape to start and socket to be ready
print_status "Waiting for socket server to start..."
sleep 5

# Test 3: Check if socket server is listening
print_status "Test 3: Checking if socket server is listening..."
if ! lsof -i :$PORT >/dev/null 2>&1; then
    print_error "Socket server is not listening on port $PORT"
    exit 1
fi
print_status "Socket server is listening on port $PORT"

# Test 4: Run Python test client
print_status "Test 4: Running socket test client..."
if [ -f "$PYTHON_SCRIPT" ]; then
    python3 "$PYTHON_SCRIPT" $PORT
    if [ $? -eq 0 ]; then
        print_status "Socket test client passed"
    else
        print_error "Socket test client failed"
        exit 1
    fi
else
    print_warning "Python test script not found, skipping client test"
fi

# Test 5: Test with netcat (if available)
print_status "Test 5: Testing with netcat..."
if command -v nc >/dev/null 2>&1; then
    echo "COMMAND:test5:status" | nc -w 5 127.0.0.1 $PORT | grep -q "RESPONSE" && {
        print_status "Netcat test passed"
    } || {
        print_error "Netcat test failed"
        exit 1
    }
else
    print_warning "Netcat not available, skipping netcat test"
fi

print_status "All socket server tests passed!" 