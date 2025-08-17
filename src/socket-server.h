// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Socket server for Inkscape command execution
 *
 * Copyright (C) 2024 Inkscape contributors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 */

#ifndef INKSCAPE_SOCKET_SERVER_H
#define INKSCAPE_SOCKET_SERVER_H

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Forward declarations
class InkscapeApplication;

/**
 * Socket server that listens on localhost and executes Inkscape actions
 */
class SocketServer
{
public:
    SocketServer(int port, InkscapeApplication *app);
    ~SocketServer();

    /**
     * Start the socket server
     * @return true if server started successfully, false otherwise
     */
    bool start();

    /**
     * Stop the socket server
     */
    void stop();

    /**
     * Run the server main loop
     */
    void run();

    /**
     * Check if server is running
     */
    bool is_running() const { return _running; }

private:
    int _port;
    int _server_fd;
    InkscapeApplication *_app;
    std::atomic<bool> _running;
    std::vector<std::thread> _client_threads;
    std::atomic<int> _client_id_counter;
    std::atomic<int> _active_client_id;

    /**
     * Handle a client connection
     * @param client_fd Client socket file descriptor
     */
    void handle_client(int client_fd);

    /**
     * Execute a command and return the response
     * @param command The command to execute
     * @return Response string with exit code
     */
    std::string execute_command(std::string const &command);

    /**
     * Parse and validate incoming command
     * @param input Raw input from client
     * @param request_id Output parameter for request ID
     * @return Parsed command or empty string if invalid
     */
    std::string parse_command(std::string const &input, std::string &request_id);

    /**
     * Generate a unique client ID
     * @return New client ID
     */
    int generate_client_id();

    /**
     * Check if client can connect (only one client allowed)
     * @param client_id Client ID to check
     * @return true if client can connect, false otherwise
     */
    bool can_client_connect(int client_id);

    /**
     * Get status information about the current document and Inkscape state
     * @return Status information string
     */
    std::string get_status_info();

    /**
     * Send response to client
     * @param client_fd Client socket file descriptor
     * @param client_id Client ID
     * @param request_id Request ID
     * @param response Response to send
     * @return true if sent successfully, false otherwise
     */
    bool send_response(int client_fd, int client_id, std::string const &request_id, std::string const &response);

    /**
     * Clean up client threads
     */
    void cleanup_threads();
};

#endif // INKSCAPE_SOCKET_SERVER_H

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