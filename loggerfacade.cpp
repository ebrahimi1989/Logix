#include "loggerfacade.h"
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/pattern_formatter.h>
#include <QUdpSocket>
#include <QHostAddress>
#include <QString>
#include <cstdlib>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace Logging {

// Custom UDP sink for network logging with JSON or plain text support
class UdpSink : public spdlog::sinks::sink {
public:
    UdpSink(const std::string& host, uint16_t port, const std::string& pattern, const std::string& udp_format)
        : host_(host), port_(port), udp_format_(udp_format) {
        if (host_.empty() || port_ == 0) {
            throw std::invalid_argument("Invalid UDP sink configuration: host or port is empty");
        }
        // Set formatter with provided pattern
        formatter_ = std::make_unique<spdlog::pattern_formatter>(pattern);
    }

    void log(const spdlog::details::log_msg& msg) override {
        // Format the message
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        std::string plain_msg = fmt::to_string(formatted);

        // Create socket in the worker thread if not already created
        if (!socket_) {
            socket_ = std::make_unique<QUdpSocket>();
        }

        // Send as JSON or plain text based on udp_format_
        if (udp_format_ == "json") {
            // Calculate time components without fmt
            auto dur = msg.time.time_since_epoch();
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur);
            auto tp_whole_sec = std::chrono::system_clock::time_point(secs);
            auto t = std::chrono::system_clock::to_time_t(tp_whole_sec);
            std::tm bt = *std::localtime(&t);  // Use localtime to match fmt's default behavior for chrono
            std::ostringstream ss;
            ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur - secs).count();
            ss << '.' << std::setfill('0') << std::setw(3) << ms;
            std::string time_str = ss.str();

            // Convert to JSON
            nlohmann::json json_msg = {
                {"time", time_str},
                {"level", spdlog::level::to_string_view(msg.level).data()},
                {"logger", msg.logger_name.data()},
                {"message", plain_msg}
            };
            std::string json_str = json_msg.dump();

            // Send JSON message over UDP
            QString qHost = QString::fromStdString(host_);
            socket_->writeDatagram(json_str.c_str(), json_str.size(), QHostAddress(qHost), port_);
        } else {
            // Send plain text message over UDP
            QString qHost = QString::fromStdString(host_);
            socket_->writeDatagram(plain_msg.c_str(), plain_msg.size(), QHostAddress(qHost), port_);
        }
    }

    void flush() override {}

    void set_pattern(const std::string& pattern) override {
        formatter_ = std::make_unique<spdlog::pattern_formatter>(pattern);
    }

    void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) override {
        formatter_ = std::move(sink_formatter);
    }

    void set_level(spdlog::level::level_enum level) {
        level_ = level;
    }

    spdlog::level::level_enum level() const {
        return level_;
    }

private:
    std::string host_;
    uint16_t port_;
    std::string udp_format_; // "json" or "plain"
    std::unique_ptr<QUdpSocket> socket_; // Initialized lazily in log()
    spdlog::level::level_enum level_ = spdlog::level::trace;
    std::unique_ptr<spdlog::formatter> formatter_;
};

// Load configuration from environment variables
LoggerConfig LoggerConfig::loadFromEnv() {
    LoggerConfig config;

    // Parse LOG_MODE as a comma-separated list
    const char* modeStr = std::getenv("LOG_MODE");
    if (modeStr) {
        std::string modes = modeStr;
        std::string mode;
        std::stringstream ss(modes);
        while (std::getline(ss, mode, ',')) {
            if (!mode.empty()) {
                config.logModes.push_back(mode);
            }
        }
    }
    if (config.logModes.empty()) {
        config.logModes.push_back("none");
    }

    const char* filePathStr = std::getenv("LOG_FILE_PATH");
    if (filePathStr) {
        config.filePath = filePathStr;
    }

    const char* ipStr = std::getenv("LOG_NETWORK_IP");
    if (ipStr) {
        config.networkIp = ipStr;
    }

    const char* fileSizeStr = std::getenv("LOG_FILE_SIZE_MB");
    if (fileSizeStr) {
        try {
            int size = std::stoi(fileSizeStr);
            if (size > 0) {
                config.fileSizeMb = static_cast<size_t>(size);
            } else {
                spdlog::warn("LOG_FILE_SIZE_MB must be a positive number. Using default {}MB.", config.fileSizeMb);
            }
        } catch (const std::exception& e) {
            spdlog::warn("Invalid LOG_FILE_SIZE_MB value: {}. Using default {}MB.", fileSizeStr, config.fileSizeMb);
        }
    }

    const char* numberOfLogFilesStr = std::getenv("LOG_NIMBER_OF_LOG_FILES");
    if (numberOfLogFilesStr) {
        try {
            int size = std::stoi(numberOfLogFilesStr);
            if (size > 0) {
                config.numberOfLogFiles = static_cast<size_t>(size);
            } else {
                spdlog::warn("LOG_NIMBER_OF_LOG_FILES number of log files. Using default {}.", config.numberOfLogFiles);
            }
        } catch (const std::exception& e) {
            spdlog::warn("Invalid LOG_NIMBER_OF_LOG_FILES value: {}. Using default {}.", numberOfLogFilesStr, config.numberOfLogFiles);
        }
    }

    const char* portStr = std::getenv("LOG_NETWORK_PORT");
    if (portStr) {
        try {
            config.networkPort = static_cast<uint16_t>(std::stoi(portStr));
        } catch (const std::exception& e) {
            spdlog::warn("Invalid LOG_NETWORK_PORT value: {}. Using default (0).", portStr);
        }
    }

    const char* levelStr = std::getenv("LOG_LEVEL");
    if (levelStr) {
        config.logLevel = levelStr;
    }

    const char* patternStr = std::getenv("LOG_PATTERN");
    if (patternStr) {
        config.logPattern = patternStr;
    }

    const char* udpFormatStr = std::getenv("LOG_UDP_FORMAT");
    if (udpFormatStr) {
        config.udpFormat = udpFormatStr;
        // Validate udpFormat
        if (config.udpFormat != "json" && config.udpFormat != "plain") {
            spdlog::warn("Invalid LOG_UDP_FORMAT value: {}. Using default (json).", udpFormatStr);
            config.udpFormat = "json";
        }
    }

    return config;
}

// LoggerFacade implementation
LoggerFacade& LoggerFacade::getInstance() {
    static LoggerFacade instance;
    return instance;
}

void LoggerFacade::initialize() {
    if (isInitialized_) {
        spdlog::warn("Logger already initialized. Skipping re-initialization.");
        return;
    }

    try {
        // Initialize thread pool explicitly
        spdlog::init_thread_pool(8192, 1); // 8192 queue size, 1 thread
        LoggerConfig config = LoggerConfig::loadFromEnv();
        sinks_.clear();

        // Convert string log level to enum
        spdlog::level::level_enum logLevel = spdlog::level::from_str(config.logLevel);

        // Check if "none" is the only mode
        if (config.logModes.size() == 1 && config.logModes[0] == "none") {
            // Null sink for disabled logging
            auto nullSink = std::make_shared<spdlog::sinks::null_sink_mt>();
            nullSink->set_level(spdlog::level::off);
            logger_ = std::make_shared<spdlog::logger>("null_logger", nullSink);
            logger_->set_level(spdlog::level::off);
            sinks_.push_back(nullSink);
            spdlog::info("Logger initialized. Mode: none");
        } else {
            // Console sink (always included for visibility)
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_level(logLevel);
            consoleSink->set_pattern(config.logPattern);
            sinks_.push_back(consoleSink);

            // Process each mode
            for (const auto& mode : config.logModes) {
                if (mode == "file") {
                    if (config.filePath.empty()) {
                        spdlog::warn("LOG_FILE_PATH not set for file mode. Skipping file sink.");
                    } else {
                        try {
                            // Check if parent directory exists and is writable
                            std::filesystem::path filePath(config.filePath);
                            std::filesystem::path parentPath = filePath.parent_path();
                            if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
                                std::filesystem::create_directories(parentPath);
                                spdlog::info("Created parent directory: {}", parentPath.string());
                            }

                            // Check if file is writable
                            std::ofstream testFile(config.filePath, std::ios::app);
                            if (!testFile.is_open()) {
                                throw std::runtime_error("Cannot open file for writing: Permission denied or invalid path");
                            }
                            testFile << "Test write to file" << std::endl;
                            testFile.close();

                            // Use rotating file sink
                            auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                                config.filePath, 1024 * 1024 * config.fileSizeMb, 3); // 5MB, 3 files
                            fileSink->set_level(logLevel);
                            fileSink->set_pattern(config.logPattern);
                            fileSink->log(spdlog::details::log_msg("", spdlog::level::info, "Initial test log to file"));
                            fileSink->flush();
                            sinks_.push_back(fileSink);
                        } catch (const std::exception& e) {
                            spdlog::error("Failed to initialize file sink for '{}': {}", config.filePath, e.what());
                        }
                    }
                } else if (mode == "network") {
                    if (config.networkIp.empty() || config.networkPort == 0) {
                        spdlog::warn("Invalid network configuration (IP or port missing). Skipping network sink.");
                    } else {
                        try {
                            auto udpSink = std::make_shared<UdpSink>(config.networkIp, config.networkPort, config.logPattern, config.udpFormat);
                            udpSink->set_level(logLevel);
                            sinks_.push_back(udpSink);
                        } catch (const std::invalid_argument& e) {
                            spdlog::error("Failed to initialize UDP sink: {}", e.what());
                        }
                    }
                }
            }

            // Create async logger
            logger_ = std::make_shared<spdlog::async_logger>(
                "async_logger",
                sinks_.begin(),
                sinks_.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);
            logger_->set_level(logLevel);
            logger_->set_pattern(config.logPattern);
            // Ensure logs are flushed immediately for debugging
            logger_->flush_on(spdlog::level::trace);
            // Convert logModes to a comma-separated string manually
            std::string modes_str;
            for (size_t i = 0; i < config.logModes.size(); ++i) {
                modes_str += config.logModes[i];
                if (i < config.logModes.size() - 1) {
                    modes_str += ", ";
                }
            }
            spdlog::info("Logger initialized. Modes: {}, File: {}, Network: {}:{}, Level: {}, UDP Format: {}",
                         modes_str, config.filePath, config.networkIp, config.networkPort, config.logLevel, config.udpFormat);
        }

        spdlog::set_default_logger(logger_);
        isInitialized_ = true;
        // Flush logger to ensure initialization message is written
        logger_->flush();
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize logger: {}", e.what());
        // Fallback to null logger
        logger_ = std::make_shared<spdlog::logger>("null_logger",
                                                  std::make_shared<spdlog::sinks::null_sink_mt>());
        spdlog::set_default_logger(logger_);
        sinks_.clear();
        sinks_.push_back(std::make_shared<spdlog::sinks::null_sink_mt>());
        isInitialized_ = true;
    }
}

void LoggerFacade::setLogLevel(spdlog::level::level_enum level) {
    if (!isInitialized_) {
        throw std::runtime_error("Logger not initialized. Call initialize() first.");
    }

    try {
        logger_->set_level(level);
        for (auto& sink : sinks_) {
            sink->set_level(level);
        }
        spdlog::info("Log level changed to: {}", spdlog::level::to_string_view(level));
        // Flush logger after changing log level
        logger_->flush();
    } catch (const std::exception& e) {
        spdlog::error("Failed to set log level: {}", e.what());
    }
}

void LoggerFacade::shutdown() {
    if (isInitialized_) {
        // Flush all sinks before shutdown
        for (auto& sink : sinks_) {
            sink->flush();
        }
        logger_->flush();
        spdlog::shutdown(); // Clean up thread pool and logger
        logger_.reset();
        sinks_.clear();
        isInitialized_ = false;
        // Use console output as logger is shut down
//        std::cout << "[info] Logger shutdown completed." << std::endl;
    }
}

std::shared_ptr<spdlog::logger> LoggerFacade::getLogger() const {
    if (!isInitialized_) {
        throw std::runtime_error("Logger not initialized. Call initialize() first.");
    }
    return logger_;
}

} // namespace Logging
