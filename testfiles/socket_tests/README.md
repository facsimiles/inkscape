# Socket Protocol Tests

This directory contains comprehensive tests for the Inkscape socket server protocol implementation.

## Overview

The socket server provides a TCP-based interface for remote command execution in Inkscape. These tests validate the protocol implementation, command parsing, response formatting, and end-to-end functionality.

## Test Structure

### Test Files

- **`test_socket_protocol.cpp`** - Core protocol parsing and validation tests
- **`test_socket_commands.cpp`** - Command parsing and validation tests
- **`test_socket_responses.cpp`** - Response formatting and validation tests
- **`test_socket_handshake.cpp`** - Connection handshake and client management tests
- **`test_socket_integration.cpp`** - End-to-end protocol integration tests

### Test Data

- **`data/test_commands.txt`** - Sample commands for testing
- **`data/expected_responses.txt`** - Expected response formats and patterns

## Protocol Specification

### Connection Handshake

- Server listens on `127.0.0.1:PORT` (specified by `--socket=PORT`)
- Only one client allowed per session
- Client receives: `"WELCOME:Client ID X"` or `"REJECT:Another client is already connected"`

### Command Format

```
COMMAND:request_id:action_name[:arg1][:arg2]...
```

Examples:
- `COMMAND:123:action-list`
- `COMMAND:456:file-new`
- `COMMAND:789:add-rect:100:100:200:200`
- `COMMAND:abc:export-png:output.png`
- `COMMAND:def:status`

### Response Format

```
RESPONSE:client_id:request_id:type:exit_code:data
```

Response Types:
- `SUCCESS:exit_code:message` (command executed successfully)
- `OUTPUT:exit_code:data` (command produced output)
- `ERROR:exit_code:message` (command failed)

Exit Codes:
- `0`: Success
- `1`: Invalid command format
- `2`: No valid actions found
- `3`: Exception occurred
- `4`: Document not available

### Special Commands

- `status`: Returns document information and Inkscape state
- `action-list`: Lists all available Inkscape actions

## Running Tests

### Automatic Testing

Tests are automatically included in the main test suite and run with:

```bash
ninja check
```

### Manual Testing

To run socket tests specifically:

```bash
# Build the tests
ninja test_socket_protocol test_socket_commands test_socket_responses test_socket_handshake test_socket_integration

# Run individual tests
./test_socket_protocol
./test_socket_commands
./test_socket_responses
./test_socket_handshake
./test_socket_integration
```

## Test Coverage

### Protocol Tests (`test_socket_protocol.cpp`)

- Command parsing and validation
- Response parsing and validation
- Protocol format compliance
- Case sensitivity handling
- Special command handling

### Command Tests (`test_socket_commands.cpp`)

- Command format validation
- Action name validation
- Request ID validation
- Argument validation
- Error handling for invalid commands

### Response Tests (`test_socket_responses.cpp`)

- Response format validation
- Response type validation
- Exit code validation
- Response data validation
- Round-trip formatting and parsing

### Handshake Tests (`test_socket_handshake.cpp`)

- Welcome message parsing and validation
- Reject message parsing and validation
- Client ID generation and validation
- Client connection management
- Multiple client scenarios

### Integration Tests (`test_socket_integration.cpp`)

- End-to-end protocol sessions
- Complete workflow scenarios
- Error recovery testing
- Response pattern matching
- Session validation

## Test Scenarios

### Basic Functionality

1. **Status Command**: Test `COMMAND:123:status` returns document information
2. **Action List**: Test `COMMAND:456:action-list` returns available actions
3. **File Operations**: Test file creation, modification, and export

### Error Handling

1. **Invalid Commands**: Test handling of malformed commands
2. **Invalid Actions**: Test handling of non-existent actions
3. **Invalid Arguments**: Test handling of incorrect argument counts/types

### Edge Cases

1. **Empty Commands**: Test handling of empty or whitespace-only commands
2. **Special Characters**: Test handling of commands with special characters
3. **Multiple Clients**: Test single-client restriction

### Integration Scenarios

1. **Complete Workflow**: Test full document creation and export workflow
2. **Error Recovery**: Test system behavior after command errors
3. **Session Management**: Test client connection and disconnection

## Dependencies

- Google Test framework (gtest)
- C++11 or later
- Standard C++ libraries (string, vector, regex, etc.)

## Contributing

When adding new socket server functionality:

1. Add corresponding tests to the appropriate test file
2. Update test data files if new command/response formats are added
3. Ensure all tests pass with `ninja check`
4. Update this README if protocol changes are made

## Related Documentation

- `doc/SOCKET_SERVER_PROTOCOL.md` - Detailed protocol specification
- `doc/SOCKET_SERVER_README.md` - Socket server overview and usage
- `src/socket-server.h` - Socket server header file
- `src/socket-server.cpp` - Socket server implementation 