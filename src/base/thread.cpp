#include "thread.h"
#include "log.h"
namespace sylar
{

static thread_local Thread* t_thread = nullptr;

static thread_local std::string t_thread_name = "UNKNOWN";

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

/**
 *  @brief 构造函数
 *  @param[in] cb 线程执行函数
 *  @param[in] name 线程名称
 */
Thread::Thread(std::function<void()> cb, const std::string &name)
    : m_cb(cb)
    , m_name(name)
{
    if (name.empty())
    {
        m_name = "UNKNOWN";
    }
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if (rt)
    {
        SYLAR_LOG_ERROR(g_logger) << "pthread create thread fail , rt=" << rt << "name="<< name;
        throw std::logic_error("pthread create error");
    }
    m_semaphore.wait();
}

/**
 * @brief 析构函数
 */
Thread::~Thread()
{
    if (m_thread)
    {
        pthread_detach(m_thread);
    }   
}

/**
 *  @brief 等待线程执行结束
 */
void Thread::join()
{
    if (m_thread)
    {
        int rt = pthread_join(m_thread,nullptr);
        if (rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "pthread join thread fail , rt=" << rt << "name="<< m_name;
            throw std::logic_error("pthread join error");
        }
        m_thread = 0;
    }
    
}

/**
 *  @brief 设置分离线程
 */
void Thread::detach()
{
    if (m_thread)
    {
        int rt = pthread_detach(m_thread);
        if (rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "pthread detach thread fail, rt=" << rt << " name=" << m_name;
            throw std::logic_error("pthread join error");
        }
        m_thread = 0;
    }
    
}

/**
 *  @brief 返回子线程对象
 */
Thread* Thread::GetThis()
{
    return t_thread;
}

/**
 *  @brief 返回子线程的名称
 */
const std::string& Thread::GetName()
{
    return t_thread_name;
}

/**
 *   @brief 设置子线程的名称
 */
void Thread::setName(const std::string &name)
{
    if (name.empty())
    {
        return;
    }
    if (t_thread)
    {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

/**
 * @brief 线程执行函数：可以接受任何类型，以及返回任何类型 
 */
void* Thread::run(void* arg)
{

    Thread* thread = (Thread*)(arg);
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = sylar::GetThreadId();
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());
    
    std::function<void()> cb;
    cb.swap(thread->m_cb);

    thread->m_semaphore.notify();
    cb();
    return 0;
}


}

