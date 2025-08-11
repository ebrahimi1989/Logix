#include <QCoreApplication>
#include "loggerfacade.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // Initialize logger
    Logging::LoggerFacade::getInstance().initialize();

    // Use logger
    auto logger = Logging::LoggerFacade::getInstance().getLogger();
    logger->info("Application started");
    logger->debug("This debug message may not show if level is higher");

    // Change log level dynamically
    Logging::LoggerFacade::getInstance().setLogLevel(spdlog::level::warn);
    logger->debug("This debug message should not appear");
    logger->warn("This warning message should appear");

    // Change log level again
    Logging::LoggerFacade::getInstance().setLogLevel(spdlog::level::trace);
    logger->trace("This trace message should now appear");

    // Shutdown logger before exiting
    Logging::LoggerFacade::getInstance().shutdown();

    return app.exec();
}
