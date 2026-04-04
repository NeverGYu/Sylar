#include "../../include/servlet/chat_create_and_send.h"
#include "../../include/base/ai_session_id_generator.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("chat");

ChatCreateAndSend::ChatCreateAndSend(ChatServer::ptr server)
    : Servlet("chat create and send")
    , m_server(server)
{}

int32_t ChatCreateAndSend::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
    sylar::http::HttpSession::ptr session)
{
    try
    {
        auto Session = req->getSession();
        SYLAR_LOG_INFO(g_logger) << "session->getValue(\"isLoggedIn\") = " << Session->getValue("isLoggedIn");

        const std::string cookieSid = req->getCookie("sessionId");
        SYLAR_LOG_INFO(g_logger) << "[send-new] cookieSid=" << cookieSid
                         << ", session_ptr=" << (Session ? "ok" : "null")
                         << ", sid=" << (Session ? Session->getSessionId() : "null")
                         << ", isLoggedIn=" << (Session ? Session->getValue("isLoggedIn") : "null");


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

        auto body = req->getBody();
        if (!body.empty())
        {
            auto j = nlohmann::json::parse(body);
            if (j.contains("question")) userQuestion = j["question"];

            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
        }

        AISessionIdGenerator generator;
        std::string sessionId = generator.generate();
        std::cout << "ɵsessionIdΪ " << sessionId << std::endl;

        std::shared_ptr<AIHelper> AIHelperPtr;
        bool isNewSession = false;
        {
            RWMutexType::WriteLock lock(m_server->mutexForChatInformation);

            auto& userSessions = m_server->chatInformation[userId];
            if (userSessions.find(sessionId) == userSessions.end())
            {
                userSessions.emplace(
                    sessionId,
                    std::make_shared<AIHelper>()
                );
                isNewSession = true;
            }
            AIHelperPtr = userSessions[sessionId];
        }

        if (isNewSession)
        {
            RWMutexType::WriteLock lock(m_server->mutexForSessionsId);
            m_server->sessionsIdsMap[userId].push_back(sessionId);
        }

        std::string aiInformation = AIHelperPtr->chat(userId, username, sessionId, userQuestion, modelType);
        nlohmann::json successResp;
        successResp["success"] = true;
        successResp["Information"] = aiInformation;
        successResp["sessionId"] = sessionId;

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
