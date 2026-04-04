#include "../../include/base/aitoolregister.h"
#include <sstream>


AIToolRegistry::AIToolRegistry() {
    registerTool("get_weather", getWeather);
    registerTool("get_time", getTime);
}


void AIToolRegistry::registerTool(const std::string& name, ToolFunc func) {
    tools_[name] = func;
}


nlohmann::json AIToolRegistry::invoke(const std::string& name, const nlohmann::json& args) const {
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        throw std::runtime_error("Tool not found: " + name);
    }
    return it->second(args);
}


bool AIToolRegistry::hasTool(const std::string& name) const {
    return tools_.count(name) > 0;
}


size_t AIToolRegistry::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}


nlohmann::json AIToolRegistry::getWeather(const nlohmann::json& args) {
#ifndef SYLAR_ENABLE_AI_CURL
    (void)args;
    return nlohmann::json{ {"error", "weather tool is unavailable because chat_server was built without curl support"} };
#else
    if (!args.contains("city")) {
        return nlohmann::json{ {"error", "Missing parameter: city"} };
    }

    std::string city = args["city"].get<std::string>();
    std::string encodedCity;

    
    char* encoded = curl_easy_escape(nullptr, city.c_str(), city.length());
    if (encoded) {
        encodedCity = encoded;
        curl_free(encoded);
    }
    else {
        return nlohmann::json{ {"error", "URL encode failed"} };
    }

    std::string url = "https://wttr.in/" + encodedCity + "?format=3&lang=zh";

    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) {
        return nlohmann::json{ {"error", "Failed to init CURL"} };
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return nlohmann::json{ {"error", "CURL request failed"} };
    }

    
    return nlohmann::json{ {"city", city}, {"weather", response} };
#endif
}


nlohmann::json AIToolRegistry::getTime(const nlohmann::json& args) {
    (void)args;
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now);
    return nlohmann::json{ {"time", buffer} };
}
