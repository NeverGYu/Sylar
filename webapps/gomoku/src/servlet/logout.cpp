#include "../../include/servlet/logout.h"

Logout::Logout(GomokuServer::ptr server)
    : Servlet("logout")
    , m_server(server)
{}

int32_t Logout::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session)
{
    auto contentType = req->getHeader("Content-Type");
    if (contentType.empty() || contentType != "application/json" || req->getBody().empty())
    {
        rsp->setStatus(sylar::http::HttpStatus::BAD_REQUEST);
        rsp->setClose(true);
        rsp->setHeader("content-type", "application/json");
        rsp->setBody("");
        return -1;
    }

    // JSON 解析使用 try catch 捕获异常
    try
    {
        // 获取会话
        auto session = m_server->m_httpserver->getSessionManager()->getSession(req, rsp);
        // 获取用户id
        int userId = std::stoi(session->getValue("userId"));
        // 清除会话数据
        session->clear();
        // 销毁会话
        m_server->m_httpserver->getSessionManager()->destroySession(session->getSessionId());
        
        nlohmann::json parsed = nlohmann::json::parse(req->getBody());
        int gameType = parsed["gameType"]; // fixme: 以后也换成从会话中获取
        
        {   // 释放资源
            RWMutexType::WriteLock lock(m_server->m_mutex_for_online_users);
            m_server->m_online_users.erase(userId);
        }

        if (gameType == GomokuServer::MAN_VS_AI)
        {
            RWMutexType::WriteLock lock(m_server->m_mutex_ai_game);
            m_server->m_aiGames.erase(userId);
        }
        else if (gameType == GomokuServer::MAN_VS_MAN)
        {
            // 释放相应创造资源，并且通知另一个用户对方已经主动退出游戏
        }

        // 返回响应报文
        nlohmann::json response;
        response["message"] = "logout successful";
        std::string responseBody = response.dump(4);
        rsp->setStatus(sylar::http::HttpStatus::OK);
        rsp->setClose(true);
        rsp->setHeader("content-type", "application/json");
        rsp->setBody(responseBody);
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
    }

    return 0;
}