// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FileUtils.hpp"

#include <cstring>
#include <cstdlib>
#include <memory>
#include <limits>

#ifdef WIN32
#include <direct.h>
#include <wchar.h>
#include <io.h>
#include <Windows.h>
#include <locale>
#include <codecvt>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif

#include "logger/Logger.hpp"

namespace libobsensor {
namespace utils {

bool fileExists(const char *file) {
#ifdef WIN32
    return (_access(file, 0) == 0);
#else
    return (access(file, 0) == 0);
#endif
}

bool checkDir(const char *dir) {
#ifdef WIN32
    return (_access(dir, 0) == 0);
#else
    return (access(dir, 0) == 0);
#endif
}

int mkDirs(const char *dir) {
    size_t i, len;
    char   str[512];
    int    error = 0;

#ifdef WIN32
    // Use snprintf to ensure the string is copied safely
    snprintf(str, sizeof(str), "%s", dir);
    len = strlen(str);
    for(i = 0; i < len; i++) {
        if(str[i] == '/' || str[i] == '\\') {
            str[i] = '\0';
            if(_access(str, 0) != 0) {
                error = _mkdir(str);
                if(error != 0) {
                    return error;
                }
            }
            str[i] = '/';
        }
    }
    if(len > 0 && _access(str, 0) != 0) {
        error = _mkdir(str);
    }
    return error;
#else
    // Use snprintf to ensure the string is copied securely
    snprintf(str, sizeof(str), "%s", dir);
    len = strlen(str);
    umask(0000);
    for(i = 0; i < len; i++) {
        if(i > 0 && str[i] == '/') {
            str[i] = '\0';
            if(access(str, 0) != 0) {
                error = mkdir(str, 0755);
                if(error != 0) {
                    return error;
                }
            }
            str[i] = '/';
        }
    }
    if(len > 0 && access(str, 0) != 0) {
        error = mkdir(str, 0755);
    }
    return error;
#endif
}

std::string getCurrentWorkDirectory() {
#ifdef WIN32
    const DWORD size = GetCurrentDirectoryA(0, nullptr);
    std::string pathStr;
    pathStr.resize(size - 1);
    GetCurrentDirectoryA(size, &pathStr[0]);
    return pathStr;
#else
    char cwd[PATH_MAX] = {};
    if(getcwd(cwd, sizeof(cwd)) != nullptr) {
        return cwd;
    }
    else {
        perror("getcwd() error");
        return "";
    }
#endif
}

std::string joinPaths(const std::string &parent, const std::string &fileName) {
    char separator = '/';
#ifdef WIN32
    separator = '\\';
#endif
    std::string rst = parent;
    if(!parent.empty() && parent.back() != separator) {
        rst += separator;
    }

    return rst + fileName;
}

std::vector<uint8_t> readFile(const std::string &filePath) {
    auto pFile = fopen(filePath.c_str(), "rb");
    if(nullptr == pFile) {
        LOG_WARN("open file failed. filePath: {}", filePath);
        return {};
    }

    // obtain file size:
    fseek(pFile, 0, SEEK_END);
    auto lSize = ftell(pFile);
    rewind(pFile);

    if(lSize <= 0) {
        fclose(pFile);
        return {};
    }

    std::vector<uint8_t> data(lSize);
    auto                 result = fread(data.data(), 1, lSize, pFile);

    if(result != static_cast<size_t>(lSize)) {
        int errCode = ferror(pFile);
        if(0 != errCode) {
            LOG_WARN("Read file failed. filePath: {}, errorCode: {}", filePath, errCode);
            fclose(pFile);
            return {};
        }
    }

    fclose(pFile);
    return data;
}

void forEachFileInDirectory(const std::string &directory, const std::function<void(const std::string &)> &callback) {
#ifdef WIN32
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring                                     wdir = converter.from_bytes(directory);
    WIN32_FIND_DATAW                                 findData;
    HANDLE                                           hFind = FindFirstFileW((wdir + L"\\*").c_str(), &findData);
    if(hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        else {
            std::string fileName = converter.to_bytes(findData.cFileName);
            callback(fileName);
        }
    } while(FindNextFileW(hFind, &findData));
    FindClose(hFind);
#else
    DIR           *dir;
    struct dirent *ent;
    dir = opendir(directory.c_str());
    if(dir != nullptr) {
        while((ent = readdir(dir)) != nullptr) {
            if(ent->d_type == DT_REG) {
                callback(ent->d_name);
            }
        }
        closedir(dir);
    }
#endif
}

void forEachSubDirInDirectory(const std::string &directory, const std::function<void(const std::string &)> &callback) {
#ifdef WIN32
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring                                     wdir = converter.from_bytes(directory);
    WIN32_FIND_DATAW                                 findData;
    HANDLE                                           hFind = FindFirstFileW((wdir + L"\\*").c_str(), &findData);
    if(hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if(wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                std::string subDir = converter.to_bytes(findData.cFileName);
                callback(subDir);
            }
        }
    } while(FindNextFileW(hFind, &findData));
    FindClose(hFind);
#else
    DIR           *dir;
    struct dirent *ent;
    if((dir = opendir(directory.c_str())) != nullptr) {
        while((ent = readdir(dir)) != NULL) {
            if(ent->d_type == DT_DIR) {
                if(strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                    callback(ent->d_name);
                }
            }
        }
        closedir(dir);
    }
#endif
}

std::string removeExtensionOfFileName(const std::string &fileName) {
    size_t pos = fileName.find_last_of(".");
    if(pos == std::string::npos) {
        return fileName;
    }
    return fileName.substr(0, pos);
}

}  // namespace utils
}  // namespace libobsensor

