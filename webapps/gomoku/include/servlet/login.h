#pragma once

#include "../../include/base/sylar.h"
#include "../../include/base/mutex.h"
#include "../base/gomoku_server.h"
#include <string>
#include <nlohmann/json.hpp>

class Login : public sylar::http::Servlet
{
public:
    using RWMutexType = sylar::RWMutex;
    /**
     *  @brief 构造函数 
     */
    Login(GomokuServer::ptr server);

    /**
     *  @brief 每条连接的处理函数 
     */
    int32_t handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session) override;

private:
    /**
     *  @brief 查询用户id是否存在  
     */
    int queryUserId(const std::string& username, const std::string& password);

private:
    GomokuServer::ptr m_server;
    sylar::db::MysqlUtil m_mysql;
};
