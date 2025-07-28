# SPDX-License-Identifier: GPL-2.0-or-later
#
# Author: Tod Schmidt
# Copyright: 2025
#

#!/usr/bin/env python3
"""
Socket server integration test for Inkscape CLI tests.
This script tests the complete socket server functionality including:
- Server startup
- Connection handling
- Command execution
- Response parsing
- Error handling
"""

import socket
import sys
import time
import subprocess


class SocketIntegrationTest:
    def __init__(self, port=8080):
        self.port = port
        self.inkscape_process = None
        self.test_results = []
        
    def log_test(self, test_name, success, message=""):
        """Log test result."""
        status = "PASS" if success else "FAIL"
        result = f"[{status}] {test_name}"
        if message:
            result += f": {message}"
        print(result)
        self.test_results.append((test_name, success, message))
        
    def start_inkscape_socket_server(self):
        """Start Inkscape with socket server."""
        try:
            cmd = ["inkscape", f"--socket={self.port}", "--without-gui"]
            self.inkscape_process = subprocess.Popen(
                cmd, 
                stdout=subprocess.PIPE, 
                stderr=subprocess.PIPE,
                text=True
            )
            
            # Wait for server to start
            time.sleep(3)
            
            # Check if process is still running
            if self.inkscape_process.poll() is None:
                msg = f"Server started on port {self.port}"
                self.log_test("Start Socket Server", True, msg)
                return True
            else:
                stdout, stderr = self.inkscape_process.communicate()
                msg = f"Server failed to start: {stderr}"
                self.log_test("Start Socket Server", False, msg)
                return False
                
        except Exception as e:
            self.log_test("Start Socket Server", False, f"Exception: {e}")
            return False
    
    def test_connection(self):
        """Test basic connection to socket server."""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect(('127.0.0.1', self.port))
            
            # Read welcome message
            welcome = sock.recv(1024).decode('utf-8').strip()
            sock.close()
            
            if welcome.startswith('WELCOME:'):
                self.log_test("Connection Test", True, f"Connected: {welcome}")
                return True
            else:
                self.log_test("Connection Test", False, f"Unexpected welcome: {welcome}")
                return False
                
        except Exception as e:
            self.log_test("Connection Test", False, f"Connection failed: {e}")
            return False
    
    def test_command_execution(self):
        """Test command execution through socket."""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect(('127.0.0.1', self.port))
            
            # Read welcome message
            welcome = sock.recv(1024).decode('utf-8').strip()
            
            # Send status command
            cmd = "COMMAND:test1:status\n"
            sock.send(cmd.encode('utf-8'))
            
            # Read response
            response = sock.recv(1024).decode('utf-8').strip()
            sock.close()
            
            if response.startswith('RESPONSE:'):
                self.log_test("Command Execution", True, f"Response: {response}")
                return True
            else:
                self.log_test("Command Execution", False, f"Invalid response: {response}")
                return False
                
        except Exception as e:
            self.log_test("Command Execution", False, f"Command failed: {e}")
            return False
    
    def test_multiple_commands(self):
        """Test multiple commands in sequence."""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect(('127.0.0.1', self.port))
            
            # Read welcome message
            welcome = sock.recv(1024).decode('utf-8').strip()
            
            commands = [
                ("test1", "status"),
                ("test2", "action-list"),
                ("test3", "file-new"),
                ("test4", "status")
            ]
            
            success_count = 0
            for request_id, command in commands:
                cmd = f"COMMAND:{request_id}:{command}\n"
                sock.send(cmd.encode('utf-8'))
                
                response = sock.recv(1024).decode('utf-8').strip()
                if response.startswith('RESPONSE:'):
                    success_count += 1
                    
            sock.close()
            
            if success_count == len(commands):
                self.log_test("Multiple Commands", True, f"All {success_count} commands succeeded")
                return True
            else:
                self.log_test("Multiple Commands", False, f"Only {success_count}/{len(commands)} commands succeeded")
                return False
                
        except Exception as e:
            self.log_test("Multiple Commands", False, f"Multiple commands failed: {e}")
            return False
    
    def test_error_handling(self):
        """Test error handling for invalid commands."""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect(('127.0.0.1', self.port))
            
            # Read welcome message
            welcome = sock.recv(1024).decode('utf-8').strip()
            
            # Send invalid command
            cmd = "COMMAND:error1:invalid-command\n"
            sock.send(cmd.encode('utf-8'))
            
            # Read response
            response = sock.recv(1024).decode('utf-8').strip()
            sock.close()
            
            if response.startswith('RESPONSE:') and 'ERROR' in response:
                self.log_test("Error Handling", True, f"Error response: {response}")
                return True
            else:
                self.log_test("Error Handling", False, f"Unexpected response: {response}")
                return False
                
        except Exception as e:
            self.log_test("Error Handling", False, f"Error handling failed: {e}")
            return False
    
    def cleanup(self):
        """Clean up resources."""
        if self.inkscape_process:
            try:
                self.inkscape_process.terminate()
                self.inkscape_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.inkscape_process.kill()
            except Exception:
                pass
    
    def run_all_tests(self):
        """Run all socket server tests."""
        print("Starting Socket Server Integration Tests...")
        print("=" * 50)
        
        try:
            # Test 1: Start server
            if not self.start_inkscape_socket_server():
                return False
            
            # Test 2: Connection
            if not self.test_connection():
                return False
            
            # Test 3: Command execution
            if not self.test_command_execution():
                return False
            
            # Test 4: Multiple commands
            if not self.test_multiple_commands():
                return False
            
            # Test 5: Error handling
            if not self.test_error_handling():
                return False
            
            # Summary
            print("\n" + "=" * 50)
            print("Test Summary:")
            passed = sum(1 for _, success, _ in self.test_results if success)
            total = len(self.test_results)
            print(f"Passed: {passed}/{total}")
            
            return passed == total
            
        finally:
            self.cleanup()


def main():
    """Main function for CLI testing."""
    if len(sys.argv) != 2:
        print("Usage: python3 socket_integration_test.py <port>")
        sys.exit(1)
    
    port = int(sys.argv[1])
    tester = SocketIntegrationTest(port)
    
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main() 