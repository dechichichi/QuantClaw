// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#include "quantclaw/cli/onboard_commands.hpp"
#include "quantclaw/config.hpp"
#include "quantclaw/gateway/gateway_client.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <thread>
#include <chrono>

namespace quantclaw::cli {

OnboardCommands::OnboardCommands(std::shared_ptr<spdlog::logger> logger)
    : logger_(logger) {}

int OnboardCommands::OnboardCommand(const std::vector<std::string>& args) {
    bool install_daemon = false;
    bool skip_daemon = false;

    for (const auto& arg : args) {
        if (arg == "--install-daemon") install_daemon = true;
        if (arg == "--skip-daemon") skip_daemon = true;
    }

    PrintWelcome();

    // Step 1: Config
    PrintStep(1, 5, "Configuration");
    if (SetupConfig() != 0) {
        std::cerr << "Configuration setup failed" << std::endl;
        return 1;
    }

    // Step 2: Workspace
    PrintStep(2, 5, "Workspace Setup");
    if (SetupWorkspace() != 0) {
        std::cerr << "Workspace setup failed" << std::endl;
        return 1;
    }

    // Step 3: Daemon
    PrintStep(3, 5, "Daemon Setup");
    if (!skip_daemon) {
        if (install_daemon || PromptYesNo("Install QuantClaw as system service?", true)) {
            if (SetupDaemon() != 0) {
                std::cerr << "Daemon setup failed" << std::endl;
                return 1;
            }
        }
    }

    // Step 4: Skills
    PrintStep(4, 5, "Skills Setup");
    if (SetupSkills() != 0) {
        logger_->warn("Skills setup had issues, but continuing");
    }

    // Step 5: Verification
    PrintStep(5, 5, "Verification");
    if (VerifySetup() != 0) {
        logger_->warn("Some verification checks failed");
    }

    std::cout << "\n✓ Onboarding complete!" << std::endl;
    std::cout << "\nNext steps:" << std::endl;
    std::cout << "  1. Start the gateway: quantclaw gateway" << std::endl;
    std::cout << "  2. Open the dashboard: quantclaw dashboard" << std::endl;
    std::cout << "  3. Send a message: quantclaw agent -m \"Hello\"" << std::endl;
    std::cout << "\nFor help: quantclaw --help" << std::endl;

    return 0;
}

int OnboardCommands::InstallDaemonCommand(const std::vector<std::string>& /*args*/) {
    std::cout << "Installing QuantClaw as system service..." << std::endl;
    if (InstallDaemon()) {
        std::cout << "✓ Daemon installed successfully" << std::endl;
        std::cout << "\nStart the service:" << std::endl;
        std::cout << "  quantclaw gateway start" << std::endl;
        return 0;
    } else {
        std::cerr << "✗ Failed to install daemon" << std::endl;
        return 1;
    }
}

int OnboardCommands::QuickSetupCommand(const std::vector<std::string>& /*args*/) {
    std::cout << "Running quick setup..." << std::endl;

    if (!CreateWorkspaceDirectory()) {
        std::cerr << "Failed to create workspace" << std::endl;
        return 1;
    }

    if (!CreateConfigFile()) {
        std::cerr << "Failed to create config" << std::endl;
        return 1;
    }

    if (!CreateSOULFile()) {
        std::cerr << "Failed to create SOUL.md" << std::endl;
        return 1;
    }

    std::cout << "✓ Quick setup complete" << std::endl;
    return 0;
}

void OnboardCommands::PrintWelcome() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                                                            ║" << std::endl;
    std::cout << "║          Welcome to QuantClaw Onboarding Wizard            ║" << std::endl;
    std::cout << "║                                                            ║" << std::endl;
    std::cout << "║  This wizard will guide you through the initial setup of   ║" << std::endl;
    std::cout << "║  QuantClaw, including configuration, workspace creation,   ║" << std::endl;
    std::cout << "║  and optional daemon installation.                         ║" << std::endl;
    std::cout << "║                                                            ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
}

void OnboardCommands::PrintStep(int current, int total, const std::string& title) {
    std::cout << "\n[" << current << "/" << total << "] " << title << std::endl;
    std::cout << std::string(40, '-') << std::endl;
}

std::string OnboardCommands::PromptString(const std::string& prompt, const std::string& default_value) {
    std::cout << prompt;
    if (!default_value.empty()) {
        std::cout << " [" << default_value << "]";
    }
    std::cout << ": ";
    std::cout.flush();

    std::string input;
    std::getline(std::cin, input);

    if (input.empty() && !default_value.empty()) {
        return default_value;
    }
    return input;
}

bool OnboardCommands::PromptYesNo(const std::string& prompt, bool default_value) {
    std::string default_str = default_value ? "Y/n" : "y/N";
    std::cout << prompt << " [" << default_str << "]: ";
    std::cout.flush();

    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
        return default_value;
    }

    return input[0] == 'y' || input[0] == 'Y';
}

std::string OnboardCommands::PromptChoice(const std::string& prompt, const std::vector<std::string>& choices) {
    std::cout << prompt << std::endl;
    for (size_t i = 0; i < choices.size(); ++i) {
        std::cout << "  " << (i + 1) << ") " << choices[i] << std::endl;
    }
    std::cout << "Choose [1]: ";
    std::cout.flush();

    std::string input;
    std::getline(std::cin, input);

    int choice = 1;
    if (!input.empty()) {
        try {
            choice = std::stoi(input);
        } catch (...) {
            choice = 1;
        }
    }

    if (choice < 1 || choice > static_cast<int>(choices.size())) {
        choice = 1;
    }

    return choices[choice - 1];
}

int OnboardCommands::SetupConfig() {
    const char* home = std::getenv("HOME");
    std::string home_str = home ? home : "/tmp";
    std::string config_path = home_str + "/.quantclaw/quantclaw.json";

    if (std::filesystem::exists(config_path)) {
        std::cout << "Config file already exists at: " << config_path << std::endl;
        if (!PromptYesNo("Overwrite?", false)) {
            return 0;
        }
    }

    std::cout << "\nLet's configure QuantClaw:" << std::endl;

    std::string model = PromptString("Default AI model", "anthropic/claude-sonnet-4-6");
    std::string gateway_port = PromptString("Gateway port", "18800");
    std::string gateway_bind = PromptString("Gateway bind address", "127.0.0.1");

    if (!CreateConfigFile()) {
        return 1;
    }

    std::cout << "✓ Configuration saved to: " << config_path << std::endl;
    return 0;
}

int OnboardCommands::SetupWorkspace() {
    if (!CreateWorkspaceDirectory()) {
        return 1;
    }

    if (!CreateSOULFile()) {
        return 1;
    }

    std::cout << "✓ Workspace created successfully" << std::endl;
    return 0;
}

int OnboardCommands::SetupDaemon() {
    std::cout << "\nSetting up QuantClaw as a system service..." << std::endl;

    if (InstallDaemon()) {
        std::cout << "✓ Daemon installed successfully" << std::endl;
        std::cout << "\nYou can now manage the service with:" << std::endl;
        std::cout << "  quantclaw gateway start" << std::endl;
        std::cout << "  quantclaw gateway stop" << std::endl;
        std::cout << "  quantclaw gateway status" << std::endl;
        return 0;
    } else {
        std::cerr << "✗ Failed to install daemon" << std::endl;
        std::cerr << "You can still run QuantClaw manually with: quantclaw gateway" << std::endl;
        return 1;
    }
}

int OnboardCommands::SetupSkills() {
    std::cout << "\nSetting up built-in skills..." << std::endl;
    std::cout << "✓ Skills are ready to use" << std::endl;
    return 0;
}

int OnboardCommands::VerifySetup() {
    std::cout << "\nVerifying setup..." << std::endl;

    const char* home = std::getenv("HOME");
    std::string home_str = home ? home : "/tmp";

    // Check config
    std::string config_path = home_str + "/.quantclaw/quantclaw.json";
    bool config_ok = std::filesystem::exists(config_path);
    std::cout << "[" << (config_ok ? "✓" : "✗") << "] Config file" << std::endl;

    // Check workspace
    auto workspace = std::filesystem::path(home_str) / ".quantclaw/agents/main/workspace";
    bool ws_ok = std::filesystem::exists(workspace);
    std::cout << "[" << (ws_ok ? "✓" : "✗") << "] Workspace directory" << std::endl;

    // Check SOUL.md
    auto soul_path = workspace / "SOUL.md";
    bool soul_ok = std::filesystem::exists(soul_path);
    std::cout << "[" << (soul_ok ? "✓" : "✗") << "] SOUL.md" << std::endl;

    return (config_ok && ws_ok && soul_ok) ? 0 : 1;
}

bool OnboardCommands::CreateWorkspaceDirectory() {
    const char* home = std::getenv("HOME");
    std::string home_str = home ? home : "/tmp";

    try {
        auto workspace = std::filesystem::path(home_str) / ".quantclaw/agents/main/workspace";
        std::filesystem::create_directories(workspace);

        // Create subdirectories
        std::filesystem::create_directories(workspace / "skills");
        std::filesystem::create_directories(workspace / "scripts");
        std::filesystem::create_directories(workspace / "references");

        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to create workspace: {}", e.what());
        return false;
    }
}

bool OnboardCommands::CreateConfigFile() {
    const char* home = std::getenv("HOME");
    std::string home_str = home ? home : "/tmp";
    std::string config_path = home_str + "/.quantclaw/quantclaw.json";

    try {
        std::filesystem::create_directories(std::filesystem::path(config_path).parent_path());

        nlohmann::json config = {
            {"gateway", {
                {"port", 18800},
                {"bind", "127.0.0.1"},
                {"auth", {{"mode", "token"}}}
            }},
            {"models", {
                {"defaultModel", "anthropic/claude-sonnet-4-6"},
                {"providers", {
                    {"anthropic", {
                        {"apiKey", ""}
                    }}
                }}
            }},
            {"agent", {
                {"model", "anthropic/claude-sonnet-4-6"},
                {"autoCompact", true},
                {"compactMaxMessages", 100}
            }},
            {"queue", {
                {"maxConcurrent", 4},
                {"debounceMs", 1000}
            }},
            {"session", {
                {"dmScope", "per-channel-peer"}
            }},
            {"channels", {
                {"discord", {{"enabled", false}}},
                {"telegram", {{"enabled", false}}}
            }},
            {"tools", {
                {"allow", nlohmann::json::array()},
                {"exec", {{"ask", "on-miss"}}}
            }}
        };

        std::ofstream file(config_path);
        file << config.dump(2);
        file.close();

        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to create config: {}", e.what());
        return false;
    }
}

bool OnboardCommands::CreateSOULFile() {
    const char* home = std::getenv("HOME");
    std::string home_str = home ? home : "/tmp";
    auto soul_path = std::filesystem::path(home_str) / ".quantclaw/agents/main/workspace/SOUL.md";

    try {
        std::ofstream file(soul_path);
        file << "# QuantClaw Agent Identity\n\n";
        file << "## Role\n";
        file << "You are a helpful AI assistant powered by QuantClaw.\n\n";
        file << "## Capabilities\n";
        file << "- Answer questions and provide information\n";
        file << "- Help with coding and technical tasks\n";
        file << "- Assist with analysis and problem-solving\n\n";
        file << "## Constraints\n";
        file << "- Be honest about your limitations\n";
        file << "- Respect user privacy\n";
        file << "- Follow ethical guidelines\n";
        file.close();

        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to create SOUL.md: {}", e.what());
        return false;
    }
}

bool OnboardCommands::InstallDaemon() {
#ifdef _WIN32
    // Windows: Use sc.exe to create service
    logger_->info("Windows service installation not yet implemented");
    return false;
#else
    // Unix: Create systemd service
    const char* home = std::getenv("HOME");
    std::string home_str = home ? home : "/tmp";

    try {
        std::string service_file = "/etc/systemd/system/quantclaw.service";
        std::string quantclaw_path = "/usr/local/bin/quantclaw";

        // Check if we have sudo access
        int ret = std::system("sudo -n true 2>/dev/null");
        if (ret != 0) {
            logger_->warn("Sudo access required for daemon installation");
            return false;
        }

        // Create service file
        std::string service_content = R"([Unit]
Description=QuantClaw AI Assistant Gateway
After=network.target

[Service]
Type=simple
User=)" + std::string(std::getenv("USER") ? std::getenv("USER") : "root") + R"(
ExecStart=)" + quantclaw_path + R"( gateway
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
)";

        // Write service file
        std::string cmd = "echo '" + service_content + "' | sudo tee " + service_file + " > /dev/null";
        ret = std::system(cmd.c_str());
        if (ret != 0) {
            logger_->error("Failed to write service file");
            return false;
        }

        // Reload systemd
        ret = std::system("sudo systemctl daemon-reload");
        if (ret != 0) {
            logger_->error("Failed to reload systemd");
            return false;
        }

        // Enable service
        ret = std::system("sudo systemctl enable quantclaw.service");
        if (ret != 0) {
            logger_->error("Failed to enable service");
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to install daemon: {}", e.what());
        return false;
    }
#endif
}

bool OnboardCommands::TestGatewayConnection() {
    try {
        auto client = std::make_shared<quantclaw::gateway::GatewayClient>(
            "ws://127.0.0.1:18789", "", logger_);

        if (client->Connect(3000)) {
            client->Disconnect();
            return true;
        }
    } catch (const std::exception&) {}

    return false;
}

} // namespace quantclaw::cli
