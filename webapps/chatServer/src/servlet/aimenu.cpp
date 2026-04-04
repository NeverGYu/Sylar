#include "../../include/servlet/aimenu.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("chat");

AiMenu::AiMenu(ChatServer::ptr server)
    : Servlet("aimenu move")
    , m_server(server)
{}

int32_t AiMenu::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session) {
    try {

        auto Session = req->getSession();
        SYLAR_LOG_INFO(g_logger)<< "session->getValue(\"isLoggedIn\") = " << Session->getValue("isLoggedIn");
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

        std::string reqFile = chatserver::resourcePath("menu.html");
        sylar::FileUtil fileOperater(reqFile);
        if (!fileOperater.isValid())
        {
            throw std::runtime_error("Missing resource file: " + reqFile);
        }

        std::vector<char> buffer(fileOperater.size());
        fileOperater.readFile(buffer); // ļ
        std::string htmlContent(buffer.data(), buffer.size());


        size_t headEnd = htmlContent.find("</head>");
        if (headEnd != std::string::npos)
        {
            std::string script = "<script>const userId = '" + std::to_string(userId) + "';</script>";
            htmlContent.insert(headEnd, script);
        }

        rsp->setStatus(sylar::http::HttpStatus::OK);
        rsp->setClose(false);
        rsp->setHeader("content-type", "text/html");
        rsp->setHeader("content-length", std::to_string(htmlContent.size()));
        rsp->setBody(htmlContent);
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
