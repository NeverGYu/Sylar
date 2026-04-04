#include "../../include/servlet/chatsession.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("chat");

ChatSessions::ChatSessions(ChatServer::ptr server)
    : Servlet("chat sessions")
    , m_server(server)
{}

int32_t ChatSessions::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
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

        std::vector<std::string> sessions;
        {
            RWMutexType::WriteLock lock(m_server->mutexForSessionsId);
            sessions = m_server->sessionsIdsMap[userId];
        }

        nlohmann::json successResp;
        successResp["success"] = true;

        nlohmann::json sessionArray = nlohmann::json::array();
        for (const auto& sid : sessions)
        {
            nlohmann::json s;
            s["sessionId"] = sid;
            s["name"] = "Ự " + sid;
            sessionArray.push_back(s);
        }
        successResp["sessions"] = sessionArray;

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
