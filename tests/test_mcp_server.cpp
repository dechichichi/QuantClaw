// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <memory>
#include "quantclaw/mcp/mcp_server.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>

class TestMCPTool : public quantclaw::mcp::MCPTool {
public:
    TestMCPTool() : MCPTool("test_tool", "A test tool for testing") {
        AddParameter("input", "string", "Input string", true);
    }

private:
    std::string execute(const nlohmann::json& arguments) override {
        std::string input = arguments.value("input", "");
        return "Processed: " + input;
    }
};

class MCPServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
        logger_ = std::make_shared<spdlog::logger>("test", null_sink);

        server_ = std::make_unique<quantclaw::mcp::MCPServer>(logger_);
        server_->RegisterTool(std::make_unique<TestMCPTool>());
    }

    std::shared_ptr<spdlog::logger> logger_;
    std::unique_ptr<quantclaw::mcp::MCPServer> server_;
};

TEST_F(MCPServerTest, Initialize) {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {}}
    };

    auto response = server_->HandleRequest(request);

    EXPECT_EQ(response["id"], 1);
    EXPECT_EQ(response["result"]["protocolVersion"], "2024-11-05");
}

TEST_F(MCPServerTest, ListTools) {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "tools/list"},
        {"params", {}}
    };

    auto response = server_->HandleRequest(request);

    EXPECT_EQ(response["id"], 2);
    auto tools = response["result"]["tools"];
    EXPECT_EQ(tools.size(), 1u);
    EXPECT_EQ(tools[0]["name"], "test_tool");
}

TEST_F(MCPServerTest, CallTool) {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", 3},
        {"method", "tools/call"},
        {"params", {
            {"name", "test_tool"},
            {"arguments", {{"input", "hello"}}}
        }}
    };

    auto response = server_->HandleRequest(request);

    EXPECT_EQ(response["id"], 3);
    auto content = response["result"]["content"];
    EXPECT_EQ(content[0]["text"], "Processed: hello");
}

// --- Backward compatibility: old method names still work ---

TEST_F(MCPServerTest, ListToolsLegacyName) {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", 10},
        {"method", "list_tools"},
        {"params", {}}
    };

    auto response = server_->HandleRequest(request);

    EXPECT_EQ(response["id"], 10);
    EXPECT_TRUE(response.contains("result"));
    EXPECT_EQ(response["result"]["tools"].size(), 1u);
}

TEST_F(MCPServerTest, CallToolLegacyName) {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", 11},
        {"method", "call_tool"},
        {"params", {
            {"name", "test_tool"},
            {"arguments", {{"input", "legacy"}}}
        }}
    };

    auto response = server_->HandleRequest(request);

    EXPECT_EQ(response["id"], 11);
    EXPECT_EQ(response["result"]["content"][0]["text"], "Processed: legacy");
}

// --- Tool schema uses inputSchema per MCP spec ---

TEST_F(MCPServerTest, ToolSchemaUsesInputSchema) {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", 12},
        {"method", "tools/list"},
        {"params", {}}
    };

    auto response = server_->HandleRequest(request);
    auto& tool = response["result"]["tools"][0];

    EXPECT_TRUE(tool.contains("inputSchema"));
    EXPECT_FALSE(tool.contains("parameters"));
    EXPECT_EQ(tool["inputSchema"]["type"], "object");
    EXPECT_TRUE(tool["inputSchema"]["properties"].contains("input"));
}

TEST_F(MCPServerTest, UnknownMethod) {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", 4},
        {"method", "unknown_method"},
        {"params", {}}
    };

    auto response = server_->HandleRequest(request);

    EXPECT_EQ(response["id"], 4);
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], -32601);
}
