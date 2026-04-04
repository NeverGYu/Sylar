#pragma once

#include <queue>
#include <memory>
#include "../db/db_connection.h"
#include "../base/singleton.h"
#include "../base/mutex.h"
#include "../base/thread.h"

namespace sylar{
namespace db{


class DbConnectionPool : public Noncopyable
{
public:
    friend class SingleTon<DbConnectionPool>;
    using ptr = std::shared_ptr<DbConnectionPool>;
    using RWMutexType = RWMutex;

    /**
     * @brief 初始化连接池
     */ 
    void init(const std::string& host, const std::string& user, const std::string& password, const std::string& database, size_t poolSize = 10);

    /**
     * @brief 获取连接
     */ 
    DbConnection::ptr getConnection();

private:
    /**
     * @brief 构造函数
     */ 
    DbConnectionPool();

    /**
     *  @brief 析构函数
     */ 
    ~DbConnectionPool();

    /**
     *  @brief 创建一个数据库连接
     */
    std::shared_ptr<DbConnection> createConnection();

    /**
     *  @brief 添加连接检查方法
     */
    void checkConnections(); 

private:
    std::string m_host;
    std::string m_user;
    std::string m_password;
    std::string m_database;
    RWMutexType m_mutex;
    Semaphore mysql_stmt_error;
    bool m_initialized = false;
    Thread m_checkthread;
    std::queue<DbConnection::ptr> m_dbconnections;
    Semaphore m_sem;
};

using db_pool = sylar::SingleTon<DbConnectionPool>;

}
}