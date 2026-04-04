#pragma once

#include "noncopyable.h"
#include "mutex.h"
#include <thread>
#include <memory>
#include <functional>
#include <pthread.h>
#include <string>
#include <sys/types.h>

namespace sylar
{

class Thread : Noncopyable
{
public:
    using ptr = std::shared_ptr<Thread>;
    /**
     *  @brief 构造函数
     *  @param[in] cb 线程执行函数
     *  @param[in] name 线程名称
      */
    Thread(std::function<void()> cb, const std::string& name);

    /**
     * @brief 析构函数 
     */
    ~Thread();

    /**
     *  @brief 获得线程id 
     */
    pid_t getId() const { return m_id; }

    /**
     *  @brief 获得当前线程的名称 
     */
    std::string getName() const { return m_name; }

    /**
     *  @brief 等待线程执行结束 
     */
    void join();

    /**
     *  @brief 设置分离线程 
     */
    void detach();
    
    /**
     *  @brief 返回当前线程对象
     */
    static Thread* GetThis();

    /**
     *  @brief 返回当前线程的名称
     */
    static const std::string& GetName();

    /**
     *   @brief 设置线程的名称
     */
    static void setName(const std::string& name);
private:
    /**
     *  @brief 线程执行函数：可以接受任何类型，以及返回任何类型 
     */
    static void* run(void* arg);
private:
    std::string m_name;             // 当前线程的名字
    pid_t m_id = -1;                // 当前线程的id
    pthread_t m_thread = 0;         // 当前的子线程对象
    std::function<void()> m_cb;     // 当前线程可执行的回调函数
    Semaphore m_semaphore;          // 当前线程的信号量
};

} // namespace sylarclass Thread : Noncopyable
