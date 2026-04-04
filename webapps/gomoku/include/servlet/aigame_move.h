#pragma once

#include "../../include/base/sylar.h"
#include "../../include/base/mutex.h"
#include "../base/gomoku_server.h"
#include <string>
#include <nlohmann/json.hpp>

class AiGameMove : public sylar::http::Servlet
{
public:
    using RWMutexType = sylar::RWMutex;

    AiGameMove(GomokuServer::ptr server);

    int32_t handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session) override;
private:
    GomokuServer::ptr m_server;
};

