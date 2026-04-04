#pragma once

#include <memory>
#include <string>
#include <mysql/mysql.h>
#include <mysql_driver.h>
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <stdexcept>
#include "../base/mutex.h"
#include "../base/noncopyable.h"
#include "../base/log.h"
#include "../base/singleton.h"



namespace sylar{
namespace db{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

class DbConnection : public Noncopyable
{
public:
    using ptr = std::shared_ptr<DbConnection>;
    using MutexType = Mutex;

    /**
     *  @brief 构造函数
     *  @param[in] host 域名
     *  @param[in] user 用户名
     *  @param[in] password 密码
     *  @param[in] database 数据库 
     */
    DbConnection(const std::string& host, const std::string& user, const std::string& password, const std::string& database);

    /**
     *  @brief 析构函数 
     */
    ~DbConnection();

    /**
     *  @brief 判断连接是否有效 
     */
    bool isValid();

    /**
     *  @brief 重新连接 
     */
    void reconnect();

    /**
     *  @brief 清理连接 
     */
    void cleanup();

    /**
     *  @brief 添加检测连接是否有效的方法 
     */
    bool ping();

    /**
     *  @brief 执行查询操作
     *  @param[in] sql 查询语句
     *  @param[in] args 可变参数 
     */
    template<typename... Args>
    sql::ResultSet* executeQuery(const std::string& sql, Args&&... args)
    {
        MutexType::Lock lock(m_mutex);
        try
        {
            // 直接创建新的预处理语句，不使用缓存
            std::unique_ptr<sql::PreparedStatement> stmt(m_dbconnection->prepareStatement(sql));
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);
            return stmt->executeQuery();
        }
        catch(const sql::SQLException& e)
        {
            SYLAR_LOG_ERROR(g_logger) << "Query failed: " << e.what() << ", SQL: " << sql;
            throw std::runtime_error(e.what());
        }
    }

    /**
     *  @brief 执行更新操作
     *  @param[in] sql 更新语句
     *  @param[in] args 可变参数 
     */
    template<typename... Args>
    int executeUpdate(const std::string& sql, Args&&... args)
    {
        MutexType::Lock lock(m_mutex);
        try
        {
            // 直接创建新的预处理语句，不使用缓存
            std::unique_ptr<sql::PreparedStatement> stmt(m_dbconnection->prepareStatement(sql));
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);
            return stmt->executeUpdate(); // returns the number of affected rows
        }
        catch(const sql::SQLException& e)
        {
            SYLAR_LOG_ERROR(g_logger) << "Update failed: " << e.what() << ", SQL: " << sql;
            throw std::runtime_error(e.what());
        }
    }
private:
    /**
     *  @brief 辅助函数，用于递归终止条件 
     */
    void bindParams(sql::PreparedStatement* , int) {}

    /**
     *  @brief 辅助函数：用于绑定参数
     *  @param[] 
     */
    template<typename T, typename...Args>
    void bindParams(sql::PreparedStatement* stmt, int index, T&& value, Args&&... args)
    {
        stmt->setString(index, std::to_string(std::forward<T>(value)));
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

    // 特化 string 类型的参数绑定
    template<typename... Args>
    void bindParams(sql::PreparedStatement* stmt, int index, const std::string& value, Args&&... args) 
    {
        stmt->setString(index, value);
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

private:
    std::string m_host;        // 域名
    std::string m_user;        // 用户名
    std::string m_password;    // 密码
    std::string m_database;    // 所属数据库
    MutexType m_mutex;         // 互斥量
    std::shared_ptr<sql::Connection> m_dbconnection;   // 数据库连接
};

}
}