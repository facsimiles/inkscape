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
import time


def test_socket_connection(port, max_retries=3, retry_delay=2):
    """Test basic socket connection and command execution."""
    for attempt in range(max_retries):
        try:
            print(f"Attempt {attempt + 1}/{max_retries}: "
                  f"Connecting to socket server...")
            
            # Connect to socket server
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10)
            sock.connect(('127.0.0.1', port))
            
            # Read welcome message
            welcome = sock.recv(1024).decode('utf-8').strip()
            print(f"Received welcome message: {welcome}")
            
            if not welcome.startswith('WELCOME:'):
                print(f"FAIL: Unexpected welcome message: {welcome}")
                sock.close()
                if attempt < max_retries - 1:
                    print(f"Retrying in {retry_delay} seconds...")
                    time.sleep(retry_delay)
                    continue
                return False
            
            # Send status command
            cmd = "COMMAND:test1:status\n"
            print(f"Sending command: {cmd.strip()}")
            sock.send(cmd.encode('utf-8'))
            
            # Read response
            response = sock.recv(1024).decode('utf-8').strip()
            print(f"Received response: {response}")
            sock.close()
            
            if response.startswith('RESPONSE:'):
                print(f"PASS: Socket test successful - {response}")
                return True
            else:
                print(f"FAIL: Invalid response: {response}")
                if attempt < max_retries - 1:
                    print(f"Retrying in {retry_delay} seconds...")
                    time.sleep(retry_delay)
                    continue
                return False
                
        except socket.timeout:
            print(f"FAIL: Socket timeout on attempt {attempt + 1}")
            if attempt < max_retries - 1:
                print(f"Retrying in {retry_delay} seconds...")
                time.sleep(retry_delay)
                continue
            return False
        except ConnectionRefusedError:
            print(f"FAIL: Connection refused on attempt {attempt + 1}")
            if attempt < max_retries - 1:
                print(f"Retrying in {retry_delay} seconds...")
                time.sleep(retry_delay)
                continue
            return False
        except Exception as e:
            print(f"FAIL: Socket test failed on attempt {attempt + 1}: {e}")
            if attempt < max_retries - 1:
                print(f"Retrying in {retry_delay} seconds...")
                time.sleep(retry_delay)
                continue
            return False
    
    return False


def main():
    """Main function."""
    if len(sys.argv) != 2:
        print("Usage: python3 socket_simple_test.py <port>")
        sys.exit(1)
    
    try:
        port = int(sys.argv[1])
    except ValueError:
        print("Error: Port must be a number")
        sys.exit(1)
    
    print(f"Testing socket server on port {port}")
    success = test_socket_connection(port)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main() 