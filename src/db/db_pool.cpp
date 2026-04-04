#include "../db/db_pool.h"
#include "../base/log.h"
#include <stdexcept>

namespace sylar{
namespace db{


DbConnectionPool::DbConnectionPool()
    : m_checkthread(std::bind(&DbConnectionPool::checkConnections, this), "check_thread")
{
    m_checkthread.detach();
}

DbConnectionPool::~DbConnectionPool()
{
    RWMutexType::WriteLock lock(m_mutex);
    while(!m_dbconnections.empty())
    {
        m_dbconnections.pop();
    }
    SYLAR_LOG_INFO(g_logger) << "Database connection pool destroyed";
}

void DbConnectionPool::init(const std::string& host,
        const std::string& user,
        const std::string& password,
        const std::string& database,
        size_t poolSize) 
{
    // 连接池会被多个线程访问，所以操作其成员变量时需要加锁
    RWMutexType::WriteLock lock(m_mutex);
    // 确保只初始化一次
    if (m_initialized) 
    {
        return;
    }
    m_host = host;
    m_user = user;
    m_password = password;
    m_database = database;
    // 创建连接
    for (size_t i = 0; i < poolSize; ++i)
    {
        m_dbconnections.push(createConnection());
    }
    m_sem = sylar::Semaphore(poolSize);
    m_initialized = true;
    SYLAR_LOG_INFO(g_logger) << "Database connection pool initialized with " << poolSize << " connections";
}

DbConnection::ptr DbConnectionPool::getConnection()
{
    // 信号量值减一，如果信号量的值等于0，说明已经没有资源，故堵塞
    m_sem.wait();
    // 这表示连接池还有资源
    DbConnection::ptr conn;
    {
        RWMutexType::WriteLock lock(m_mutex);
        if (!m_initialized)
        {
            throw std::runtime_error("Connection pool not initialized") ;
        }
        conn = m_dbconnections.front();
        m_dbconnections.pop();
    }

    try
    {
        // 检查连接是否存在
        if (!conn->ping())
        {
            SYLAR_LOG_WARN(g_logger) << "Connection lose ,attempting to reconnect ...";
            conn->reconnect();
        }
        return std::shared_ptr<DbConnection>(conn.get(), [this, conn](DbConnection* ){
            {
                RWMutexType::WriteLock lock(m_mutex);
                m_dbconnections.push(conn);
            }
            // 唤醒等待线程
            m_sem.notify();
        });
    }
    catch (const std::exception& e) 
    {
        SYLAR_LOG_ERROR(g_logger) << "Failed to get connection: " << e.what();
        {
            RWMutexType::WriteLock lock(m_mutex);
            m_dbconnections.push(conn); 
        }
        m_sem.notify();
        throw;
    }
    
}

DbConnection::ptr DbConnectionPool::createConnection()
{
    return std::make_shared<DbConnection>(m_host,m_user,m_password,m_database);
}

void DbConnectionPool::checkConnections()
{
    while(true)
    {
        try
        {
            std::vector<DbConnection::ptr> connsTocheck;
            {
                RWMutexType::ReadLock lock(m_mutex);
                if (m_dbconnections.empty())
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                auto temp = m_dbconnections;
                while(!temp.empty())
                {
                    connsTocheck.push_back(temp.front());
                    temp.pop();    
                }
            }

             // 在锁外检查连接
             for (auto& conn : connsTocheck) 
             {
                 if (!conn->ping()) 
                 {
                     try 
                     {
                         conn->reconnect();
                     } 
                     catch (const std::exception& e) 
                     {
                        SYLAR_LOG_ERROR(g_logger) << "Failed to reconnect: " << e.what();
                     }
                 }
             }             
             std::this_thread::sleep_for(std::chrono::seconds(60));
        }
        catch(const std::exception& e)
        {
            SYLAR_LOG_ERROR(g_logger) << "Error in check thread: " << e.what();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

}
}