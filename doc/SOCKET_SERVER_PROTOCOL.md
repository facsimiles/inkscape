# Inkscape Socket Server Protocol

## Overview

The Inkscape Socket Server provides a TCP-based interface for executing Inkscape commands remotely. It's designed specifically for MCP (Model Context Protocol) server integration.

## Connection

- **Host**: 127.0.0.1
- **Port**: Specified by `--socket=PORT` command line argument
- **Protocol**: TCP
- **Client Limit**: Only one client allowed per session

## Connection Handshake

When a client connects:

```
Client connects → Server responds with:
"WELCOME:Client ID X" (if no other client is connected)
"REJECT:Another client is already connected" (if another client is active)
```

## Command Format

```
COMMAND:request_id:action_name[:arg1][:arg2]...
```

### Parameters

- **request_id**: Unique identifier for request/response correlation (any string)
- **action_name**: Inkscape action to execute
- **arg1, arg2, ...**: Optional arguments for the action

### Examples

```
COMMAND:123:action-list
COMMAND:456:file-new
COMMAND:789:add-rect:100:100:200:200
COMMAND:abc:export-png:output.png
COMMAND:def:status
```

## Response Format

```
RESPONSE:client_id:request_id:type:exit_code:data
```

### Parameters

- **client_id**: Numeric ID assigned by server
- **request_id**: Echo of the request ID from the command
- **type**: Response type (SUCCESS, OUTPUT, ERROR)
- **exit_code**: Numeric exit code
- **data**: Response data or error message

## Response Types

### SUCCESS
Command executed successfully with no output.

```
RESPONSE:1:456:SUCCESS:0:Command executed successfully
```

### OUTPUT
Command produced output data.

```
RESPONSE:1:123:OUTPUT:0:action1,action2,action3
```

### ERROR
Command failed with an error.

```
RESPONSE:1:789:ERROR:2:No valid actions found in command
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Invalid command format |
| 2 | No valid actions found |
| 3 | Exception occurred |
| 4 | Document not available |

## Special Commands

### STATUS
Returns information about the current document and Inkscape state.

```
Input:  COMMAND:123:status
Output: RESPONSE:1:123:SUCCESS:0:Document active - Name: test.svg, Size: 1024x768px, Objects: 12
```

**Status Information Includes:**
- Document name (if available)
- Document dimensions (width x height)
- Number of objects in the document
- Document state (active/not active)

### ACTION-LIST
Lists all available Inkscape actions.

```
Input:  COMMAND:456:action-list
Output: RESPONSE:1:456:OUTPUT:0:file-new,file-open,add-rect,export-png,...
```

## MCP Server Integration

### Parsing Responses

Your MCP server should:

1. **Split the response** by colons: `RESPONSE:client_id:request_id:type:exit_code:data`
2. **Extract the data** (everything after the 4th colon)
3. **Check the type** to determine how to handle the response
4. **Use the exit code** for error handling

### Example MCP Server Code (Python)

```python
import socket
import json

class InkscapeSocketClient:
    def __init__(self, host='127.0.0.1', port=8080):
        self.host = host
        self.port = port
        self.socket = None
        
    def connect(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((self.host, self.port))
        
        # Read welcome message
        welcome = self.socket.recv(1024).decode('utf-8').strip()
        if welcome.startswith('REJECT'):
            raise Exception(welcome)
        return welcome
        
    def execute_command(self, request_id, command):
        # Send command
        cmd = f"COMMAND:{request_id}:{command}\n"
        self.socket.send(cmd.encode('utf-8'))
        
        # Read response
        response = self.socket.recv(1024).decode('utf-8').strip()
        
        # Parse response
        parts = response.split(':', 4)  # Split into max 5 parts
        if len(parts) < 5 or parts[0] != 'RESPONSE':
            raise Exception(f"Invalid response format: {response}")
            
        client_id, req_id, resp_type, exit_code, data = parts
        
        return {
            'client_id': client_id,
            'request_id': req_id,
            'type': resp_type,
            'exit_code': int(exit_code),
            'data': data
        }
        
    def close(self):
        if self.socket:
            self.socket.close()

# Usage example
client = InkscapeSocketClient(port=8080)
client.connect()

# Get status
result = client.execute_command('123', 'status')
print(f"Status: {result['data']}")

# List actions
result = client.execute_command('456', 'action-list')
print(f"Actions: {result['data']}")

# Create new document
result = client.execute_command('789', 'file-new')
print(f"Result: {result['type']} - {result['data']}")

client.close()
```

### Converting to MCP JSON Format

```python
def convert_to_mcp_response(inkscape_response):
    """Convert Inkscape socket response to MCP JSON format"""
    
    if inkscape_response['type'] == 'SUCCESS':
        return {
            'success': True,
            'data': inkscape_response['data'],
            'exit_code': inkscape_response['exit_code']
        }
    elif inkscape_response['type'] == 'OUTPUT':
        return {
            'success': True,
            'output': inkscape_response['data'],
            'exit_code': inkscape_response['exit_code']
        }
    else:  # ERROR
        return {
            'success': False,
            'error': inkscape_response['data'],
            'exit_code': inkscape_response['exit_code']
        }
```

## Error Handling

### Common Error Scenarios

1. **Invalid Command Format**
   ```
   RESPONSE:1:123:ERROR:1:Invalid command format. Use: COMMAND:request_id:action1:arg1;action2:arg2
   ```

2. **Action Not Found**
   ```
   RESPONSE:1:456:ERROR:2:No valid actions found in command
   ```

3. **Exception During Execution**
   ```
   RESPONSE:1:789:ERROR:3:Exception message here
   ```

### Best Practices

1. **Always check exit codes** - 0 means success, non-zero means error
2. **Handle connection errors** - Socket may disconnect unexpectedly
3. **Use request IDs** - Essential for correlating responses with requests
4. **Parse response type** - Different types require different handling
5. **Clean up connections** - Close socket when done

## Testing

### Manual Testing with Telnet

```bash
# Connect to socket server
telnet 127.0.0.1 8080

# Send commands
COMMAND:123:status
COMMAND:456:action-list
COMMAND:789:file-new
```

### Expected Output

```
WELCOME:Client ID 1
RESPONSE:1:123:SUCCESS:0:No active document - Inkscape ready for new document
RESPONSE:1:456:OUTPUT:0:file-new,file-open,add-rect,export-png,...
RESPONSE:1:789:SUCCESS:0:Command executed successfully
```

## Security Considerations

- **Local Only**: Server only listens on 127.0.0.1 (localhost)
- **Single Client**: Only one client allowed per session
- **No Authentication**: Intended for local MCP server integration only
- **Command Validation**: Inkscape validates all actions before execution

## Performance Notes

- **Low Latency**: Direct socket communication
- **Buffered Input**: Handles telnet character-by-character input properly
- **Output Capture**: Captures Inkscape action output and sends through socket
- **Thread Safety**: Uses atomic operations for client management 