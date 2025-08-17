// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Socket Command Tests for Inkscape
 *
 * Copyright (C) 2024 Inkscape contributors
 *
 * Tests for socket server command parsing and validation
 */

#include <regex>
#include <string>
#include <vector>
#include <gtest/gtest.h>

// Mock command parser for testing
class SocketCommandParser
{
public:
    struct ParsedCommand
    {
        std::string request_id;
        std::string action_name;
        std::vector<std::string> arguments;
        bool is_valid;
        std::string error_message;
    };

    // Parse and validate a command string
    static ParsedCommand parse_command(std::string const &input)
    {
        ParsedCommand result;
        result.is_valid = false;

        // Remove leading/trailing whitespace
        std::string cleaned = input;
        cleaned.erase(0, cleaned.find_first_not_of(" \t\r\n"));
        cleaned.erase(cleaned.find_last_not_of(" \t\r\n") + 1);

        if (cleaned.empty()) {
            result.error_message = "Empty command";
            return result;
        }

        // Check for COMMAND: prefix (case insensitive)
        std::string upper_input = cleaned;
        std::transform(upper_input.begin(), upper_input.end(), upper_input.begin(), ::toupper);

        if (upper_input.substr(0, 8) != "COMMAND:") {
            result.error_message = "Missing COMMAND: prefix";
            return result;
        }

        // Extract the command part after COMMAND:
        std::string command_part = cleaned.substr(8);

        if (command_part.empty()) {
            result.error_message = "No command specified after COMMAND:";
            return result;
        }

        // Parse request ID and actual command
        size_t first_colon = command_part.find(':');
        if (first_colon != std::string::npos) {
            result.request_id = command_part.substr(0, first_colon);
            std::string actual_command = command_part.substr(first_colon + 1);

            if (actual_command.empty()) {
                result.error_message = "No action specified after request ID";
                return result;
            }

            // Parse action name and arguments
            std::vector<std::string> parts = split_string(actual_command, ':');
            result.action_name = parts[0];
            result.arguments.assign(parts.begin() + 1, parts.end());
        } else {
            // No request ID provided
            result.request_id = "";
            std::vector<std::string> parts = split_string(command_part, ':');
            result.action_name = parts[0];
            result.arguments.assign(parts.begin() + 1, parts.end());
        }

        // Validate action name
        if (result.action_name.empty()) {
            result.error_message = "Empty action name";
            return result;
        }

        // Check for invalid characters in action name
        if (!is_valid_action_name(result.action_name)) {
            result.error_message = "Invalid action name: " + result.action_name;
            return result;
        }

        result.is_valid = true;
        return result;
    }

    // Validate action name format
    static bool is_valid_action_name(std::string const &action_name)
    {
        if (action_name.empty()) {
            return false;
        }

        // Action names should contain only alphanumeric characters, hyphens, and underscores
        std::regex action_pattern("^[a-zA-Z0-9_-]+$");
        return std::regex_match(action_name, action_pattern);
    }

    // Validate request ID format
    static bool is_valid_request_id(std::string const &request_id)
    {
        if (request_id.empty()) {
            return true; // Empty request ID is allowed
        }

        // Request IDs should contain only alphanumeric characters and hyphens
        std::regex id_pattern("^[a-zA-Z0-9-]+$");
        return std::regex_match(request_id, id_pattern);
    }

    // Check if command is a special command
    static bool is_special_command(std::string const &action_name)
    {
        return action_name == "status" || action_name == "action-list";
    }

    // Validate arguments for specific actions
    static bool validate_arguments(std::string const &action_name, std::vector<std::string> const &arguments)
    {
        if (action_name == "status" || action_name == "action-list") {
            return arguments.empty(); // These commands take no arguments
        }

        if (action_name == "file-new") {
            return arguments.empty(); // file-new takes no arguments
        }

        if (action_name == "add-rect") {
            return arguments.size() == 4; // x, y, width, height
        }

        if (action_name == "export-png") {
            return arguments.size() >= 1 && arguments.size() <= 3; // filename, [width], [height]
        }

        // For other actions, accept any number of arguments
        return true;
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

// Test fixture for socket command tests
class SocketCommandTest : public ::testing::Test
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

// Test valid command parsing
TEST_F(SocketCommandTest, ParseValidCommands)
{
    // Test basic command
    auto cmd1 = SocketCommandParser::parse_command("COMMAND:123:file-new");
    EXPECT_TRUE(cmd1.is_valid);
    EXPECT_EQ(cmd1.request_id, "123");
    EXPECT_EQ(cmd1.action_name, "file-new");
    EXPECT_TRUE(cmd1.arguments.empty());

    // Test command with arguments
    auto cmd2 = SocketCommandParser::parse_command("COMMAND:456:add-rect:100:100:200:200");
    EXPECT_TRUE(cmd2.is_valid);
    EXPECT_EQ(cmd2.request_id, "456");
    EXPECT_EQ(cmd2.action_name, "add-rect");
    EXPECT_EQ(cmd2.arguments.size(), 4);
    EXPECT_EQ(cmd2.arguments[0], "100");
    EXPECT_EQ(cmd2.arguments[1], "100");
    EXPECT_EQ(cmd2.arguments[2], "200");
    EXPECT_EQ(cmd2.arguments[3], "200");

    // Test command without request ID
    auto cmd3 = SocketCommandParser::parse_command("COMMAND:status");
    EXPECT_TRUE(cmd3.is_valid);
    EXPECT_EQ(cmd3.request_id, "");
    EXPECT_EQ(cmd3.action_name, "status");
    EXPECT_TRUE(cmd3.arguments.empty());

    // Test command with whitespace
    auto cmd4 = SocketCommandParser::parse_command("  COMMAND:789:export-png:output.png  ");
    EXPECT_TRUE(cmd4.is_valid);
    EXPECT_EQ(cmd4.request_id, "789");
    EXPECT_EQ(cmd4.action_name, "export-png");
    EXPECT_EQ(cmd4.arguments.size(), 1);
    EXPECT_EQ(cmd4.arguments[0], "output.png");
}

// Test invalid command parsing
TEST_F(SocketCommandTest, ParseInvalidCommands)
{
    // Test missing COMMAND: prefix
    auto cmd1 = SocketCommandParser::parse_command("file-new");
    EXPECT_FALSE(cmd1.is_valid);
    EXPECT_EQ(cmd1.error_message, "Missing COMMAND: prefix");

    // Test empty command
    auto cmd2 = SocketCommandParser::parse_command("");
    EXPECT_FALSE(cmd2.is_valid);
    EXPECT_EQ(cmd2.error_message, "Empty command");

    // Test command with only COMMAND: prefix
    auto cmd3 = SocketCommandParser::parse_command("COMMAND:");
    EXPECT_FALSE(cmd3.is_valid);
    EXPECT_EQ(cmd3.error_message, "No command specified after COMMAND:");

    // Test command with only request ID
    auto cmd4 = SocketCommandParser::parse_command("COMMAND:123:");
    EXPECT_FALSE(cmd4.is_valid);
    EXPECT_EQ(cmd4.error_message, "No action specified after request ID");

    // Test command with invalid action name
    auto cmd5 = SocketCommandParser::parse_command("COMMAND:123:invalid@action");
    EXPECT_FALSE(cmd5.is_valid);
    EXPECT_EQ(cmd5.error_message, "Invalid action name: invalid@action");
}

// Test action name validation
TEST_F(SocketCommandTest, ValidateActionNames)
{
    EXPECT_TRUE(SocketCommandParser::is_valid_action_name("file-new"));
    EXPECT_TRUE(SocketCommandParser::is_valid_action_name("add-rect"));
    EXPECT_TRUE(SocketCommandParser::is_valid_action_name("export-png"));
    EXPECT_TRUE(SocketCommandParser::is_valid_action_name("status"));
    EXPECT_TRUE(SocketCommandParser::is_valid_action_name("action-list"));
    EXPECT_TRUE(SocketCommandParser::is_valid_action_name("action_name"));
    EXPECT_TRUE(SocketCommandParser::is_valid_action_name("action123"));

    EXPECT_FALSE(SocketCommandParser::is_valid_action_name(""));
    EXPECT_FALSE(SocketCommandParser::is_valid_action_name("invalid@action"));
    EXPECT_FALSE(SocketCommandParser::is_valid_action_name("invalid action"));
    EXPECT_FALSE(SocketCommandParser::is_valid_action_name("invalid:action"));
    EXPECT_FALSE(SocketCommandParser::is_valid_action_name("invalid.action"));
}

// Test request ID validation
TEST_F(SocketCommandTest, ValidateRequestIds)
{
    EXPECT_TRUE(SocketCommandParser::is_valid_request_id(""));
    EXPECT_TRUE(SocketCommandParser::is_valid_request_id("123"));
    EXPECT_TRUE(SocketCommandParser::is_valid_request_id("abc"));
    EXPECT_TRUE(SocketCommandParser::is_valid_request_id("abc123"));
    EXPECT_TRUE(SocketCommandParser::is_valid_request_id("abc-123"));

    EXPECT_FALSE(SocketCommandParser::is_valid_request_id("abc@123"));
    EXPECT_FALSE(SocketCommandParser::is_valid_request_id("abc_123"));
    EXPECT_FALSE(SocketCommandParser::is_valid_request_id("abc 123"));
    EXPECT_FALSE(SocketCommandParser::is_valid_request_id("abc:123"));
}

// Test special commands
TEST_F(SocketCommandTest, SpecialCommands)
{
    EXPECT_TRUE(SocketCommandParser::is_special_command("status"));
    EXPECT_TRUE(SocketCommandParser::is_special_command("action-list"));
    EXPECT_FALSE(SocketCommandParser::is_special_command("file-new"));
    EXPECT_FALSE(SocketCommandParser::is_special_command("add-rect"));
    EXPECT_FALSE(SocketCommandParser::is_special_command("export-png"));
}

// Test argument validation
TEST_F(SocketCommandTest, ValidateArguments)
{
    // Test status command (no arguments)
    EXPECT_TRUE(SocketCommandParser::validate_arguments("status", {}));
    EXPECT_FALSE(SocketCommandParser::validate_arguments("status", {"arg1"}));

    // Test action-list command (no arguments)
    EXPECT_TRUE(SocketCommandParser::validate_arguments("action-list", {}));
    EXPECT_FALSE(SocketCommandParser::validate_arguments("action-list", {"arg1"}));

    // Test file-new command (no arguments)
    EXPECT_TRUE(SocketCommandParser::validate_arguments("file-new", {}));
    EXPECT_FALSE(SocketCommandParser::validate_arguments("file-new", {"arg1"}));

    // Test add-rect command (4 arguments)
    EXPECT_TRUE(SocketCommandParser::validate_arguments("add-rect", {"100", "100", "200", "200"}));
    EXPECT_FALSE(SocketCommandParser::validate_arguments("add-rect", {"100", "100", "200"}));
    EXPECT_FALSE(SocketCommandParser::validate_arguments("add-rect", {"100", "100", "200", "200", "extra"}));

    // Test export-png command (1-3 arguments)
    EXPECT_TRUE(SocketCommandParser::validate_arguments("export-png", {"output.png"}));
    EXPECT_TRUE(SocketCommandParser::validate_arguments("export-png", {"output.png", "800"}));
    EXPECT_TRUE(SocketCommandParser::validate_arguments("export-png", {"output.png", "800", "600"}));
    EXPECT_FALSE(SocketCommandParser::validate_arguments("export-png", {}));
    EXPECT_FALSE(SocketCommandParser::validate_arguments("export-png", {"output.png", "800", "600", "extra"}));
}

// Test case sensitivity
TEST_F(SocketCommandTest, CaseSensitivity)
{
    // COMMAND: prefix should be case insensitive
    auto cmd1 = SocketCommandParser::parse_command("command:123:file-new");
    EXPECT_TRUE(cmd1.is_valid);
    EXPECT_EQ(cmd1.action_name, "file-new");

    auto cmd2 = SocketCommandParser::parse_command("Command:123:file-new");
    EXPECT_TRUE(cmd2.is_valid);
    EXPECT_EQ(cmd2.action_name, "file-new");

    auto cmd3 = SocketCommandParser::parse_command("COMMAND:123:file-new");
    EXPECT_TRUE(cmd3.is_valid);
    EXPECT_EQ(cmd3.action_name, "file-new");
}

// Test command with various argument types
TEST_F(SocketCommandTest, CommandArguments)
{
    // Test numeric arguments
    auto cmd1 = SocketCommandParser::parse_command("COMMAND:123:add-rect:100:200:300:400");
    EXPECT_TRUE(cmd1.is_valid);
    EXPECT_EQ(cmd1.action_name, "add-rect");
    EXPECT_EQ(cmd1.arguments.size(), 4);
    EXPECT_EQ(cmd1.arguments[0], "100");
    EXPECT_EQ(cmd1.arguments[1], "200");
    EXPECT_EQ(cmd1.arguments[2], "300");
    EXPECT_EQ(cmd1.arguments[3], "400");

    // Test string arguments
    auto cmd2 = SocketCommandParser::parse_command("COMMAND:456:export-png:output.png:800:600");
    EXPECT_TRUE(cmd2.is_valid);
    EXPECT_EQ(cmd2.action_name, "export-png");
    EXPECT_EQ(cmd2.arguments.size(), 3);
    EXPECT_EQ(cmd2.arguments[0], "output.png");
    EXPECT_EQ(cmd2.arguments[1], "800");
    EXPECT_EQ(cmd2.arguments[2], "600");

    // Test command ending with colon (no arguments)
    auto cmd3 = SocketCommandParser::parse_command("COMMAND:789:file-new:");
    EXPECT_TRUE(cmd3.is_valid);
    EXPECT_EQ(cmd3.action_name, "file-new");
    EXPECT_EQ(cmd3.arguments.size(), 1);
    EXPECT_EQ(cmd3.arguments[0], "");
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}