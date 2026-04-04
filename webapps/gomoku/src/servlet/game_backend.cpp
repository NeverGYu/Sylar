#include "../../include/servlet/game_backend.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("gomoku");

GameBackend::GameBackend(GomokuServer::ptr server)
    : Servlet("GameBackend")
    , m_server(server)
{}

int32_t GameBackend::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session)
{
    // 后台界面
    // 获取当前在线人数、历史最高在线人数、数据库中已注册用户总数
    std::string reqFile("../../webapps/gomoku/resource/backend.html");
    sylar::FileUtil fileOperater(reqFile);
    if (!fileOperater.isValid())
    {
        SYLAR_LOG_WARN(g_logger) << reqFile << "not exist.";
        fileOperater.resetDefaultFile();
    }

    std::vector<char> buffer(fileOperater.size());
    fileOperater.readFile(buffer); // 读出文件数据
    std::string htmlContent(buffer.data(), buffer.size());

    rsp->setStatus(sylar::http::HttpStatus::OK);
    rsp->setClose(false);
    rsp->setHeader("content-type", "text/html");
    rsp->setBody(htmlContent);

    return 0;
}