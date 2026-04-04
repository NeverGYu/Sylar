#pragma once

#include "../../include/base/sylar.h"
#include "../../include/base/mutex.h"
#include "../base/chatServer.h"
#include <string>
#include <nlohmann/json.hpp>

class ChatRegister : public sylar::http::Servlet {
public:
    using RWMutexType = sylar::RWMutex;

    ChatRegister(ChatServer::ptr server);

    int32_t handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp,
        sylar::http::HttpSession::ptr session) override;
private:
    int insertUser(const std::string& username, const std::string& password);
    bool isUserExist(const std::string& username);

    ChatServer::ptr m_server;
};
