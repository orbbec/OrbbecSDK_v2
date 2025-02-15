// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "src/PerformanceTester.hpp"

int main() try {
    ob::Context::setLoggerSeverity(OB_LOG_SEVERITY_INFO);
    auto ctx      = std::make_shared<ob::Context>();
    auto devList  = ctx->queryDeviceList();
    int  devCount = devList->getCount();

    if(devCount == 0) {
        std::cerr << "No devices found." << std::endl;
        std::cout << "\nPress any key to exit.";
        getchar();
        exit(EXIT_FAILURE);
    }

    PerformanceTester tester(devList);

    tester.startTesting();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    getchar();
    exit(EXIT_FAILURE);
}
