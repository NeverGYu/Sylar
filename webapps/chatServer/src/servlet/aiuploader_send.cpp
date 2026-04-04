#include "../../include/servlet/aiuploader_send.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("chat");

AiUploadSend::AiUploadSend(ChatServer::ptr server)
    : Servlet("ai upload send")
    , m_server(server)
{}

int32_t AiUploadSend::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
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
        std::shared_ptr<ImageRecognizer> ImageRecognizerPtr;
        {
            RWMutexType::WriteLock lock(m_server->mutexForImageRecognizerMap);
            if (m_server->ImageRecognizerMap.find(userId) == m_server->ImageRecognizerMap.end())
            {
                const std::string modelPath = chatserver::imageModelPath();
                if (modelPath.empty()) {
                    throw std::runtime_error("CHAT_SERVER_IMAGE_MODEL is not set");
                }
                m_server->ImageRecognizerMap.emplace(
                    userId,
                    std::make_shared<ImageRecognizer>(modelPath, chatserver::imageLabelPath())
                );
            }
            ImageRecognizerPtr = m_server->ImageRecognizerMap[userId];
        }

        auto body = req->getBody();
        std::string filename;
        std::string imageBase64;
        if (!body.empty())
        {
            auto j = nlohmann::json::parse(body);
            if (j.contains("filename")) filename = j["filename"];
            if (j.contains("image")) imageBase64 = j["image"];
        }
        if (imageBase64.empty())
        {
            throw std::runtime_error("No image data provided");
        }

        std::string decodedData = base64_decode(imageBase64);
        std::vector<unsigned char> imgData(decodedData.begin(), decodedData.end());

        std::string className = ImageRecognizerPtr->PredictFromBuffer(imgData);

        nlohmann::json successResp;
        successResp["success"] = "ok";
        successResp["filename"] = filename;
        successResp["class_name"] = className;
        successResp["confidence"] = 0.95;

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
