// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Socket Response Tests for Inkscape
 *
 * Copyright (C) 2024 Inkscape contributors
 *
 * Tests for socket server response formatting and validation
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <regex>

// Mock response formatter for testing
class SocketResponseFormatter {
public:
    struct Response {
        int client_id;
        std::string request_id;
        std::string type;
        int exit_code;
        std::string data;
    };

    // Format a response according to the socket protocol
    static std::string format_response(const Response& response) {
        std::stringstream ss;
        ss << "RESPONSE:" << response.client_id << ":" 
           << response.request_id << ":" 
           << response.type << ":" 
           << response.exit_code;
        
        if (!response.data.empty()) {
            ss << ":" << response.data;
        }
        
        return ss.str();
    }

    // Parse a response string
    static Response parse_response(const std::string& input) {
        Response resp;
        resp.client_id = 0;
        resp.exit_code = 0;
        
        std::vector<std::string> parts = split_string(input, ':');
        if (parts.size() >= 5 && parts[0] == "RESPONSE") {
            try {
                resp.client_id = std::stoi(parts[1]);
                resp.request_id = parts[2];
                resp.type = parts[3];
                resp.exit_code = std::stoi(parts[4]);
                
                // Combine remaining parts as data
                if (parts.size() > 5) {
                    resp.data = parts[5];
                    for (size_t i = 6; i < parts.size(); ++i) {
                        resp.data += ":" + parts[i];
                    }
                }
            } catch (const std::exception& e) {
                // Parsing failed, return default values
                resp.client_id = 0;
                resp.exit_code = 0;
            }
        }
        
        return resp;
    }

    // Validate response format
    static bool is_valid_response(const std::string& input) {
        Response resp = parse_response(input);
        return resp.client_id > 0 && !resp.request_id.empty() && !resp.type.empty();
    }

    // Validate response type
    static bool is_valid_response_type(const std::string& type) {
        return type == "SUCCESS" || type == "OUTPUT" || type == "ERROR";
    }

    // Validate exit code
    static bool is_valid_exit_code(int exit_code) {
        return exit_code >= 0 && exit_code <= 4;
    }

    // Get exit code description
    static std::string get_exit_code_description(int exit_code) {
        switch (exit_code) {
            case 0: return "Success";
            case 1: return "Invalid command format";
            case 2: return "No valid actions found";
            case 3: return "Exception occurred";
            case 4: return "Document not available";
            default: return "Unknown exit code";
        }
    }

    // Create success response
    static Response create_success_response(int client_id, const std::string& request_id, const std::string& message = "Command executed successfully") {
        return {client_id, request_id, "SUCCESS", 0, message};
    }

    // Create output response
    static Response create_output_response(int client_id, const std::string& request_id, const std::string& output) {
        return {client_id, request_id, "OUTPUT", 0, output};
    }

    // Create error response
    static Response create_error_response(int client_id, const std::string& request_id, int exit_code, const std::string& error_message) {
        return {client_id, request_id, "ERROR", exit_code, error_message};
    }

    // Validate response data based on type
    static bool validate_response_data(const std::string& type, const std::string& data) {
        if (type == "SUCCESS") {
            return !data.empty();
        } else if (type == "OUTPUT") {
            return true; // Output can be empty
        } else if (type == "ERROR") {
            return !data.empty(); // Error should have a message
        }
        return false;
    }

private:
    static std::vector<std::string> split_string(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        
        while (std::getline(ss, token, delimiter)) {
            tokens.push_back(token);
        }
        
        return tokens;
    }
};

// Test fixture for socket response tests
class SocketResponseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test response formatting
TEST_F(SocketResponseTest, FormatResponses) {
    // Test success response
    auto resp1 = SocketResponseFormatter::create_success_response(1, "123", "Command executed successfully");
    std::string formatted1 = SocketResponseFormatter::format_response(resp1);
    EXPECT_EQ(formatted1, "RESPONSE:1:123:SUCCESS:0:Command executed successfully");

    // Test output response
    auto resp2 = SocketResponseFormatter::create_output_response(1, "456", "action1,action2,action3");
    std::string formatted2 = SocketResponseFormatter::format_response(resp2);
    EXPECT_EQ(formatted2, "RESPONSE:1:456:OUTPUT:0:action1,action2,action3");

    // Test error response
    auto resp3 = SocketResponseFormatter::create_error_response(1, "789", 2, "No valid actions found");
    std::string formatted3 = SocketResponseFormatter::format_response(resp3);
    EXPECT_EQ(formatted3, "RESPONSE:1:789:ERROR:2:No valid actions found");

    // Test response with empty data
    auto resp4 = SocketResponseFormatter::create_success_response(1, "abc", "");
    std::string formatted4 = SocketResponseFormatter::format_response(resp4);
    EXPECT_EQ(formatted4, "RESPONSE:1:abc:SUCCESS:0");
}

// Test response parsing
TEST_F(SocketResponseTest, ParseResponses) {
    // Test success response parsing
    auto resp1 = SocketResponseFormatter::parse_response("RESPONSE:1:123:SUCCESS:0:Command executed successfully");
    EXPECT_EQ(resp1.client_id, 1);
    EXPECT_EQ(resp1.request_id, "123");
    EXPECT_EQ(resp1.type, "SUCCESS");
    EXPECT_EQ(resp1.exit_code, 0);
    EXPECT_EQ(resp1.data, "Command executed successfully");

    // Test output response parsing
    auto resp2 = SocketResponseFormatter::parse_response("RESPONSE:1:456:OUTPUT:0:action1,action2,action3");
    EXPECT_EQ(resp2.client_id, 1);
    EXPECT_EQ(resp2.request_id, "456");
    EXPECT_EQ(resp2.type, "OUTPUT");
    EXPECT_EQ(resp2.exit_code, 0);
    EXPECT_EQ(resp2.data, "action1,action2,action3");

    // Test error response parsing
    auto resp3 = SocketResponseFormatter::parse_response("RESPONSE:1:789:ERROR:2:No valid actions found");
    EXPECT_EQ(resp3.client_id, 1);
    EXPECT_EQ(resp3.request_id, "789");
    EXPECT_EQ(resp3.type, "ERROR");
    EXPECT_EQ(resp3.exit_code, 2);
    EXPECT_EQ(resp3.data, "No valid actions found");

    // Test response with data containing colons
    auto resp4 = SocketResponseFormatter::parse_response("RESPONSE:1:abc:OUTPUT:0:path:to:file:with:colons");
    EXPECT_EQ(resp4.client_id, 1);
    EXPECT_EQ(resp4.request_id, "abc");
    EXPECT_EQ(resp4.type, "OUTPUT");
    EXPECT_EQ(resp4.exit_code, 0);
    EXPECT_EQ(resp4.data, "path:to:file:with:colons");
}

// Test invalid response parsing
TEST_F(SocketResponseTest, ParseInvalidResponses) {
    // Test missing RESPONSE prefix
    auto resp1 = SocketResponseFormatter::parse_response("SUCCESS:0:Command executed");
    EXPECT_EQ(resp1.client_id, 0);
    EXPECT_TRUE(resp1.request_id.empty());
    EXPECT_TRUE(resp1.type.empty());

    // Test incomplete response
    auto resp2 = SocketResponseFormatter::parse_response("RESPONSE:1:123");
    EXPECT_EQ(resp2.client_id, 1);
    EXPECT_EQ(resp2.request_id, "123");
    EXPECT_TRUE(resp2.type.empty());

    // Test invalid client ID
    auto resp3 = SocketResponseFormatter::parse_response("RESPONSE:abc:123:SUCCESS:0:test");
    EXPECT_EQ(resp3.client_id, 0); // Should fail to parse

    // Test invalid exit code
    auto resp4 = SocketResponseFormatter::parse_response("RESPONSE:1:123:SUCCESS:xyz:test");
    EXPECT_EQ(resp4.exit_code, 0); // Should fail to parse

    // Test empty response
    auto resp5 = SocketResponseFormatter::parse_response("");
    EXPECT_EQ(resp5.client_id, 0);
    EXPECT_TRUE(resp5.request_id.empty());
    EXPECT_TRUE(resp5.type.empty());
}

// Test response validation
TEST_F(SocketResponseTest, ValidateResponses) {
    EXPECT_TRUE(SocketResponseFormatter::is_valid_response("RESPONSE:1:123:SUCCESS:0:Command executed successfully"));
    EXPECT_TRUE(SocketResponseFormatter::is_valid_response("RESPONSE:1:456:OUTPUT:0:action1,action2,action3"));
    EXPECT_TRUE(SocketResponseFormatter::is_valid_response("RESPONSE:1:789:ERROR:2:No valid actions found"));
    
    EXPECT_FALSE(SocketResponseFormatter::is_valid_response("SUCCESS:0:Command executed"));
    EXPECT_FALSE(SocketResponseFormatter::is_valid_response("RESPONSE:1:123"));
    EXPECT_FALSE(SocketResponseFormatter::is_valid_response("RESPONSE:0:123:SUCCESS:0:test"));
    EXPECT_FALSE(SocketResponseFormatter::is_valid_response(""));
}

// Test response type validation
TEST_F(SocketResponseTest, ValidateResponseTypes) {
    EXPECT_TRUE(SocketResponseFormatter::is_valid_response_type("SUCCESS"));
    EXPECT_TRUE(SocketResponseFormatter::is_valid_response_type("OUTPUT"));
    EXPECT_TRUE(SocketResponseFormatter::is_valid_response_type("ERROR"));
    
    EXPECT_FALSE(SocketResponseFormatter::is_valid_response_type(""));
    EXPECT_FALSE(SocketResponseFormatter::is_valid_response_type("SUCCES"));
    EXPECT_FALSE(SocketResponseFormatter::is_valid_response_type("success"));
    EXPECT_FALSE(SocketResponseFormatter::is_valid_response_type("UNKNOWN"));
}

// Test exit code validation
TEST_F(SocketResponseTest, ValidateExitCodes) {
    EXPECT_TRUE(SocketResponseFormatter::is_valid_exit_code(0));
    EXPECT_TRUE(SocketResponseFormatter::is_valid_exit_code(1));
    EXPECT_TRUE(SocketResponseFormatter::is_valid_exit_code(2));
    EXPECT_TRUE(SocketResponseFormatter::is_valid_exit_code(3));
    EXPECT_TRUE(SocketResponseFormatter::is_valid_exit_code(4));
    
    EXPECT_FALSE(SocketResponseFormatter::is_valid_exit_code(-1));
    EXPECT_FALSE(SocketResponseFormatter::is_valid_exit_code(5));
    EXPECT_FALSE(SocketResponseFormatter::is_valid_exit_code(100));
}

// Test exit code descriptions
TEST_F(SocketResponseTest, ExitCodeDescriptions) {
    EXPECT_EQ(SocketResponseFormatter::get_exit_code_description(0), "Success");
    EXPECT_EQ(SocketResponseFormatter::get_exit_code_description(1), "Invalid command format");
    EXPECT_EQ(SocketResponseFormatter::get_exit_code_description(2), "No valid actions found");
    EXPECT_EQ(SocketResponseFormatter::get_exit_code_description(3), "Exception occurred");
    EXPECT_EQ(SocketResponseFormatter::get_exit_code_description(4), "Document not available");
    EXPECT_EQ(SocketResponseFormatter::get_exit_code_description(5), "Unknown exit code");
    EXPECT_EQ(SocketResponseFormatter::get_exit_code_description(-1), "Unknown exit code");
}

// Test response data validation
TEST_F(SocketResponseTest, ValidateResponseData) {
    // Test SUCCESS response data
    EXPECT_TRUE(SocketResponseFormatter::validate_response_data("SUCCESS", "Command executed successfully"));
    EXPECT_FALSE(SocketResponseFormatter::validate_response_data("SUCCESS", ""));

    // Test OUTPUT response data
    EXPECT_TRUE(SocketResponseFormatter::validate_response_data("OUTPUT", "action1,action2,action3"));
    EXPECT_TRUE(SocketResponseFormatter::validate_response_data("OUTPUT", ""));

    // Test ERROR response data
    EXPECT_TRUE(SocketResponseFormatter::validate_response_data("ERROR", "No valid actions found"));
    EXPECT_FALSE(SocketResponseFormatter::validate_response_data("ERROR", ""));

    // Test unknown response type
    EXPECT_FALSE(SocketResponseFormatter::validate_response_data("UNKNOWN", "test"));
}

// Test response creation helpers
TEST_F(SocketResponseTest, ResponseCreationHelpers) {
    // Test success response creation
    auto success_resp = SocketResponseFormatter::create_success_response(1, "123", "Test message");
    EXPECT_EQ(success_resp.client_id, 1);
    EXPECT_EQ(success_resp.request_id, "123");
    EXPECT_EQ(success_resp.type, "SUCCESS");
    EXPECT_EQ(success_resp.exit_code, 0);
    EXPECT_EQ(success_resp.data, "Test message");

    // Test output response creation
    auto output_resp = SocketResponseFormatter::create_output_response(1, "456", "test output");
    EXPECT_EQ(output_resp.client_id, 1);
    EXPECT_EQ(output_resp.request_id, "456");
    EXPECT_EQ(output_resp.type, "OUTPUT");
    EXPECT_EQ(output_resp.exit_code, 0);
    EXPECT_EQ(output_resp.data, "test output");

    // Test error response creation
    auto error_resp = SocketResponseFormatter::create_error_response(1, "789", 2, "Test error");
    EXPECT_EQ(error_resp.client_id, 1);
    EXPECT_EQ(error_resp.request_id, "789");
    EXPECT_EQ(error_resp.type, "ERROR");
    EXPECT_EQ(error_resp.exit_code, 2);
    EXPECT_EQ(error_resp.data, "Test error");
}

// Test round-trip formatting and parsing
TEST_F(SocketResponseTest, RoundTripFormatting) {
    // Test success response round-trip
    auto original1 = SocketResponseFormatter::create_success_response(1, "123", "Test message");
    std::string formatted1 = SocketResponseFormatter::format_response(original1);
    auto parsed1 = SocketResponseFormatter::parse_response(formatted1);
    
    EXPECT_EQ(parsed1.client_id, original1.client_id);
    EXPECT_EQ(parsed1.request_id, original1.request_id);
    EXPECT_EQ(parsed1.type, original1.type);
    EXPECT_EQ(parsed1.exit_code, original1.exit_code);
    EXPECT_EQ(parsed1.data, original1.data);

    // Test output response round-trip
    auto original2 = SocketResponseFormatter::create_output_response(1, "456", "test:output:with:colons");
    std::string formatted2 = SocketResponseFormatter::format_response(original2);
    auto parsed2 = SocketResponseFormatter::parse_response(formatted2);
    
    EXPECT_EQ(parsed2.client_id, original2.client_id);
    EXPECT_EQ(parsed2.request_id, original2.request_id);
    EXPECT_EQ(parsed2.type, original2.type);
    EXPECT_EQ(parsed2.exit_code, original2.exit_code);
    EXPECT_EQ(parsed2.data, original2.data);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 