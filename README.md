# Logix 

A powerful, asynchronous, and highly configurable logging facade for C++/Qt applications, built on top of the excellent [spdlog](https://github.com/gabime/spdlog) library.

Logix simplifies logging by providing a clean interface that can be configured entirely through environment variables, allowing you to direct logs to the console, rotating files, and a network endpoint (UDP) simultaneously without changing a single line of code.

---

## ✨ Key Features

* **🚀 Asynchronous Logging**: All logging operations are handled in a background thread to ensure your application's performance is never impacted.
* **⚙️ Zero-Code Configuration**: Configure everything from log level to output sinks using environment variables. Perfect for containerized environments like Docker.
* **🎨 Multiple Sinks**:
    * **Console**: Color-coded console output for easy reading.
    * **Rotating File**: Automatically manages log files, preventing them from growing indefinitely.
    * **Network (UDP)**: Stream logs in plain text or structured JSON format to a central logging server (e.g., Graylog, ELK Stack).
* **🔧 Dynamic Log Level**: Change the log verbosity at runtime without restarting your application.
* **🧱 Structured Logging**: Easily log C++ structs or other data as JSON strings, making your logs machine-readable.
* **🎯 Singleton Access**: A globally accessible instance makes logging available from anywhere in your codebase.

---

## 🛠️ Getting Started

### Dependencies

* [**Qt 5/6**](https://www.qt.io/) (Core & Network modules)
* [**spdlog**](https://github.com/gabime/spdlog) (Included in this repo)
* [**nlohmann/json**](https://github.com/nlohmann/json) (Included in this repo)

### Setup

1.  Clone this repository.
2.  Ensure the `spdlog` and `nlohmann` directories are in your project's root.
3.  Your `.pro` (qmake project) file should point to the include paths using relative addresses:

    ```pro
    QT += network
    CONFIG += c++17 console

    # Library Paths
    INCLUDEPATH += $$PWD/spdlog/include
    INCLUDEPATH += $$PWD/nlohmann/include

    # Sources and Headers
    SOURCES += main.cpp loggerfacade.cpp
    HEADERS += loggerfacade.h
    ```
4.  Build and run your project!

---

## 💻 Usage

Using Logix is straightforward. Initialize it once, get the logger instance, and start logging.

```cpp
#include "loggerfacade.h"
#include <nlohmann/json.hpp>

// Example struct for structured logging
struct UserData {
    int userId;
    std::string username;
    std::string action;
};

// Helper to convert the struct to json
void to_json(nlohmann::json& j, const UserData& u) {
    j = nlohmann::json{
        {"userId", u.userId},
        {"username", u.username},
        {"action", u.action}
    };
}

int main(int argc, char* argv[]) {
    // 1. Initialize the logger on application start
    Logging::LoggerFacade::getInstance().initialize();

    // 2. Get the logger instance
    auto logger = Logging::LoggerFacade::getInstance().getLogger();

    // 3. Log simple messages
    logger->info("Application has started.");
    logger->warn("Configuration value is missing, using default.");

    // 4. Log structured data
    UserData loginEvent = {101, "admin", "login_success"};
    nlohmann::json eventJson = loginEvent;
    logger->info("User event: {}", eventJson.dump());

    // 5. Change log level dynamically if needed
    Logging::LoggerFacade::getInstance().setLogLevel(spdlog::level::debug);
    logger->debug("This is a detailed debug message.");

    // 6. Shutdown the logger before exit
    Logging::LoggerFacade::getInstance().shutdown();

    return 0;
}
```

---

## ⚙️ Configuration

Configure the logger by setting these environment variables before running your application.

| Variable             | Description                                                                                             | Example                                             | Default             |
| -------------------- | ------------------------------------------------------------------------------------------------------- | --------------------------------------------------- | ------------------- |
| `LOG_MODE`           | A comma-separated list of active sinks. Options: `console`, `file`, `network`. `none` disables logging. | `file,network`                                      | `none`              |
| `LOG_LEVEL`          | The minimum level of logs to record. Options: `trace`, `debug`, `info`, `warn`, `error`, `critical`.     | `debug`                                             | `debug`             |
| `LOG_FILE_PATH`      | The full path for the log file if `file` mode is active.                                                | `/var/log/my_app.log`                               | (none)              |
| `LOG_NETWORK_IP`     | The IP address for the UDP sink if `network` mode is active.                                            | `127.0.0.1`                                         | (none)              |
| `LOG_NETWORK_PORT`   | The port for the UDP sink.                                                                              | `12201`                                             | `0`                 |
| `LOG_UDP_FORMAT`     | The format for UDP messages. Options: `json` or `plain`.                                                | `json`                                              | `json`              |
| `LOG_PATTERN`        | The pattern for formatting log messages. See spdlog's documentation for syntax.                         | `%Y-%m-%d %H:%M:%S.%e [%l] %v`                      | (Default pattern)   |
| `LOG_FILE_SIZE_MB`        | Sets the maximum size for a single log file in megabytes (MB). Once this size is reached, the file is rotated.	                         | 10                      | 1   |
| `LOG_NUMBER_OF_LOGS`        | Defines the total number of log files to keep (1 active + N-1 archives). The oldest file is deleted on rotation.                      | 5                       | 3   |

**Example Bash export:**
```bash
export LOG_MODE=console,file
export LOG_LEVEL=info
export LOG_FILE_PATH="/tmp/myapp.log"
./your_application
```

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
