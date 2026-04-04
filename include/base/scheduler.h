#pragma once

#include <functional>
#include <list>
#include <memory>
#include <string>
#include "fiber.h"
#include "log.h"
#include "thread.h"

namespace sylar
{


/**
 *  @brief 协程调度器
 *  @details 封装的是N-M的协程调度器
 *           里面有一个线程池，支持协程在线程池中进行切换 
 */
class Scheduler
{
public:
    using ptr = std::shared_ptr<Scheduler>;
    using MutexType = Mutex;
    using Tid = size_t;

    /**
     *  @brief 构造函数
     *  @param[in] threads 线程数
     *  @param[in] usr_caller 是否将当前线程也作为调度线程
     *  @param[in] name 调度器的名称
     */
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "Scheduler");

    /**
     *  @brief 析构函数 
     */
    ~Scheduler();

    /**
     *  @brief 获取调度器的名称 
     */
    const std::string& getName() const { return m_name; }

    /**
     *  @brief 获得当前线程调度器的指针 
     */
    static Scheduler* GetThis();

    /**
     *  @brief 获得当前线程的主协程 
     */
    static Fiber* GetMainFiber();

    /**
     *  @brief 调价调度任务
     *  @tparam FiberOrcb 调度任务类型，可以是 Fiber 以及 function函数
     *  @param fc FiberOrcb类型
     *  @param threadid 指定运行该任务的线程号，-1 表示任意线程
     */
    template<typename FiberOrcb>
    void schedule(FiberOrcb ft, size_t threadid = -1)
    {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            need_tickle = scheduleNoLock(ft, threadid);
        }

        if (need_tickle)    // 这说明现在任务队列中有任务，需要唤醒idle协程
        {
            tickle();       // 唤醒idle协程
        }
        
    }

    /**
     *  @brief 启动调度器 
     */
    void start();

    /**
     *  @brief 停止调度器，等所有调度任务都执行完了再返回
     */
    void stop();

protected:
    /**
     *  @brief 通知协程调度器有任务了 
     */
    virtual void tickle();

    /**
     *  @brief 协程调度函数 
     */
    void run();

    /**
     *  @brief 无任务调度执行idle协程 
     */
    virtual void idle();

    /**
     *  @brief 返回是否可以停止 
     */
    virtual bool stopping();

    /**
     *  @brief 设置当前的协程调度器 
     */
    void setThis();

    /**
     *  @brief 返回是否有空闲线程 
     */
    bool hasIdleThreads() { return m_threadCount > 0; }

private:

    /**
     *  @brief 该函数用于标记是否需要唤醒调度器进行任务调度
     */
    template<typename FiberOrcb>
    bool scheduleNoLock(FiberOrcb ft, size_t threadid)
    {
        bool need_tickle = m_tasks.empty();
        ScheduleTask task(ft, threadid);
        if (task.fiber || task.cb)
        {
            m_tasks.push_back(task);
        } 
        return need_tickle;     
    }

     /**
     * @brief 调度任务，协程/函数二选一，可指定在哪个线程上调度
     */
    class ScheduleTask
    {
    public:
        friend class Scheduler;

        /**
         *  @brief 默认构造函数 
         */
        ScheduleTask()
        {
            threadid =  -1;
        }   

        /**
         *  @brief 构造函数 
         */
        ScheduleTask(Fiber::ptr f, int thr)
        {
            fiber = f;
            threadid = thr;
        }

        ScheduleTask(Fiber::ptr* f, int thr)
        {
            fiber.swap(*f);
            threadid = thr;
        }

        ScheduleTask(std::function<void()> c, int thr)
        {
            cb = c;
            threadid = thr;
        }
        
        void reset()
        {
            fiber = nullptr;
            cb = nullptr;
            threadid = -1;
        }
        
    private:
        Fiber::ptr fiber;
        std::function<void()> cb;
        int threadid;
    };

private:
    std::string m_name;                         // 调度器的名称
    MutexType m_mutex;                          // 互斥量
    std::vector<Thread::ptr> m_threads;         // 线程池
    std::list<ScheduleTask> m_tasks;            // 任务队列
    std::vector<Tid> m_tids;                    // 线程池中包含的线程id
    size_t m_threadCount = 0;                   // 线程数量
    std::atomic<size_t> m_activeCount = {0};    // 活跃线程数量
    std::atomic<size_t> m_idleCount = {0};      // 不活跃线程数量
    bool m_useCaller;                           // 判断是否执行Scheduler构造函数的线程
    Fiber::ptr m_rootFiber;                     // 当 m_userCaller = true 时，主协程以后的第一个协程
    uint64_t m_rootThread = 0;                  // 当 m_userCaller = true 时，调度器所在线程的线程id
    bool m_stopping = false;                    // 是否正在停止
};

}