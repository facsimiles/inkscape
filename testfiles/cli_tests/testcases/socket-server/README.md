# Socket Server Tests

This directory contains CLI tests for the Inkscape socket server functionality.

## Test Files

### `test-document.svg`
A simple test SVG document used for socket server operations.

### `socket_test_client.py`
A comprehensive Python test client that tests:
- Connection to socket server
- Command execution
- Response parsing
- Multiple command sequences

### `socket_simple_test.py`
A simplified test script for basic socket functionality testing.

### `test_socket_startup.sh`
A shell script that tests:
- Socket server startup
- Port availability checking
- Basic connectivity
- Integration with Python test client

### `socket_integration_test.py`
A comprehensive integration test that covers:
- Server startup and shutdown
- Connection handling
- Command execution
- Error handling
- Multiple command sequences

## Running Tests

### Manual Testing
```bash
# Start Inkscape with socket server
inkscape --socket=8080 --without-gui &

# Run Python test client
python3 socket_test_client.py 8080

# Run shell test script
./test_socket_startup.sh

# Run simple test
python3 socket_simple_test.py 8080
```

### Automated Testing
The tests are integrated into the Inkscape test suite and can be run with:
```bash
ninja check
```

## Test Coverage

The socket server tests cover:

1. **CLI Integration Tests**
   - `--socket=PORT` command line option
   - Invalid port number handling
   - Server startup verification

2. **Socket Protocol Tests**
   - Connection establishment
   - Welcome message handling
   - Command format validation
   - Response parsing

3. **Command Execution Tests**
   - Basic command execution
   - Multiple command sequences
   - Error handling for invalid commands
   - Status and action-list commands

4. **Integration Tests**
   - End-to-end socket communication
   - Server lifecycle management
   - Cross-platform compatibility

## Expected Behavior

### Successful Tests
- Socket server starts on specified port
- Client can connect and receive welcome message
- Commands are executed and responses are received
- Error conditions are handled gracefully

### Failure Conditions
- Invalid port numbers (0, negative, >65535, non-numeric)
- Port already in use
- Connection timeouts
- Invalid command formats
- Server startup failures

## Dependencies

- Python 3.x
- Socket support (standard library)
- Inkscape with socket server support
- Netcat (optional, for additional testing)

## Notes

- Tests use port 8080 by default
- All tests bind to localhost (127.0.0.1) only
- Tests include proper cleanup of resources
- Timeout values are set to prevent hanging tests 