// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <functional>
#include "dylib.hpp"

namespace libobsensor {

class DynamicLibraryManager {
public:
    using LibraryLoadedCallback = std::function<void(const std::string &libraryName, const std::shared_ptr<dylib> &lib)>;

    static std::shared_ptr<DynamicLibraryManager> getInstance();

    void                   registerLoadedCallback(const std::string &libraryName, LibraryLoadedCallback callback);
    std::shared_ptr<dylib> getLibrary(const std::string &libraryName);
    std::shared_ptr<dylib> loadLibrary(const std::string &moduleLoadPath, const std::string &libraryName);

private:
    DynamicLibraryManager();

    static std::mutex                             instanceMutex_;
    static std::weak_ptr<DynamicLibraryManager>   instanceWeakPtr_;
    std::recursive_mutex                          libraryMutex_;
    std::map<std::string, std::shared_ptr<dylib>> libMap_;
    std::map<std::string, LibraryLoadedCallback>  loadedCallbacks_;
};

}  // namespace libobsensor
