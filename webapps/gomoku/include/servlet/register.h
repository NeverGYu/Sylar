#pragma once

#include "../../include/base/sylar.h"
#include "../../include/base/mutex.h"
#include "../base/gomoku_server.h"
#include <string>
#include <nlohmann/json.hpp>

class Register : public sylar::http::Servlet
{
public:
    /**
     *  @brief 构造函数
     *  @param[in] server 五子棋服务 
     */
    Register(GomokuServer::ptr server);

    /**
     *  @brief  处理函数
     */
    int32_t handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session) override;

private:
    /**
     *  @brief 添加用户
     *  @param[in] username 用户名
     *  @param[in] password 密码 
     */
    int insertUser(const std::string& username, const std::string& password);

    /**
     *  @brief 判断用户是否存在
     *  @param[in] username 用户名 
     */
    bool isUserExist(const std::string& username);

private:
    GomokuServer::ptr m_server;
    sylar::db::MysqlUtil m_mysql;
};