#include "../../include/servlet/register.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("gomoku");

Register::Register(GomokuServer::ptr server)
    : Servlet("register")
    , m_server(server)
{}

int32_t Register::handle(const sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session)
{
    SYLAR_LOG_INFO(g_logger) << "this is register !!!";

    // 解析body(json格式)
    nlohmann::json parsed = nlohmann::json::parse(req->getBody());
    std::string username = parsed["username"];
    std::string password = parsed["password"];
    SYLAR_LOG_INFO(g_logger) << "username= " << username << " ,password= " << password;
    // 判断用户是否已经存在，如果存在则注册失败
    int userId = insertUser(username, password);
    if (userId != -1)
    {
        // 插入成功
        // 封装成功响应
        nlohmann::json successResp;   
        successResp["status"] = "success";
        successResp["message"] = "Register successful";
        successResp["userId"] = userId;
        std::string successBody = successResp.dump(4);

        rsp->setStatus(sylar::http::HttpStatus::OK);
        rsp->setClose(false);
        rsp->setHeader("content-type", "application/json");
        rsp->setBody(successBody);
    }
    else
    {
        // 插入失败
        nlohmann::json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = "username already exists";
        std::string failureBody = failureResp.dump(4);

        rsp->setStatus(sylar::http::HttpStatus::CONFLICT);
        rsp->setClose(false);
        rsp->setHeader("content-type", "application/json");
        rsp->setBody(failureBody);
    }

    return 0;
}

int Register::insertUser(const std::string &username, const std::string &password)
{
    // 判断用户是否存在，如果存在则返回-1，否则返回用户id
    if (!isUserExist(username))
    {

        std::string sql = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + password + "')";
        m_mysql.executeUpdate(sql);
        std::string sql2 = "SELECT id FROM users WHERE username = '" + username + "'";
        sql::ResultSet* res = m_mysql.executeQuery(sql2);
        if (res->next())
        {
            return res->getInt("id");
        }
    }
    return -1;
}

bool Register::isUserExist(const std::string &username)
{
    std::string sql = "SELECT id FROM users WHERE username = '" + username + "'";
    sql::ResultSet* res = m_mysql.executeQuery(sql);
    if (res->next())
    {
        SYLAR_LOG_INFO(g_logger) << "user is exists" ;
        return true;
    }
    SYLAR_LOG_INFO(g_logger) << "user is not exists" ;
    return false;
}