// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Socket Protocol Tests for Inkscape
 *
 * Copyright (C) 2024 Inkscape contributors
 *
 * Tests for the socket server protocol implementation
 */

#include <regex>
#include <string>
#include <vector>
#include <gtest/gtest.h>

// Mock socket server protocol parser for testing
class SocketProtocolParser
{
public:
    struct Command
    {
        std::string request_id;
        std::string action_name;
        std::vector<std::string> arguments;
    };

    struct Response
    {
        int client_id;
        std::string request_id;
        std::string type;
        int exit_code;
        std::string data;
    };

    // Parse incoming command string
    static Command parse_command(std::string const &input)
    {
        Command cmd;

        // Remove leading/trailing whitespace
        std::string cleaned = input;
        cleaned.erase(0, cleaned.find_first_not_of(" \t\r\n"));
        cleaned.erase(cleaned.find_last_not_of(" \t\r\n") + 1);

        // Check for COMMAND: prefix (case insensitive)
        std::string upper_input = cleaned;
        std::transform(upper_input.begin(), upper_input.end(), upper_input.begin(), ::toupper);

        if (upper_input.substr(0, 8) != "COMMAND:") {
            return cmd; // Return empty command
        }

        // Extract the command part after COMMAND:
        std::string command_part = cleaned.substr(8);

        // Parse request ID and actual command
        size_t first_colon = command_part.find(':');
        if (first_colon != std::string::npos) {
            cmd.request_id = command_part.substr(0, first_colon);
            cmd.action_name = command_part.substr(first_colon + 1);
            // Don't parse arguments - the action system handles that
        } else {
            // No request ID provided
            cmd.request_id = "";
            cmd.action_name = command_part;
        }

        return cmd;
    }

    // Parse response string
    static Response parse_response(std::string const &input)
    {
        Response resp;

        std::vector<std::string> parts = split_string(input, ':');
        if (parts.size() >= 5 && parts[0] == "RESPONSE") {
            try {
                resp.client_id = std::stoi(parts[1]);
            } catch (std::exception const &) {
                resp.client_id = 0; // Default value on parsing error
            }
            resp.request_id = parts[2];
            resp.type = parts[3];
            try {
                resp.exit_code = std::stoi(parts[4]);
            } catch (std::exception const &) {
                resp.exit_code = 0; // Default value on parsing error
            }

            // Combine remaining parts as data
            if (parts.size() > 5) {
                resp.data = parts[5];
                for (size_t i = 6; i < parts.size(); ++i) {
                    resp.data += ":" + parts[i];
                }
            }
        }

        return resp;
    }

    // Validate command format
    static bool is_valid_command(std::string const &input)
    {
        Command cmd = parse_command(input);
        return !cmd.action_name.empty();
    }

    // Validate response format
    static bool is_valid_response(std::string const &input)
    {
        Response resp = parse_response(input);
        return resp.client_id > 0 && !resp.request_id.empty() && !resp.type.empty();
    }

private:
    static std::vector<std::string> split_string(std::string const &str, char delimiter)
    {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;

        while (std::getline(ss, token, delimiter)) {
            tokens.push_back(token);
        }

        return tokens;
    }
};

// Test fixture for socket protocol tests
class SocketProtocolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Setup code if needed
    }

    void TearDown() override
    {
        // Cleanup code if needed
    }
};

// Test command parsing
TEST_F(SocketProtocolTest, ParseValidCommands)
{
    // Test basic command format
    auto cmd1 = SocketProtocolParser::parse_command("COMMAND:123:file-new");
    EXPECT_EQ(cmd1.request_id, "123");
    EXPECT_EQ(cmd1.action_name, "file-new");
    EXPECT_TRUE(cmd1.arguments.empty());

    // Test command with arguments (arguments are part of action_name)
    auto cmd2 = SocketProtocolParser::parse_command("COMMAND:456:add-rect:100:100:200:200");
    EXPECT_EQ(cmd2.request_id, "456");
    EXPECT_EQ(cmd2.action_name, "add-rect:100:100:200:200");
    EXPECT_TRUE(cmd2.arguments.empty());

    // Test command without request ID
    auto cmd3 = SocketProtocolParser::parse_command("COMMAND:status");
    EXPECT_EQ(cmd3.request_id, "");
    EXPECT_EQ(cmd3.action_name, "status");
    EXPECT_TRUE(cmd3.arguments.empty());

    // Test command with whitespace
    auto cmd4 = SocketProtocolParser::parse_command("  COMMAND:789:export-png:output.png  ");
    EXPECT_EQ(cmd4.request_id, "789");
    EXPECT_EQ(cmd4.action_name, "export-png:output.png");
    EXPECT_TRUE(cmd4.arguments.empty());
}

// Test invalid command parsing
TEST_F(SocketProtocolTest, ParseInvalidCommands)
{
    // Test missing COMMAND: prefix
    auto cmd1 = SocketProtocolParser::parse_command("file-new");
    EXPECT_TRUE(cmd1.action_name.empty());

    // Test empty command
    auto cmd2 = SocketProtocolParser::parse_command("COMMAND:");
    EXPECT_TRUE(cmd2.action_name.empty());

    // Test command with only request ID
    auto cmd3 = SocketProtocolParser::parse_command("COMMAND:123:");
    EXPECT_EQ(cmd3.request_id, "123");
    EXPECT_TRUE(cmd3.action_name.empty());

    // Test case sensitivity (should be case insensitive for COMMAND:)
    auto cmd4 = SocketProtocolParser::parse_command("command:123:file-new");
    EXPECT_EQ(cmd4.request_id, "123");
    EXPECT_EQ(cmd4.action_name, "file-new");
}

// Test response parsing
TEST_F(SocketProtocolTest, ParseValidResponses)
{
    // Test success response
    auto resp1 = SocketProtocolParser::parse_response("RESPONSE:1:123:SUCCESS:0:Command executed successfully");
    EXPECT_EQ(resp1.client_id, 1);
    EXPECT_EQ(resp1.request_id, "123");
    EXPECT_EQ(resp1.type, "SUCCESS");
    EXPECT_EQ(resp1.exit_code, 0);
    EXPECT_EQ(resp1.data, "Command executed successfully");

    // Test output response
    auto resp2 = SocketProtocolParser::parse_response("RESPONSE:1:456:OUTPUT:0:action1,action2,action3");
    EXPECT_EQ(resp2.client_id, 1);
    EXPECT_EQ(resp2.request_id, "456");
    EXPECT_EQ(resp2.type, "OUTPUT");
    EXPECT_EQ(resp2.exit_code, 0);
    EXPECT_EQ(resp2.data, "action1,action2,action3");

    // Test error response
    auto resp3 = SocketProtocolParser::parse_response("RESPONSE:1:789:ERROR:2:No valid actions found");
    EXPECT_EQ(resp3.client_id, 1);
    EXPECT_EQ(resp3.request_id, "789");
    EXPECT_EQ(resp3.type, "ERROR");
    EXPECT_EQ(resp3.exit_code, 2);
    EXPECT_EQ(resp3.data, "No valid actions found");

    // Test response with data containing colons
    auto resp4 = SocketProtocolParser::parse_response("RESPONSE:1:abc:OUTPUT:0:path:to:file:with:colons");
    EXPECT_EQ(resp4.client_id, 1);
    EXPECT_EQ(resp4.request_id, "abc");
    EXPECT_EQ(resp4.type, "OUTPUT");
    EXPECT_EQ(resp4.exit_code, 0);
    EXPECT_EQ(resp4.data, "path:to:file:with:colons");
}

// Test invalid response parsing
TEST_F(SocketProtocolTest, ParseInvalidResponses)
{
    // Test missing RESPONSE prefix
    auto resp1 = SocketProtocolParser::parse_response("SUCCESS:0:Command executed");
    EXPECT_EQ(resp1.client_id, 0);

    // Test incomplete response - should fail to parse due to insufficient parts
    auto resp2 = SocketProtocolParser::parse_response("RESPONSE:1:123");
    EXPECT_EQ(resp2.client_id, 0); // Should fail to parse due to insufficient parts
    EXPECT_TRUE(resp2.request_id.empty());

    // Test invalid client ID - should fail to parse and return 0
    auto resp3 = SocketProtocolParser::parse_response("RESPONSE:abc:123:SUCCESS:0:test");
    EXPECT_EQ(resp3.client_id, 0); // Should fail to parse

    // Test invalid exit code - should fail to parse and return 0
    auto resp4 = SocketProtocolParser::parse_response("RESPONSE:1:123:SUCCESS:xyz:test");
    EXPECT_EQ(resp4.exit_code, 0); // Should fail to parse
}

// Test command validation
TEST_F(SocketProtocolTest, ValidateCommands)
{
    EXPECT_TRUE(SocketProtocolParser::is_valid_command("COMMAND:123:file-new"));
    EXPECT_TRUE(SocketProtocolParser::is_valid_command("COMMAND:456:add-rect:100:100:200:200"));
    EXPECT_TRUE(SocketProtocolParser::is_valid_command("COMMAND:status"));
    EXPECT_TRUE(SocketProtocolParser::is_valid_command("  COMMAND:789:export-png:output.png  "));

    EXPECT_FALSE(SocketProtocolParser::is_valid_command("file-new"));
    EXPECT_FALSE(SocketProtocolParser::is_valid_command("COMMAND:"));
    EXPECT_FALSE(SocketProtocolParser::is_valid_command("COMMAND:123:"));
    EXPECT_FALSE(SocketProtocolParser::is_valid_command(""));
}

// Test response validation
TEST_F(SocketProtocolTest, ValidateResponses)
{
    EXPECT_TRUE(SocketProtocolParser::is_valid_response("RESPONSE:1:123:SUCCESS:0:Command executed successfully"));
    EXPECT_TRUE(SocketProtocolParser::is_valid_response("RESPONSE:1:456:OUTPUT:0:action1,action2,action3"));
    EXPECT_TRUE(SocketProtocolParser::is_valid_response("RESPONSE:1:789:ERROR:2:No valid actions found"));

    EXPECT_FALSE(SocketProtocolParser::is_valid_response("SUCCESS:0:Command executed"));
    EXPECT_FALSE(SocketProtocolParser::is_valid_response("RESPONSE:1:123"));
    EXPECT_FALSE(SocketProtocolParser::is_valid_response("RESPONSE:0:123:SUCCESS:0:test"));
    EXPECT_FALSE(SocketProtocolParser::is_valid_response(""));
}

// Test special commands
TEST_F(SocketProtocolTest, SpecialCommands)
{
    // Test status command
    auto cmd1 = SocketProtocolParser::parse_command("COMMAND:123:status");
    EXPECT_EQ(cmd1.action_name, "status");
    EXPECT_TRUE(cmd1.arguments.empty());

    // Test action-list command
    auto cmd2 = SocketProtocolParser::parse_command("COMMAND:456:action-list");
    EXPECT_EQ(cmd2.action_name, "action-list");
    EXPECT_TRUE(cmd2.arguments.empty());
}

// Test command with various argument types
TEST_F(SocketProtocolTest, CommandArguments)
{
    // Test numeric arguments (arguments are part of action_name)
    auto cmd1 = SocketProtocolParser::parse_command("COMMAND:123:add-rect:100:200:300:400");
    EXPECT_EQ(cmd1.action_name, "add-rect:100:200:300:400");
    EXPECT_TRUE(cmd1.arguments.empty());

    // Test string arguments (arguments are part of action_name)
    auto cmd2 = SocketProtocolParser::parse_command("COMMAND:456:export-png:output.png:800:600");
    EXPECT_EQ(cmd2.action_name, "export-png:output.png:800:600");
    EXPECT_TRUE(cmd2.arguments.empty());

    // Test command ending with colon (no arguments)
    auto cmd3 = SocketProtocolParser::parse_command("COMMAND:789:file-new:");
    EXPECT_EQ(cmd3.action_name, "file-new:");
    EXPECT_TRUE(cmd3.arguments.empty());
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}