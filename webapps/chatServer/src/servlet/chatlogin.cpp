#include "../../include/servlet/chatlogin.h"

ChatLogin::ChatLogin(ChatServer::ptr server)
    : Servlet("chat login")
    , m_server(server)
{}

int32_t ChatLogin::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
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
        m_server->requireDatabase();
        nlohmann::json parsed = nlohmann::json::parse(req->getBody());
        std::string username = parsed["username"];
        std::string password = parsed["password"];

        int userId = queryUserId(username, password);
        if (userId != -1)
        {
            auto Session = req->getSession();

            Session->setValue("userId", std::to_string(userId));
            Session->setValue("username", username);
            Session->setValue("isLoggedIn", "true");

            bool allowLogin = false;
            {
                RWMutexType::WriteLock lock(m_server->mutexForOnlineUsers_);
                auto it = m_server->m_onlineUsers.find(userId);
                if (it == m_server->m_onlineUsers.end() || !it->second)
                {
                    m_server->m_onlineUsers[userId] = true;
                    allowLogin = true;
                }
            }

            if (allowLogin)
            {
                nlohmann::json successResp;
                successResp["success"] = true;
                successResp["userId"] = userId;
                std::string successBody = successResp.dump(4);

                m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::OK,
                    "OK", false, "application/json", successBody.size(), successBody, rsp);
                return 0;
            }

            nlohmann::json failureResp;
            failureResp["success"] = false;
            failureResp["error"] = "˺ط¼";
            std::string failureBody = failureResp.dump(4);

            m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::FORBIDDEN,
                "Forbidden", true, "application/json", failureBody.size(), failureBody, rsp);
            return 0;
        }

        nlohmann::json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = "Invalid username or password";
        std::string failureBody = failureResp.dump(4);

        m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::UNAUTHORIZED,
            "Unauthorized", false, "application/json", failureBody.size(), failureBody, rsp);
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
        return 0;
    }
}

int ChatLogin::queryUserId(const std::string& username, const std::string& password)
{
    m_server->requireDatabase();
    std::string sql = "SELECT id FROM users WHERE username = ? AND password = ?";
    auto res = m_server->m_mysql.executeQuery(sql, username, password);
    if (res->next())
    {
        int id = res->getInt("id");
        return id;
    }

    return -1;
}
