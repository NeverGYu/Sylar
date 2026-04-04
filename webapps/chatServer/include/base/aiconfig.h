#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>  

struct AITool {
    std::string name;
    std::unordered_map<std::string, std::string> params;
    std::string desc;
};


struct AIToolCall {
    std::string toolName;
    nlohmann::json args;
    bool isToolCall = false;
};


class AIConfig {
public:
    bool loadFromFile(const std::string& path);
    std::string buildPrompt(const std::string& userInput) const;
    AIToolCall parseAIResponse(const std::string& response) const;
    std::string buildToolResultPrompt(const std::string& userInput,const std::string& toolName
        ,const nlohmann::json& toolArgs,const nlohmann::json& toolResult) const;

private:
    std::string promptTemplate_;
    std::vector<AITool> tools_;

    std::string buildToolList() const;
};