#include "../../include/servlet/chatentry.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("chat");

ChatEntry::ChatEntry(ChatServer::ptr server)
    : Servlet("chat entry")
    , m_server(server)
{}

int32_t ChatEntry::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
    sylar::http::HttpSession::ptr session)
{
    try
    {
        std::string reqFile = chatserver::resourcePath("entry.html");
        sylar::FileUtil fileOperater(reqFile);
        if (!fileOperater.isValid())
        {
            throw std::runtime_error("Missing resource file: " + reqFile);
        }

        std::vector<char> buffer(fileOperater.size());
        fileOperater.readFile(buffer);
        std::string bufStr(buffer.data(), buffer.size());

        rsp->setStatus(sylar::http::HttpStatus::OK);
        rsp->setClose(false);
        rsp->setHeader("content-type", "text/html");
        rsp->setHeader("content-length", std::to_string(bufStr.size()));
        rsp->setBody(bufStr);
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
