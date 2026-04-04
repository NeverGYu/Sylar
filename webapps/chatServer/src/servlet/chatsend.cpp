#include "../../include/servlet/chatsend.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("chat");

ChatSend::ChatSend(ChatServer::ptr server)
    : Servlet("chat send")
    , m_server(server)
{}

int32_t ChatSend::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
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

        std::string userQuestion;
        std::string modelType;
        std::string sessionId;

        auto body = req->getBody();
        if (!body.empty())
        {
            auto j = nlohmann::json::parse(body);
            if (j.contains("question")) userQuestion = j["question"];
            if (j.contains("sessionId")) sessionId = j["sessionId"];

            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
        }

        std::shared_ptr<AIHelper> AIHelperPtr;
        {
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
        }

        std::string aiInformation = AIHelperPtr->chat(userId, username, sessionId, userQuestion, modelType);
        nlohmann::json successResp;
        successResp["success"] = true;
        successResp["Information"] = aiInformation;
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
