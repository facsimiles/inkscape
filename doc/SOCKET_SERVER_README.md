<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!--
Author: Tod Schmidt
Copyright: 2025
-->

# Inkscape Socket Server

This document describes the new socket server functionality added to Inkscape.

## Overview

The socket server allows external applications to send commands to Inkscape via TCP socket connections. It emulates a shell interface over the network, enabling remote control of Inkscape operations.

## Usage

### Starting the Socket Server

To start Inkscape with the socket server enabled:

```bash
inkscape --socket=8080
```

This will:
- Start Inkscape in headless mode (no GUI)
- Open a TCP socket server on `127.0.0.1:8080`
- Listen for incoming connections
- Accept and execute commands from clients

### Command Protocol

The socket server uses a simple text-based protocol:

**Command Format:**
```
COMMAND:action1:arg1;action2:arg2
```

**Response Format:**
```
SUCCESS:Command executed successfully
```
or
```
ERROR:Error message
```

### Example Commands

```bash
# List all available actions
COMMAND:action-list

# Get version information
COMMAND:version

# Query document properties
COMMAND:query-all

# Execute multiple actions
COMMAND:new;select-all;delete
```

### Testing with Python

Use the provided test script:

```bash
python3 test_socket.py 8080
```

### Testing with netcat

```bash
# Connect to the server
nc 127.0.0.1 8080

# Send commands
COMMAND:action-list
COMMAND:version
```

## Implementation Details

### Files Modified

1. **`src/inkscape-application.h`** - Added socket server member variables
2. **`src/inkscape-application.cpp`** - Added socket option processing and integration
3. **`src/socket-server.h`** - New socket server class header
4. **`src/socket-server.cpp`** - New socket server implementation
5. **`src/CMakeLists.txt`** - Added socket server source files

### Architecture

- **SocketServer Class**: Handles TCP connections and command execution
- **Multi-threaded**: Each client connection runs in its own thread
- **Action Integration**: Uses existing Inkscape action system
- **Cross-platform**: Supports both Windows and Unix-like systems

### Security Considerations

- Only binds to localhost (127.0.0.1)
- No authentication (intended for local use only)
- Port validation (1-65535)
- Input sanitization

## Limitations

- No persistent connections (each command is processed independently)
- No output capture (stdout/stderr not captured)
- No authentication or encryption
- Limited to localhost connections

## Future Enhancements

- Persistent connections
- Output capture and streaming
- Authentication and encryption
- Remote connections (with proper security)
- JSON-based protocol
- Batch command processing 