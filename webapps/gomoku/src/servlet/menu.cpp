#include "../../include/servlet/menu.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("gomoku");

Menu::Menu(GomokuServer::ptr server)
    : Servlet("Menu")
    , m_server(server)
{}

int32_t Menu::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session)
{
    // JSON 解析使用 try catch 捕获异常
    try
    {
        // 检查用户是否已登录
        auto session = m_server->m_httpserver->getSessionManager()->getSession(req, rsp);
        SYLAR_LOG_INFO(g_logger) << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
        if (session->getValue("isLoggedIn") != "true")
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

        // 获取用户信息
        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        std::string reqFile("../../webapps/gomoku/resource/menu.html");
        sylar::FileUtil fileOperater(reqFile);
        if (!fileOperater.isValid())
        {
            SYLAR_LOG_WARN(g_logger) << reqFile << "not exist.";
            fileOperater.resetDefaultFile();
        }

        std::vector<char> buffer(fileOperater.size());
        fileOperater.readFile(buffer); // 读出文件数据
        std::string htmlContent(buffer.data(), buffer.size());

        // 在HTML内容中插入userId
        size_t headEnd = htmlContent.find("</head>");
        if (headEnd != std::string::npos)
        {
            std::string script = "<script>const userId = '" + std::to_string(userId) + "';</script>";
            htmlContent.insert(headEnd, script);
        }

        rsp->setStatus(sylar::http::HttpStatus::OK);
        rsp->setClose(false);
        rsp->setHeader("content-type","text/html");
        rsp->setBody(htmlContent);
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
        rsp->setHeader("content-type","text/html");
        rsp->setBody(failureBody);
    }

    return 0;
}