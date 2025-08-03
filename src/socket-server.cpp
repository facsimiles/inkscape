// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Socket server for Inkscape command execution
 *
 * Copyright (C) 2024 Inkscape contributors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 * PROTOCOL DOCUMENTATION:
 * ======================
 *
 * Connection:
 * -----------
 * - Server listens on 127.0.0.1:PORT (specified by --socket=PORT)
 * - Only one client allowed per session
 * - Client receives: "WELCOME:Client ID X" or "REJECT:Another client is already connected"
 *
 * Command Format:
 * ---------------
 * COMMAND:request_id:action_name[:arg1][:arg2]...
 *
 * Examples:
 * - COMMAND:123:action-list
 * - COMMAND:456:file-new
 * - COMMAND:789:add-rect:100:100:200:200
 * - COMMAND:abc:export-png:output.png
 * - COMMAND:def:status
 *
 * Response Format:
 * ---------------
 * RESPONSE:client_id:request_id:type:exit_code:data
 *
 * Response Types:
 * - SUCCESS:exit_code:message (command executed successfully)
 * - OUTPUT:exit_code:data (command produced output)
 * - ERROR:exit_code:message (command failed)
 *
 * Exit Codes:
 * - 0: Success
 * - 1: Invalid command format
 * - 2: No valid actions found
 * - 3: Exception occurred
 * - 4: Document not available
 *
 * Examples:
 * - RESPONSE:1:123:OUTPUT:0:action1,action2,action3
 * - RESPONSE:1:456:SUCCESS:0:Command executed successfully
 * - RESPONSE:1:789:ERROR:2:No valid actions found in command
 *
 * Special Commands:
 * ----------------
 * - status: Returns document information and Inkscape state
 * - action-list: Lists all available Inkscape actions
 *
 * MCP Server Integration:
 * ----------------------
 * This protocol is designed for MCP (Model Context Protocol) server integration.
 * The MCP server should:
 * 1. Parse RESPONSE:client_id:request_id:type:exit_code:data format
 * 2. Extract data after the fourth colon
 * 3. Convert to appropriate MCP JSON format
 * 4. Handle different response types (SUCCESS, OUTPUT, ERROR)
 * 5. Use exit codes for proper error handling
 */

#include "socket-server.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

#include "actions/actions-helper-gui.h"
#include "document.h"
#include "inkscape-application.h"
#include "inkscape.h"
#include "util/units.h"
#include "xml/node.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define close closesocket
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

SocketServer::SocketServer(int port, InkscapeApplication *app)
    : _port(port)
    , _server_fd(-1)
    , _app(app)
    , _running(false)
    , _client_id_counter(0)
    , _active_client_id(-1)
{}

SocketServer::~SocketServer()
{
    stop();
}

bool SocketServer::start()
{
#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
#endif

    // Create socket
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        close(_server_fd);
        return false;
    }

    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(_port);

    if (bind(_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind socket to port " << _port << std::endl;
        close(_server_fd);
        return false;
    }

    // Listen for connections
    if (listen(_server_fd, 5) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(_server_fd);
        return false;
    }

    _running = true;
    std::cout << "Socket server started on 127.0.0.1:" << _port << std::endl;
    return true;
}

void SocketServer::stop()
{
    /** TODO: Error handling? */
    if (_server_fd >= 0) {
        close(_server_fd);
        _server_fd = -1;
    }

    cleanup_threads();

#ifdef _WIN32
    WSACleanup();
#endif
    _running = false;
}

void SocketServer::run()
{
    if (!_running) {
        std::cerr << "Server not started" << std::endl;
        return;
    }

    std::cout << "Socket server running. Accepting connections..." << std::endl;

    while (_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(_server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (_running) {
                std::cerr << "Failed to accept connection" << std::endl;
            }
            continue;
        }

        // Create a new thread to handle this client
        _client_threads.emplace_back(&SocketServer::handle_client, this, client_fd);
    }
}

void SocketServer::handle_client(int client_fd)
{
    /** TODO: Error handling? Is this buffer safe? */
    char buffer[1024];
    std::string response;
    std::string input_buffer;

    // Generate client ID and check if we can accept this client
    int client_id = generate_client_id();
    if (!can_client_connect(client_id)) {
        std::string reject_msg = "REJECT:Another client is already connected";
        send(client_fd, reject_msg.c_str(), reject_msg.length(), 0);
        close(client_fd);
        return;
    }

    // Send welcome message with client ID
    std::string welcome_msg = "WELCOME:Client ID " + std::to_string(client_id);
    send(client_fd, welcome_msg.c_str(), welcome_msg.length(), 0);

    while (_running) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0) {
            break; // Client disconnected or error
        }

        // Add received data to buffer
        input_buffer += std::string(buffer);

        // Look for complete commands (ending with newline or semicolon)
        /** TODO: Shell requires semicolon to separate commands 
         and end a command */
        size_t pos = 0;
        while ((pos = input_buffer.find('\n')) != std::string::npos ||
               (pos = input_buffer.find('\r')) != std::string::npos) {
            // Extract the command up to the newline
            std::string command_line = input_buffer.substr(0, pos);
            input_buffer = input_buffer.substr(pos + 1);

            // Remove carriage return if present
            if (!command_line.empty() && command_line.back() == '\r') {
                command_line.pop_back();
            }

            // Skip empty lines
            if (command_line.empty()) {
                continue;
            }

            // Parse and execute command
            std::string request_id;
            std::string command = parse_command(command_line, request_id);
            if (!command.empty()) {
                response = execute_command(command);
            } else {
                response = "ERROR:1:Invalid command format. Use: COMMAND:request_id:action1:arg1;action2:arg2";
            }

            // Send response
            if (!send_response(client_fd, client_id, request_id, response)) {
                close(client_fd);
                return;
            }
        }

        // Also check for commands ending with semicolon (for multiple commands)
        while ((pos = input_buffer.find(';')) != std::string::npos) {
            std::string command_line = input_buffer.substr(0, pos);
            input_buffer = input_buffer.substr(pos + 1);

            // Skip empty commands
            if (command_line.empty()) {
                continue;
            }

            // Parse and execute command
            std::string request_id;
            std::string command = parse_command(command_line, request_id);
            if (!command.empty()) {
                response = execute_command(command);
            } else {
                response = "ERROR:1:Invalid command format. Use: COMMAND:request_id:action1:arg1;action2:arg2;";
            }

            // Send response
            if (!send_response(client_fd, client_id, request_id, response)) {
                close(client_fd);
                return;
            }
        }
    }

    // Release client ID when client disconnects
    if (_active_client_id.load() == client_id) {
        _active_client_id.store(-1);
    }

    close(client_fd);
}

std::string SocketServer::execute_command(std::string const &command)
{
    try {
        // Handle special STATUS command
        if (command == "status") {
            return get_status_info();
        }

        // Create action vector from command
        action_vector_t action_vector;
        _app->parse_actions(command, action_vector);

        if (action_vector.empty()) {
            return "ERROR:2:No valid actions found in command";
        }

        // Ensure we have a document for actions that need it
        /** TODO: Not sure I like creating a new document here.
         *  I think we should just use the active document.
         *  If there is no active document, we should return an error.
         *  If the document is not loaded, we should return an error.
         *  If the document is not saved, we should return an error.
         *  If the document is not a valid document, we should return an error.
         */
        if (!_app->get_active_document()) {
            // Create a new document if none exists
            _app->document_new();
        }

        // Capture stdout before executing actions
        std::stringstream captured_output;
        std::streambuf *original_cout = std::cout.rdbuf();
        std::cout.rdbuf(captured_output.rdbuf());

        // Execute actions
        activate_any_actions(action_vector, Glib::RefPtr<Gio::Application>(_app->gio_app()), _app->get_active_window(),
                             _app->get_active_document());

        // Process any pending events
        auto context = Glib::MainContext::get_default();
        while (context->iteration(false)) {
        }

        // Restore original stdout
        std::cout.rdbuf(original_cout);

        // Get the captured output
        std::string output = captured_output.str();

        // Clean up the output (remove trailing newlines)
        while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
            output.pop_back();
        }

        // If there's output, return it, otherwise return success message
        if (!output.empty()) {
            return "OUTPUT:0:" + output;
        } else {
            return "SUCCESS:0:Command executed successfully";
        }

    } catch (std::exception const &e) {
        return "ERROR:3:" + std::string(e.what());
    }
}

std::string SocketServer::parse_command(std::string const &input, std::string &request_id)
{
    // Remove leading/trailing whitespace
    std::string cleaned = input;
    cleaned.erase(0, cleaned.find_first_not_of(" \t\r\n"));
    cleaned.erase(cleaned.find_last_not_of(" \t\r\n") + 1);

    // Check for COMMAND: prefix (case insensitive)
    /** We use COMMAND to allow for request id so client
     *  can track the response to the command.
     */
    std::string upper_input = cleaned;
    std::transform(upper_input.begin(), upper_input.end(), upper_input.begin(), ::toupper);

    if (upper_input.substr(0, 8) != "COMMAND:") {
        return "";
    }

    // Extract the command part after COMMAND:
    std::string command_part = cleaned.substr(8);

    // Parse request ID (format: COMMAND:request_id:actual_command)
    size_t first_colon = command_part.find(':');
    if (first_colon != std::string::npos) {
        request_id = command_part.substr(0, first_colon);
        return command_part.substr(first_colon + 1);
    } else {
        // No request ID provided, use empty string
        request_id = "";
        return command_part;
    }
}

int SocketServer::generate_client_id()
{
    return ++_client_id_counter;
}

bool SocketServer::can_client_connect(int client_id)
{
    int expected = -1;
    return _active_client_id.compare_exchange_strong(expected, client_id);
}

std::string SocketServer::get_status_info()
{
    std::stringstream status;

    // Check if we have an active document
    auto doc = _app->get_active_document();
    if (doc) {
        status << "SUCCESS:0:Document active - ";

        // Get document name
        char const *doc_name = doc->getDocumentName();
        if (doc_name && strlen(doc_name) > 0) {
            status << "Name: " << doc_name << ", ";
        }

        // Get document dimensions
        auto width = doc->getWidth();
        auto height = doc->getHeight();
        status << "Size: " << width.quantity << "x" << height.quantity << "px, ";

        // Get number of objects
        /** TODO: Any other info we want to include? */
        auto root = doc->getReprRoot();
        if (root) {
            int object_count = 0;
            for (auto child = root->firstChild(); child; child = child->next()) {
                object_count++;
            }
            status << "Objects: " << object_count;
        }
    } else {
        status << "SUCCESS:0:No active document - Inkscape ready for new document";
    }

    return status.str();
}

bool SocketServer::send_response(int client_fd, int client_id, std::string const &request_id,
                                 std::string const &response)
{
    // Format: RESPONSE:client_id:request_id:response
    std::string formatted_response = "RESPONSE:" + std::to_string(client_id) + ":" + request_id + ":" + response + "\n";
    int bytes_sent = send(client_fd, formatted_response.c_str(), formatted_response.length(), 0);
    return bytes_sent > 0;
}

void SocketServer::cleanup_threads()
{
    for (auto &thread : _client_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    _client_threads.clear();
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :