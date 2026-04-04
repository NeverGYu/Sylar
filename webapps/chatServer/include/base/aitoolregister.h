#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <ctime>
#include <nlohmann/json.hpp>

#ifdef SYLAR_ENABLE_AI_CURL
#include <curl/curl.h>
#endif

class AIToolRegistry {
public:
    using ToolFunc = std::function<nlohmann::json(const nlohmann::json&)>;

    AIToolRegistry();

    void registerTool(const std::string& name, ToolFunc func);
    nlohmann::json invoke(const std::string& name, const nlohmann::json& args) const;
    bool hasTool(const std::string& name) const;

private:
    std::unordered_map<std::string, ToolFunc> tools_;

    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
    static nlohmann::json getWeather(const nlohmann::json& args);
    static nlohmann::json getTime(const nlohmann::json& args);
};
