#include"../../include/base/aistrategy.h"
#include"../../include/base/aifactory.h"

std::string AliyunStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

std::string AliyunStrategy::getApiKey()const {
    return apiKey_;
}


std::string AliyunStrategy::getModel() const {
    return "qwen-plus";
}


nlohmann::json AliyunStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    nlohmann::json payload;
    payload["model"] = getModel();
    nlohmann::json msgArray = nlohmann::json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        nlohmann::json msg;
        if (i % 2 == 0) {
            msg["role"] = "user";
        }
        else {
            msg["role"] = "assistant";
        }
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}


std::string AliyunStrategy::parseResponse(const nlohmann::json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}


std::string DouBaoStrategy::getApiUrl()const {
    return "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
}

std::string DouBaoStrategy::getApiKey()const {
    return apiKey_;
}


std::string DouBaoStrategy::getModel() const {
    return "doubao-seed-1-6-thinking-250715";
}


nlohmann::json DouBaoStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    nlohmann::json payload;
    payload["model"] = getModel();
    nlohmann::json msgArray = nlohmann::json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        nlohmann::json msg;
        if (i % 2 == 0) {
            msg["role"] = "user";
        }
        else {
            msg["role"] = "assistant";
        }
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}


std::string DouBaoStrategy::parseResponse(const nlohmann::json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}


std::string AliyunRAGStrategy::getApiUrl() const {
    const char* key = std::getenv("Knowledge_Base_ID");
    if (!key) throw std::runtime_error("Knowledge_Base_ID not found!");
    std::string id(key);
    //϶Ӧ֪ʶID
    return "https://dashscope.aliyuncs.com/api/v1/apps/"+id+"/completion";
}

std::string AliyunRAGStrategy::getApiKey()const {
    return apiKey_;
}


std::string AliyunRAGStrategy::getModel() const {
    return ""; //Ҫģ
}


nlohmann::json AliyunRAGStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    nlohmann::json payload;
    nlohmann::json msgArray = nlohmann::json::array();
    for (size_t i = 0; i < messages.size(); ++i) {
        nlohmann::json msg;
        msg["role"] = (i % 2 == 0 ? "user" : "assistant");
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["input"]["messages"] = msgArray;
    payload["parameters"] = nlohmann::json::object(); 
    return payload;
}


std::string AliyunRAGStrategy::parseResponse(const nlohmann::json& response) const {
    if (response.contains("output") && response["output"].contains("text")) {
        return response["output"]["text"];
    }
    return {};
}



std::string AliyunMcpStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

std::string AliyunMcpStrategy::getApiKey()const {
    return apiKey_;
}


std::string AliyunMcpStrategy::getModel() const {
    return "qwen-plus";
}


nlohmann::json AliyunMcpStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    nlohmann::json payload;
    payload["model"] = getModel();
    nlohmann::json msgArray = nlohmann::json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        nlohmann::json msg;
        if (i % 2 == 0) {
            msg["role"] = "user";
        }
        else {
            msg["role"] = "assistant";
        }
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}


std::string AliyunMcpStrategy::parseResponse(const nlohmann::json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}


static StrategyRegister<AliyunStrategy> regAliyun("1");
static StrategyRegister<DouBaoStrategy> regDoubao("2");
static StrategyRegister<AliyunRAGStrategy> regAliyunRag("3");
static StrategyRegister<AliyunMcpStrategy> regAliyunMcp("4");