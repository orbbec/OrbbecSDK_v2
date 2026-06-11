// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DynamicLibraryManager.hpp"
#include "DynamicLibraryHelper.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {

std::mutex                           DynamicLibraryManager::instanceMutex_;
std::weak_ptr<DynamicLibraryManager> DynamicLibraryManager::instanceWeakPtr_;

std::shared_ptr<DynamicLibraryManager> DynamicLibraryManager::getInstance() {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<DynamicLibraryManager>(new DynamicLibraryManager());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

DynamicLibraryManager::DynamicLibraryManager() = default;

void DynamicLibraryManager::registerLoadedCallback(const std::string &libraryName, LibraryLoadedCallback callback) {
    std::unique_lock<std::recursive_mutex> lock(libraryMutex_);
    loadedCallbacks_[libraryName] = callback;
}

std::shared_ptr<dylib> DynamicLibraryManager::getLibrary(const std::string &libraryName) {
    std::unique_lock<std::recursive_mutex> lock(libraryMutex_);
    auto                                   libIt = libMap_.find(libraryName);
    return libIt != libMap_.end() ? libIt->second : nullptr;
}

std::shared_ptr<dylib> DynamicLibraryManager::loadLibrary(const std::string &moduleLoadPath, const std::string &libraryName) {
    std::unique_lock<std::recursive_mutex> lock(libraryMutex_);
    auto                                   libIt = libMap_.find(libraryName);
    if(libIt != libMap_.end()) {
        return libIt->second;
    }

    LOG_DEBUG("Current working directory: {}", utils::getCurrentWorkDirectory());
    auto library         = std::make_shared<dylib>(moduleLoadPath.c_str(), libraryName.c_str());
    libMap_[libraryName] = library;

    auto callbackIt = loadedCallbacks_.find(libraryName);
    if(callbackIt != loadedCallbacks_.end()) {
        callbackIt->second(libraryName, library);
    }
    DynamicLibraryHelper::logExtensionsCommitHashes(libraryName, library);
    return library;
}

}  // namespace libobsensor
