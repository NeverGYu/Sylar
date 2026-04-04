#include "../db/db_connection.h"
#include <stdexcept>

namespace sylar{
namespace db{

DbConnection::DbConnection(const std::string& host, const std::string& user, const std::string& password, const std::string& database)
    : m_host(host)
    , m_user(user)
    , m_password(password)
    , m_database(database)
{
    try
    {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_driver_instance();
        m_dbconnection.reset(driver->connect(m_host, m_user, m_password));
        if (m_dbconnection)
        {
            m_dbconnection->setSchema(m_database);
            
            // 设置连接属性
            m_dbconnection->setClientOption("OPT_RECONNECT", "true");
            m_dbconnection->setClientOption("OPT_CONNECT_TIMEOUT", "10");
            m_dbconnection->setClientOption("multi_statements", "false");
            
            // 设置字符集
            std::unique_ptr<sql::Statement> stmt(m_dbconnection->createStatement());
            stmt->execute("SET NAMES utf8mb4");
            
            SYLAR_LOG_INFO(g_logger) << "Database connection established";
        }
        
    }
    catch(const sql::SQLException& e)
    {
        SYLAR_LOG_ERROR(g_logger) << "Failed to create database connection: " << e.what();
        throw std::runtime_error(e.what());
    }
}

DbConnection::~DbConnection() 
{
    try 
    {
        cleanup();
    } 
    catch (...) 
    {
        // 析构函数中不抛出异常
    }
    SYLAR_LOG_ERROR(g_logger) << "Database connection established";
}

bool DbConnection::isValid() 
{
    try 
    {
        if (!m_dbconnection) 
        {
            return false;
        }
        std::unique_ptr<sql::Statement> stmt(m_dbconnection->createStatement());
        stmt->execute("SELECT 1");
        return true;
    } 
    catch (const sql::SQLException&) 
    {
        return false;
    }
}

void DbConnection::reconnect() 
{
    try 
    {
        if (m_dbconnection) 
        {
            m_dbconnection->reconnect();
        } 
        else 
        {
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
            m_dbconnection.reset(driver->connect(m_host, m_user, m_password));
            m_dbconnection->setSchema(m_database);
        }
    } 
    catch (const sql::SQLException& e) 
    {
        SYLAR_LOG_ERROR(g_logger) << "Reconnect failed: " << e.what();
        throw std::runtime_error(e.what());
    }
}

void DbConnection::cleanup() 
{
    MutexType::Lock lock(m_mutex);
    try 
    {
        if (m_dbconnection) 
        {
            // 确保所有事务都已完成
            if (!m_dbconnection->getAutoCommit()) 
            {
                m_dbconnection->rollback();
                m_dbconnection->setAutoCommit(true);
            }
            
            // 清理所有未处理的结果集
            std::unique_ptr<sql::Statement> stmt(m_dbconnection->createStatement());
            while (stmt->getMoreResults()) 
            {
                auto result = stmt->getResultSet();
                while (result && result->next()) 
                {
                    // 消费所有结果
                }
            }
        }
    } 
    catch (const std::exception& e) 
    {
        SYLAR_LOG_WARN(g_logger) << "Error cleaning up connection: " << e.what();
        try 
        {
            reconnect();
        } 
        catch (...) 
        {
            // 忽略重连错误
        }
    }
}

bool DbConnection::ping() 
{
    try 
    {
        // 不使用 getStmt，直接创建新的语句
        std::unique_ptr<sql::Statement> stmt(m_dbconnection->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery("SELECT 1 limit 1"));
        return true;
    } 
    catch (const sql::SQLException& e) 
    {
        SYLAR_LOG_INFO(g_logger) << "Ping failed: " << e.what();
        return false;
    }
}

}
}