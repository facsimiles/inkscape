# SPDX-License-Identifier: GPL-2.0-or-later
#
# Author: Tod Schmidt
# Copyright: 2025
#

#!/usr/bin/env python3
"""
Socket server test client for Inkscape CLI tests.
This script connects to the Inkscape socket server and sends test commands.
"""

import socket
import sys


class InkscapeSocketClient:
    def __init__(self, host='127.0.0.1', port=8080):
        self.host = host
        self.port = port
        self.socket = None
        
    def connect(self):
        """Connect to the socket server and read welcome message."""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)  # 10 second timeout
            self.socket.connect((self.host, self.port))
            
            # Read welcome message
            welcome = self.socket.recv(1024).decode('utf-8').strip()
            if welcome.startswith('REJECT'):
                raise Exception(welcome)
            return welcome
        except Exception as e:
            raise Exception(f"Failed to connect: {e}")
        
    def execute_command(self, request_id, command):
        """Send a command and receive response."""
        try:
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
        except Exception as e:
            raise Exception(f"Command execution failed: {e}")
        
    def close(self):
        """Close the socket connection."""
        if self.socket:
            self.socket.close()


def test_socket_server(port):
    """Run basic socket server tests."""
    client = InkscapeSocketClient(port=port)
    
    try:
        # Test 1: Connect and get welcome message
        print(f"Testing connection to port {port}...")
        welcome = client.connect()
        print(f"✓ Connected successfully: {welcome}")
        
        # Test 2: Get status
        print("Testing status command...")
        result = client.execute_command('test1', 'status')
        print(f"✓ Status: {result['data']}")
        
        # Test 3: List actions
        print("Testing action-list command...")
        result = client.execute_command('test2', 'action-list')
        print(f"✓ Actions available: {len(result['data'].split(','))} actions")
        
        # Test 4: Create new document
        print("Testing file-new command...")
        result = client.execute_command('test3', 'file-new')
        print(f"✓ New document: {result['type']} - {result['data']}")
        
        # Test 5: Get status after new document
        print("Testing status after new document...")
        result = client.execute_command('test4', 'status')
        print(f"✓ Status after new: {result['data']}")
        
        print("✓ All basic tests passed!")
        return True
        
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        client.close()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 socket_test_client.py <port>")
        sys.exit(1)
    
    port = int(sys.argv[1])
    success = test_socket_server(port)
    sys.exit(0 if success else 1) 