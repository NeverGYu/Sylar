#include "../../include/servlet/chatlogout.h"

ChatLogout::ChatLogout(ChatServer::ptr server)
    : Servlet("chat logout")
    , m_server(server)
{}

int32_t ChatLogout::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
    sylar::http::HttpSession::ptr session)
{
    auto contentType = req->getHeader("Content-Type");
    if (contentType.empty() || contentType != "application/json" || req->getBody().empty())
    {
        m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::BAD_REQUEST,
            "Bad Request", true, "application/json", 0, "", rsp);
        return 0;
    }

    try
    {
        auto Session = req->getSession();

        int userId = std::stoi(Session->getValue("userId"));

        Session->clear();
        m_server->m_httpserver->getSessionManager()->destroySession(Session->getSessionId());

        {
            RWMutexType::WriteLock lock(m_server->mutexForOnlineUsers_);
            m_server->m_onlineUsers.erase(userId);
        }

        nlohmann::json response;
        response["message"] = "logout successful";
        std::string responseBody = response.dump(4);

        m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::OK,
            "OK", true, "application/json", responseBody.size(), responseBody, rsp);
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
