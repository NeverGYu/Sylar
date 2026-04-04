#include "../../include/servlet/chathistory.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("chat");

ChatHistory::ChatHistory(ChatServer::ptr server)
    : Servlet("chat history")
    , m_server(server)
{}

int32_t ChatHistory::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
    sylar::http::HttpSession::ptr session)
{
    try
    {
        auto Session = req->getSession();
        SYLAR_LOG_INFO(g_logger) << "session->getValue(\"isLoggedIn\") = " << Session->getValue("isLoggedIn");
        if (Session->getValue("isLoggedIn") != "true")
        {
            nlohmann::json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);

            m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::UNAUTHORIZED,
                "Unauthorized", true, "application/json", errorBody.size(), errorBody, rsp);
            return 0;
        }

        int userId = std::stoi(Session->getValue("userId"));
        std::string username = Session->getValue("username");

        std::string sessionId;
        auto body = req->getBody();
        if (!body.empty())
        {
            auto j = nlohmann::json::parse(body);
            if (j.contains("sessionId")) sessionId = j["sessionId"];
        }

        std::vector<std::pair<std::string, long long>> messages;
        {
            std::shared_ptr<AIHelper> AIHelperPtr;
            RWMutexType::WriteLock lock(m_server->mutexForChatInformation);

            auto& userSessions = m_server->chatInformation[userId];
            if (userSessions.find(sessionId) == userSessions.end())
            {
                userSessions.emplace(
                    sessionId,
                    std::make_shared<AIHelper>()
                );
            }
            AIHelperPtr = userSessions[sessionId];
            messages = AIHelperPtr->GetMessages();
        }

        nlohmann::json successResp;
        successResp["success"] = true;
        successResp["history"] = nlohmann::json::array();

        for (size_t i = 0; i < messages.size(); ++i)
        {
            nlohmann::json msgJson;
            msgJson["is_user"] = (i % 2 == 0);
            msgJson["content"] = messages[i].first;
            successResp["history"].push_back(msgJson);
        }

        std::string successBody = successResp.dump(4);

        m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::OK,
            "OK", false, "application/json", successBody.size(), successBody, rsp);
        return 0;
    }
    catch (const std::exception& e)
    {
        nlohmann::json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);

        m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::BAD_REQUEST,
            "Bad Request", true, "application/json", failureBody.size(), failureBody, rsp);
    }

    return 0;
}
