#include "../../include/servlet/login.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("gomoku");

Login::Login(GomokuServer::ptr server)
    : Servlet("login")
    , m_server(server)
{}

int Login::queryUserId(const std::string& username, const std::string& password)
{
    SYLAR_LOG_INFO(g_logger) << "queryUserId is processing !";
    // 前端用户传来账号密码，查找数据库是否有该账号密码
    // 使用预处理语句, 防止sql注入
    std::string sql = "SELECT id FROM users WHERE username = ? AND password = ?";
    sql::ResultSet* res = m_mysql.executeQuery(sql, username, password);
    SYLAR_LOG_DEBUG(g_logger) << "executeQuery";
    if (res->next())
    {
        int id = res->getInt("id");
        return id;
    }
    // 如果查询结果为空，则返回-1
    return -1;
}

int32_t Login::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session)
{
    SYLAR_LOG_INFO(g_logger) << "this is login !!!!";
    // 处理登录逻辑
    auto contentType = req->getHeader("Content-Type");
    // 判断是否为空
    if (contentType.empty() || contentType != "application/json" || req->getBody().empty())
    {
        SYLAR_LOG_INFO(g_logger) << "content: " << req->getBody();
        rsp->setStatus( sylar::http::HttpStatus::BAD_REQUEST);
        rsp->setClose(true);
        rsp->setHeader("content-type", "application/json");
        rsp->setBody("content is null, please try again");
        return -1;
    }
    // 使用json解析
    try
    {
        nlohmann::json parsed = nlohmann::json::parse(req->getBody());
        std::string username = parsed.value("username", "");
        std::string password = parsed.value("password","");

        SYLAR_LOG_DEBUG(g_logger) << "username: " << username << " ,password: " << password; 
        // 验证用户是否存在
        int userId = queryUserId(username, password);

        SYLAR_LOG_DEBUG(g_logger) << "userId = " << userId << ", online_users count: " << m_server->m_online_users.size();

        if (userId != -1)
        {
            SYLAR_LOG_DEBUG(g_logger) << "before getSession";
            // 获取会话
            auto userSession = m_server->m_httpserver->getSessionManager()->getSession(req, rsp);

            // 增加更详细的指针检查日志
            SYLAR_LOG_DEBUG(g_logger) << "userSession pointer: " << userSession.get(); 

            if (!userSession) {
                SYLAR_LOG_ERROR(g_logger) << "Critical Error: SessionManager returned null session!";
                return -1;
            }
            
            // 在会话中存储用户信息
            userSession->setValue("userId", std::to_string(userId));
            userSession->setValue("username", username);
            userSession->setValue("isLoggedIn", "true");

            SYLAR_LOG_DEBUG(g_logger) << "after setValue";
            
            if (m_server->m_online_users.find(userId) == m_server->m_online_users.end() || m_server->m_online_users[userId] == false)
            {
                SYLAR_LOG_DEBUG(g_logger) << "userId not found";
                {
                    RWMutexType::WriteLock lock(m_server->m_mutex_for_online_users);
                    m_server->m_online_users[userId] = true;
                }

                // 更新历史最高在线人数
                m_server->updateMaxOnline(m_server->m_online_users.size());
                // 用户存在登录成功
                // 封装json 数据。
                nlohmann::json successResp;
                successResp["success"] = true;
                successResp["userId"] = userId;

                SYLAR_LOG_INFO(g_logger) << successResp.dump();

                std::string successBody = successResp.dump(4);

                rsp->setStatus(sylar::http::HttpStatus::OK);
                rsp->setClose(false);
                rsp->setHeader("content-type", "application/json");
                rsp->setBody(successBody);
                SYLAR_LOG_INFO(g_logger) << "Login::handle end"; 
                return 0; 
            }
            else
            {
                // FIXME: 当前该用户正在其他地方登录中，将原有登录用户强制下线更好
                // 不允许重复登录，
                nlohmann::json failureResp;
                failureResp["success"] = false;
                failureResp["error"] = "账号已在其他地方登录";
                std::string failureBody = failureResp.dump(4);

                rsp->setStatus(sylar::http::HttpStatus::FORBIDDEN);
                rsp->setClose(true);
                rsp->setHeader("content-type", "application/json");
                rsp->setBody(failureBody);
                return -1;
            }
        }
        else // 账号密码错误，请重新登录
        {
            SYLAR_LOG_INFO(g_logger) << "Login failed: invalid username or password. username=" << username; 
            // 封装json数据
            nlohmann::json failureResp;
            failureResp["status"] = "error";
            failureResp["message"] = "Invalid username or password";
            std::string failureBody = failureResp.dump(4);

            rsp->setStatus(sylar::http::HttpStatus::UNAUTHORIZED);
            rsp->setClose(false);
            rsp->setHeader("content-type", "application/json");
            rsp->setBody(failureBody);
            return -1;
        }
    }
    catch (const std::exception &e)
    {
        // 捕获异常，返回错误信息
        nlohmann::json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);

        rsp->setStatus(sylar::http::HttpStatus::BAD_REQUEST);
        rsp->setClose(true);
        rsp->setHeader("content-type", "application/json");
        rsp->setBody(failureBody);
        return -1;
    }

    return 0;
}
        
