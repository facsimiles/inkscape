# SPDX-License-Identifier: GPL-2.0-or-later
#
# Author: Tod Schmidt
# Copyright: 2025
#

#!/usr/bin/env python3
"""
Simple socket server test for Inkscape CLI tests.
"""

import socket
import sys


def test_socket_connection(port):
    """Test basic socket connection and command execution."""
    try:
        # Connect to socket server
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect(('127.0.0.1', port))
        
        # Read welcome message
        welcome = sock.recv(1024).decode('utf-8').strip()
        if not welcome.startswith('WELCOME:'):
            print(f"FAIL: Unexpected welcome message: {welcome}")
            return False
        
        # Send status command
        cmd = "COMMAND:test1:status\n"
        sock.send(cmd.encode('utf-8'))
        
        # Read response
        response = sock.recv(1024).decode('utf-8').strip()
        sock.close()
        
        if response.startswith('RESPONSE:'):
            print(f"PASS: Socket test successful - {response}")
            return True
        else:
            print(f"FAIL: Invalid response: {response}")
            return False
            
    except Exception as e:
        print(f"FAIL: Socket test failed: {e}")
        return False


def main():
    """Main function."""
    if len(sys.argv) != 2:
        print("Usage: python3 socket_simple_test.py <port>")
        sys.exit(1)
    
    port = int(sys.argv[1])
    success = test_socket_connection(port)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main() 