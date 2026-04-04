#include "../../include/servlet/chatspeech.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("chat");

ChatSpeech::ChatSpeech(ChatServer::ptr server)
    : Servlet("chat speech")
    , m_server(server)
{}

int32_t ChatSpeech::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
    sylar::http::HttpSession::ptr session)
{
    try
    {
        auto Session = req->getSession();
        SYLAR_LOG_INFO(g_logger) << "session->getValue(\"isLoggedIn\") = " << Session->getValue("isLoggedIn");
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

        std::string text;
        auto body = req->getBody();
        if (!body.empty())
        {
            auto j = nlohmann::json::parse(body);
            if (j.contains("text")) text = j["text"];
        }

        const char* secretEnv = std::getenv("BAIDU_CLIENT_SECRET");
        const char* idEnv = std::getenv("BAIDU_CLIENT_ID");

        if (!secretEnv) throw std::runtime_error("BAIDU_CLIENT_SECRET not found!");
        if (!idEnv) throw std::runtime_error("BAIDU_CLIENT_ID not found!");

        std::string clientSecret(secretEnv);
        std::string clientId(idEnv);

        AISpeechProcessor speechProcessor(clientId, clientSecret);

        std::string speechUrl = speechProcessor.synthesize(text,
                                                           "mp3-16k",
                                                           "zh",
                                                           5,
                                                           5,
                                                           5);

        nlohmann::json successResp;
        successResp["success"] = true;
        successResp["url"] = speechUrl;
        std::string successBody = successResp.dump(4);

        m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::OK,
            "OK", false, "application/json", successBody.size(), successBody, rsp);
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
    }

    return 0;
}
