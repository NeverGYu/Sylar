#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifndef SYLAR_PROJECT_ROOT
#define SYLAR_PROJECT_ROOT "."
#endif

#ifndef SYLAR_CHATSERVER_ROOT
#define SYLAR_CHATSERVER_ROOT SYLAR_PROJECT_ROOT "/webapps/chatServer"
#endif

#ifndef SYLAR_CHATSERVER_RESOURCE_DIR
#define SYLAR_CHATSERVER_RESOURCE_DIR SYLAR_CHATSERVER_ROOT "/resource"
#endif

#ifndef SYLAR_CHATSERVER_SSL_CERT
#define SYLAR_CHATSERVER_SSL_CERT SYLAR_PROJECT_ROOT "/bin/conf/server.crt"
#endif

#ifndef SYLAR_CHATSERVER_SSL_KEY
#define SYLAR_CHATSERVER_SSL_KEY SYLAR_PROJECT_ROOT "/bin/conf/server.key"
#endif

namespace chatserver {

inline std::string envOrDefault(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    return value;
}

inline int envIntOrDefault(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

inline bool envBoolOrDefault(const char* name, bool fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }

    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    return fallback;
}

inline std::string resourceDir() {
    return envOrDefault("CHAT_SERVER_RESOURCE_DIR", SYLAR_CHATSERVER_RESOURCE_DIR);
}

inline std::string resourcePath(const std::string& filename) {
    return (std::filesystem::path(resourceDir()) / filename).string();
}

inline std::string sslCertPath() {
    return envOrDefault("CHAT_SERVER_SSL_CERT", SYLAR_CHATSERVER_SSL_CERT);
}

inline std::string sslKeyPath() {
    return envOrDefault("CHAT_SERVER_SSL_KEY", SYLAR_CHATSERVER_SSL_KEY);
}

inline std::string aiConfigPath() {
    return envOrDefault("CHAT_SERVER_AI_CONFIG", resourcePath("config.json"));
}

inline std::string imageModelPath() {
    return envOrDefault("CHAT_SERVER_IMAGE_MODEL", "");
}

inline std::string imageLabelPath() {
    return envOrDefault("CHAT_SERVER_IMAGE_LABELS", "");
}

inline bool fileExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

inline std::string bindAddress(bool use_ssl) {
    return envOrDefault("CHAT_SERVER_BIND", use_ssl ? "0.0.0.0:8443" : "0.0.0.0:8080");
}

}  // namespace chatserver
