#include "../../include/servlet/chatregister.h"

ChatRegister::ChatRegister(ChatServer::ptr server)
    : Servlet("chat register")
    , m_server(server)
{}

int32_t ChatRegister::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
    sylar::http::HttpSession::ptr session)
{
    try
    {
        m_server->requireDatabase();
        nlohmann::json parsed = nlohmann::json::parse(req->getBody());
        std::string username = parsed["username"];
        std::string password = parsed["password"];

        int userId = insertUser(username, password);
        if (userId != -1)
        {
            nlohmann::json successResp;
            successResp["status"] = "success";
            successResp["message"] = "Register successful";
            successResp["userId"] = userId;
            std::string successBody = successResp.dump(4);

            m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::OK,
                "OK", false, "application/json", successBody.size(), successBody, rsp);
        }
        else
        {
            nlohmann::json failureResp;
            failureResp["status"] = "error";
            failureResp["message"] = "username already exists";
            std::string failureBody = failureResp.dump(4);

            m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::CONFLICT,
                "Conflict", false, "application/json", failureBody.size(), failureBody, rsp);
        }
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

int ChatRegister::insertUser(const std::string& username, const std::string& password)
{
    m_server->requireDatabase();
    if (!isUserExist(username))
    {
        std::string sql = "INSERT INTO users (username, password) VALUES (?, ?)";
        m_server->m_mysql.executeUpdate(sql, username, password);
        std::string sql2 = "SELECT id FROM users WHERE username = ?";
        auto res = m_server->m_mysql.executeQuery(sql2, username);
        if (res->next())
        {
            return res->getInt("id");
        }
    }
    return -1;
}

bool ChatRegister::isUserExist(const std::string& username)
{
    m_server->requireDatabase();
    std::string sql = "SELECT id FROM users WHERE username = ?";
    auto res = m_server->m_mysql.executeQuery(sql, username);
    if (res->next())
    {
        return true;
    }
    return false;
}
