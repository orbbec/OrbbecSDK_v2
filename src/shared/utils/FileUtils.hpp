// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <functional>
#include <vector>
#include <cstdint>

namespace libobsensor {
namespace utils {
bool        fileExists(const char *file);
bool        checkDir(const char *dir);
int         mkDirs(const char *dir);
std::string getCurrentWorkDirectory();
std::string joinPaths(const std::string &parent, const std::string &fileName);
std::vector<uint8_t> readFile(const std::string &filePath);
void forEachFileInDirectory(const std::string &directory, const std::function<void(const std::string &)> &callback);
void forEachSubDirInDirectory(const std::string &directory, const std::function<void(const std::string &)> &callback);
std::string removeExtensionOfFileName(const std::string &fileName);

std::string makeRealPath(const std::string &path);

}  // namespace utils
}  // namespace libobsensor

