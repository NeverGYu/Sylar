#include "../../include/servlet/aigame_start.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("gomoku");

AiGameStart::AiGameStart(GomokuServer::ptr server)
    : Servlet("AiGameStart")
    , m_server(server)
{}

int32_t AiGameStart::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session)
{
    auto sessionptr = m_server->m_httpserver->getSessionManager()->getSession(req, rsp);
    if (sessionptr->getValue("isLoggedIn") != "true")
    {
        // 用户未登录，返回未授权错误
        nlohmann::json errorResp;
        errorResp["status"] = "error";
        errorResp["message"] = "Unauthorized";
        std::string errorBody = errorResp.dump(4);

        m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::UNAUTHORIZED,
                             "Unauthorized", true, "application/json", errorBody.size(),
                             errorBody, rsp);
        return -1;
    }

    int userId = std::stoi(sessionptr->getValue("userId"));

    // 看来需要menu页面post发送userId
    {
        RWMutexType::WriteLock lock(m_server->m_mutex_ai_game);
        if (m_server->m_aiGames.find(userId) != m_server->m_aiGames.end())
        m_server->m_aiGames.erase(userId);
        m_server->m_aiGames[userId] = std::make_shared<AiGame>(userId);
    }

    // 创建一个ai机器人，它就while不断地执行下棋逻辑
    std::string reqFile("../../webapps/gomoku/resource/chessgame_vs_ai.html");
    sylar::FileUtil fileOperater(reqFile);
    if (!fileOperater.isValid())
    {
        SYLAR_LOG_WARN(g_logger) << reqFile << "not exist.";
        fileOperater.resetDefaultFile(); // FIXME:其实这里可能不必要，后续删了吧，不过其实也不会调用到毕竟详细地址是我服务端定义的
    }

    std::vector<char> buffer(fileOperater.size());
    fileOperater.readFile(buffer);
    std::string htmlContent(buffer.data(), buffer.size());

    rsp->setStatus(sylar::http::HttpStatus::OK);
    rsp->setClose(false);
    rsp->setHeader("content-type", "text/html");
    rsp->setBody(htmlContent);

    return 0;
}