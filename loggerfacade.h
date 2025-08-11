#pragma once
#include <spdlog/spdlog.h>
#include <memory>
#include <vector>
#include <string>

namespace Logging {

class LoggerFacade {
public:
    // Singleton instance
    static LoggerFacade& getInstance();

    // Initialize logger based on environment variables
    void initialize();

    // Get the logger for use
    std::shared_ptr<spdlog::logger> getLogger() const;

    // Change log level dynamically at runtime
    void setLogLevel(spdlog::level::level_enum level);

    // Shutdown logger to clean up resources
    void shutdown();

private:
    LoggerFacade() = default;
    ~LoggerFacade() = default;

    // Prevent copy/move
    LoggerFacade(const LoggerFacade&) = delete;
    LoggerFacade& operator=(const LoggerFacade&) = delete;

    std::shared_ptr<spdlog::logger> logger_;
    bool isInitialized_ = false;
    std::vector<spdlog::sink_ptr> sinks_; // Store sinks for dynamic level changes
};

// Configuration class to handle environment variables
struct LoggerConfig {
    std::vector<std::string> logModes; // List of modes: none, file, network
    std::string filePath;
    std::string networkIp;
    uint16_t networkPort = 0;
    std::string logLevel = "debug";
    std::string logPattern = "%Y-%m-%d %H:%M:%S.%e [%n] [%l] %v"; // Default pattern
    std::string udpFormat = "json"; // Default: JSON for UDP sink

    static LoggerConfig loadFromEnv();
};

} // namespace Logging
