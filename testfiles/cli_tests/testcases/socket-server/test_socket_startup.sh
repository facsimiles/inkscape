# SPDX-License-Identifier: GPL-2.0-or-later
#
# Author: Tod Schmidt
# Copyright: 2025
#

#!/bin/bash
# Test script for Inkscape socket server startup and basic functionality

set -e

# Configuration
PORT=8080
TEST_DOCUMENT="test-document.svg"
PYTHON_SCRIPT="socket_test_client.py"
MAX_WAIT_TIME=30  # Maximum time to wait for server to start

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

# Function to check if port is in use (works without lsof)
check_port_available() {
    local port=$1
    if command -v lsof >/dev/null 2>&1; then
        lsof -i :$port >/dev/null 2>&1
        return $?
    else
        # Fallback: try to bind to the port
        timeout 1 bash -c "echo >/dev/tcp/127.0.0.1/$port" 2>/dev/null
        return $?
    fi
}

# Function to check if port is listening (works without lsof)
check_port_listening() {
    local port=$1
    if command -v lsof >/dev/null 2>&1; then
        lsof -i :$port >/dev/null 2>&1
        return $?
    else
        # Fallback: try to connect to the port
        timeout 1 bash -c "echo >/dev/tcp/127.0.0.1/$port" 2>/dev/null
        return $?
    fi
}

# Function to cleanup background processes
cleanup() {
    print_status "Cleaning up..."
    if [ ! -z "$INKSCAPE_PID" ]; then
        kill $INKSCAPE_PID 2>/dev/null || true
        wait $INKSCAPE_PID 2>/dev/null || true
    fi
    # Wait a bit for port to be released
    sleep 2
}

# Set up cleanup on script exit
trap cleanup EXIT

# Test 1: Check if port is available
print_status "Test 1: Checking if port $PORT is available..."
if check_port_available $PORT; then
    print_error "Port $PORT is already in use"
    exit 1
fi
print_status "Port $PORT is available"

# Test 2: Start Inkscape with socket server
print_status "Test 2: Starting Inkscape with socket server on port $PORT..."
inkscape --socket=$PORT --without-gui &
INKSCAPE_PID=$!

# Wait for Inkscape to start and socket to be ready
print_status "Waiting for socket server to start (max $MAX_WAIT_TIME seconds)..."
wait_time=0
while [ $wait_time -lt $MAX_WAIT_TIME ]; do
    if check_port_listening $PORT; then
        print_status "Socket server is listening on port $PORT"
        break
    fi
    sleep 1
    wait_time=$((wait_time + 1))
done

if [ $wait_time -ge $MAX_WAIT_TIME ]; then
    print_error "Socket server failed to start within $MAX_WAIT_TIME seconds"
    exit 1
fi

# Test 3: Run Python test client
print_status "Test 3: Running socket test client..."
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

# Test 4: Test with netcat (if available)
print_status "Test 4: Testing with netcat..."
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