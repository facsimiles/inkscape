// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Socket Handshake Tests for Inkscape
 *
 * Copyright (C) 2024 Inkscape contributors
 *
 * Tests for socket server connection handshake and client management
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <regex>

// Mock handshake manager for testing
class SocketHandshakeManager {
public:
    struct HandshakeMessage {
        std::string type;  // "WELCOME" or "REJECT"
        int client_id;
        std::string message;
    };

    struct ClientInfo {
        int client_id;
        bool is_active;
        std::string connection_time;
    };

    // Parse welcome message
    static HandshakeMessage parse_welcome_message(const std::string& input) {
        HandshakeMessage msg;
        msg.client_id = 0;
        
        // Expected format: "WELCOME:Client ID X"
        std::regex welcome_pattern(R"(WELCOME:Client ID (\d+))");
        std::smatch match;
        
        if (std::regex_match(input, match, welcome_pattern)) {
            msg.type = "WELCOME";
            msg.client_id = std::stoi(match[1]);
            msg.message = "Client ID " + match[1];
        } else {
            msg.type = "UNKNOWN";
            msg.message = input;
        }
        
        return msg;
    }

    // Parse reject message
    static HandshakeMessage parse_reject_message(const std::string& input) {
        HandshakeMessage msg;
        msg.client_id = 0;
        
        // Expected format: "REJECT:Another client is already connected"
        if (input == "REJECT:Another client is already connected") {
            msg.type = "REJECT";
            msg.message = "Another client is already connected";
        } else {
            msg.type = "UNKNOWN";
            msg.message = input;
        }
        
        return msg;
    }

    // Validate welcome message
    static bool is_valid_welcome_message(const std::string& input) {
        HandshakeMessage msg = parse_welcome_message(input);
        return msg.type == "WELCOME" && msg.client_id > 0;
    }

    // Validate reject message
    static bool is_valid_reject_message(const std::string& input) {
        HandshakeMessage msg = parse_reject_message(input);
        return msg.type == "REJECT";
    }

    // Check if message is a handshake message
    static bool is_handshake_message(const std::string& input) {
        return input.find("WELCOME:") == 0 || input.find("REJECT:") == 0;
    }

    // Generate client ID (mock implementation)
    static int generate_client_id() {
        static int counter = 0;
        return ++counter;
    }

    // Check if client can connect (only one client allowed)
    static bool can_client_connect(int client_id, int& active_client_id) {
        if (active_client_id == -1) {
            active_client_id = client_id;
            return true;
        }
        return false;
    }

    // Release client connection
    static void release_client_connection(int client_id, int& active_client_id) {
        if (active_client_id == client_id) {
            active_client_id = -1;
        }
    }

    // Validate client ID format
    static bool is_valid_client_id(int client_id) {
        return client_id > 0;
    }

    // Create welcome message
    static std::string create_welcome_message(int client_id) {
        return "WELCOME:Client ID " + std::to_string(client_id);
    }

    // Create reject message
    static std::string create_reject_message() {
        return "REJECT:Another client is already connected";
    }

    // Simulate handshake process
    static HandshakeMessage perform_handshake(int client_id, int& active_client_id) {
        if (can_client_connect(client_id, active_client_id)) {
            return parse_welcome_message(create_welcome_message(client_id));
        } else {
            return parse_reject_message(create_reject_message());
        }
    }
};

// Test fixture for socket handshake tests
class SocketHandshakeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test welcome message parsing
TEST_F(SocketHandshakeTest, ParseWelcomeMessages) {
    // Test valid welcome message
    auto msg1 = SocketHandshakeManager::parse_welcome_message("WELCOME:Client ID 1");
    EXPECT_EQ(msg1.type, "WELCOME");
    EXPECT_EQ(msg1.client_id, 1);
    EXPECT_EQ(msg1.message, "Client ID 1");

    // Test welcome message with different client ID
    auto msg2 = SocketHandshakeManager::parse_welcome_message("WELCOME:Client ID 123");
    EXPECT_EQ(msg2.type, "WELCOME");
    EXPECT_EQ(msg2.client_id, 123);
    EXPECT_EQ(msg2.message, "Client ID 123");

    // Test invalid welcome message
    auto msg3 = SocketHandshakeManager::parse_welcome_message("WELCOME:Invalid format");
    EXPECT_EQ(msg3.type, "UNKNOWN");
    EXPECT_EQ(msg3.client_id, 0);

    // Test non-welcome message
    auto msg4 = SocketHandshakeManager::parse_welcome_message("COMMAND:123:status");
    EXPECT_EQ(msg4.type, "UNKNOWN");
    EXPECT_EQ(msg4.client_id, 0);
}

// Test reject message parsing
TEST_F(SocketHandshakeTest, ParseRejectMessages) {
    // Test valid reject message
    auto msg1 = SocketHandshakeManager::parse_reject_message("REJECT:Another client is already connected");
    EXPECT_EQ(msg1.type, "REJECT");
    EXPECT_EQ(msg1.client_id, 0);
    EXPECT_EQ(msg1.message, "Another client is already connected");

    // Test invalid reject message
    auto msg2 = SocketHandshakeManager::parse_reject_message("REJECT:Different message");
    EXPECT_EQ(msg2.type, "UNKNOWN");
    EXPECT_EQ(msg2.client_id, 0);

    // Test non-reject message
    auto msg3 = SocketHandshakeManager::parse_reject_message("WELCOME:Client ID 1");
    EXPECT_EQ(msg3.type, "UNKNOWN");
    EXPECT_EQ(msg3.client_id, 0);
}

// Test welcome message validation
TEST_F(SocketHandshakeTest, ValidateWelcomeMessages) {
    EXPECT_TRUE(SocketHandshakeManager::is_valid_welcome_message("WELCOME:Client ID 1"));
    EXPECT_TRUE(SocketHandshakeManager::is_valid_welcome_message("WELCOME:Client ID 123"));
    EXPECT_TRUE(SocketHandshakeManager::is_valid_welcome_message("WELCOME:Client ID 999"));
    
    EXPECT_FALSE(SocketHandshakeManager::is_valid_welcome_message("WELCOME:Invalid format"));
    EXPECT_FALSE(SocketHandshakeManager::is_valid_welcome_message("WELCOME:Client ID 0"));
    EXPECT_FALSE(SocketHandshakeManager::is_valid_welcome_message("WELCOME:Client ID -1"));
    EXPECT_FALSE(SocketHandshakeManager::is_valid_welcome_message("REJECT:Another client is already connected"));
    EXPECT_FALSE(SocketHandshakeManager::is_valid_welcome_message("COMMAND:123:status"));
}

// Test reject message validation
TEST_F(SocketHandshakeTest, ValidateRejectMessages) {
    EXPECT_TRUE(SocketHandshakeManager::is_valid_reject_message("REJECT:Another client is already connected"));
    
    EXPECT_FALSE(SocketHandshakeManager::is_valid_reject_message("REJECT:Different message"));
    EXPECT_FALSE(SocketHandshakeManager::is_valid_reject_message("WELCOME:Client ID 1"));
    EXPECT_FALSE(SocketHandshakeManager::is_valid_reject_message("COMMAND:123:status"));
}

// Test handshake message detection
TEST_F(SocketHandshakeTest, DetectHandshakeMessages) {
    EXPECT_TRUE(SocketHandshakeManager::is_handshake_message("WELCOME:Client ID 1"));
    EXPECT_TRUE(SocketHandshakeManager::is_handshake_message("REJECT:Another client is already connected"));
    
    EXPECT_FALSE(SocketHandshakeManager::is_handshake_message("COMMAND:123:status"));
    EXPECT_FALSE(SocketHandshakeManager::is_handshake_message("RESPONSE:1:123:SUCCESS:0:Command executed"));
    EXPECT_FALSE(SocketHandshakeManager::is_handshake_message(""));
    EXPECT_FALSE(SocketHandshakeManager::is_handshake_message("Some other message"));
}

// Test client ID generation
TEST_F(SocketHandshakeTest, GenerateClientIds) {
    // Reset counter for testing
    int id1 = SocketHandshakeManager::generate_client_id();
    int id2 = SocketHandshakeManager::generate_client_id();
    int id3 = SocketHandshakeManager::generate_client_id();
    
    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, id1);
    EXPECT_GT(id3, id2);
}

// Test client connection management
TEST_F(SocketHandshakeTest, ClientConnectionManagement) {
    int active_client_id = -1;
    
    // Test first client connection
    EXPECT_TRUE(SocketHandshakeManager::can_client_connect(1, active_client_id));
    EXPECT_EQ(active_client_id, 1);
    
    // Test second client connection (should be rejected)
    EXPECT_FALSE(SocketHandshakeManager::can_client_connect(2, active_client_id));
    EXPECT_EQ(active_client_id, 1); // Should still be 1
    
    // Test third client connection (should be rejected)
    EXPECT_FALSE(SocketHandshakeManager::can_client_connect(3, active_client_id));
    EXPECT_EQ(active_client_id, 1); // Should still be 1
    
    // Release first client
    SocketHandshakeManager::release_client_connection(1, active_client_id);
    EXPECT_EQ(active_client_id, -1);
    
    // Test new client connection after release
    EXPECT_TRUE(SocketHandshakeManager::can_client_connect(4, active_client_id));
    EXPECT_EQ(active_client_id, 4);
}

// Test client ID validation
TEST_F(SocketHandshakeTest, ValidateClientIds) {
    EXPECT_TRUE(SocketHandshakeManager::is_valid_client_id(1));
    EXPECT_TRUE(SocketHandshakeManager::is_valid_client_id(123));
    EXPECT_TRUE(SocketHandshakeManager::is_valid_client_id(999));
    
    EXPECT_FALSE(SocketHandshakeManager::is_valid_client_id(0));
    EXPECT_FALSE(SocketHandshakeManager::is_valid_client_id(-1));
    EXPECT_FALSE(SocketHandshakeManager::is_valid_client_id(-123));
}

// Test message creation
TEST_F(SocketHandshakeTest, CreateMessages) {
    // Test welcome message creation
    std::string welcome1 = SocketHandshakeManager::create_welcome_message(1);
    EXPECT_EQ(welcome1, "WELCOME:Client ID 1");
    
    std::string welcome2 = SocketHandshakeManager::create_welcome_message(123);
    EXPECT_EQ(welcome2, "WELCOME:Client ID 123");
    
    // Test reject message creation
    std::string reject = SocketHandshakeManager::create_reject_message();
    EXPECT_EQ(reject, "REJECT:Another client is already connected");
}

// Test handshake process simulation
TEST_F(SocketHandshakeTest, HandshakeProcess) {
    int active_client_id = -1;
    
    // Test successful handshake for first client
    auto handshake1 = SocketHandshakeManager::perform_handshake(1, active_client_id);
    EXPECT_EQ(handshake1.type, "WELCOME");
    EXPECT_EQ(handshake1.client_id, 1);
    EXPECT_EQ(active_client_id, 1);
    
    // Test failed handshake for second client
    auto handshake2 = SocketHandshakeManager::perform_handshake(2, active_client_id);
    EXPECT_EQ(handshake2.type, "REJECT");
    EXPECT_EQ(handshake2.client_id, 0);
    EXPECT_EQ(active_client_id, 1); // Should still be 1
    
    // Release first client
    SocketHandshakeManager::release_client_connection(1, active_client_id);
    EXPECT_EQ(active_client_id, -1);
    
    // Test successful handshake for new client
    auto handshake3 = SocketHandshakeManager::perform_handshake(3, active_client_id);
    EXPECT_EQ(handshake3.type, "WELCOME");
    EXPECT_EQ(handshake3.client_id, 3);
    EXPECT_EQ(active_client_id, 3);
}

// Test multiple client scenarios
TEST_F(SocketHandshakeTest, MultipleClientScenarios) {
    int active_client_id = -1;
    
    // Scenario 1: Multiple clients trying to connect
    EXPECT_TRUE(SocketHandshakeManager::can_client_connect(1, active_client_id));
    EXPECT_EQ(active_client_id, 1);
    
    EXPECT_FALSE(SocketHandshakeManager::can_client_connect(2, active_client_id));
    EXPECT_EQ(active_client_id, 1);
    
    EXPECT_FALSE(SocketHandshakeManager::can_client_connect(3, active_client_id));
    EXPECT_EQ(active_client_id, 1);
    
    // Scenario 2: Release and reconnect
    SocketHandshakeManager::release_client_connection(1, active_client_id);
    EXPECT_EQ(active_client_id, -1);
    
    EXPECT_TRUE(SocketHandshakeManager::can_client_connect(4, active_client_id));
    EXPECT_EQ(active_client_id, 4);
    
    // Scenario 3: Try to release non-active client
    SocketHandshakeManager::release_client_connection(1, active_client_id);
    EXPECT_EQ(active_client_id, 4); // Should remain unchanged
    
    // Scenario 4: Release active client
    SocketHandshakeManager::release_client_connection(4, active_client_id);
    EXPECT_EQ(active_client_id, -1);
}

// Test edge cases
TEST_F(SocketHandshakeTest, EdgeCases) {
    int active_client_id = -1;
    
    // Test with client ID 0 (invalid)
    EXPECT_FALSE(SocketHandshakeManager::is_valid_client_id(0));
    
    // Test with negative client ID (invalid)
    EXPECT_FALSE(SocketHandshakeManager::is_valid_client_id(-1));
    
    // Test releasing when no client is active
    SocketHandshakeManager::release_client_connection(1, active_client_id);
    EXPECT_EQ(active_client_id, -1);
    
    // Test connecting with invalid client ID
    EXPECT_FALSE(SocketHandshakeManager::is_valid_client_id(0));
    EXPECT_FALSE(SocketHandshakeManager::is_valid_client_id(-1));
}

// Test message format consistency
TEST_F(SocketHandshakeTest, MessageFormatConsistency) {
    // Test that created messages can be parsed back
    std::string welcome = SocketHandshakeManager::create_welcome_message(123);
    auto parsed_welcome = SocketHandshakeManager::parse_welcome_message(welcome);
    EXPECT_EQ(parsed_welcome.type, "WELCOME");
    EXPECT_EQ(parsed_welcome.client_id, 123);
    
    std::string reject = SocketHandshakeManager::create_reject_message();
    auto parsed_reject = SocketHandshakeManager::parse_reject_message(reject);
    EXPECT_EQ(parsed_reject.type, "REJECT");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 