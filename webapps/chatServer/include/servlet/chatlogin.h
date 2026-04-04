#pragma once

#include "../../include/base/sylar.h"
#include "../../include/base/mutex.h"
#include "../base/chatServer.h"
#include <string>
#include <nlohmann/json.hpp>

class ChatLogin : public sylar::http::Servlet {
public:
    using RWMutexType = sylar::RWMutex;

    ChatLogin(ChatServer::ptr server);

    int32_t handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
        sylar::http::HttpSession::ptr session) override;
private:
    int queryUserId(const std::string& username, const std::string& password);

    ChatServer::ptr m_server;
};
