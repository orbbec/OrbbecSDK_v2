#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <iostream>
#include <functional>

int main() try {
    // Configure the log level output to the terminal.
    ob::Context::setLoggerToConsole(OB_LOG_SEVERITY_ERROR);

    // Configure the log level and path output to the file.
    ob::Context::setLoggerToFile(OB_LOG_SEVERITY_DEBUG, "Log/Custom/");

    // Register a log callback, you can get log information in the callback.
    ob::Context::setLoggerToCallback(OB_LOG_SEVERITY_DEBUG,
                                     [](OBLogSeverity severity, const char *logMsg) { std::cout << "[CallbackMessage][Level:" << severity << "]" << logMsg; });

    // Configure which streams to enable or disable for the Pipeline by creating a Config
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    config->enableVideoStream(OB_STREAM_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);
    config->enableVideoStream(OB_STREAM_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_RGB);

    // Create a pipeline with default device to manage stream
    std::shared_ptr<ob::Pipeline> pipe = std::make_shared<ob::Pipeline>();

    // Start the pipeline with config
    pipe->start(config);
    // Stop the Pipeline, no frame data will be generated
    pipe->stop();

    ob::Context::setLoggerToCallback(OB_LOG_SEVERITY_OFF, nullptr);
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
