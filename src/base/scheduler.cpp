#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace sylar
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

// 每个线程都有一个调度器
static thread_local Scheduler* t_schedule = nullptr;
// 每个线程都有一个主协程
static thread_local Fiber* t_schedule_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
{
    SYLAR_ASSERT(threads > 0);

    m_useCaller = use_caller;
    m_name = name;
    if (use_caller)         // 这表示打算将当前的线程用作调度线程
    {
        --threads;          // 将调度线程的数量减一
        Fiber::GetThis();   // 创建当前调度线程的主协程
        SYLAR_ASSERT(GetThis() == nullptr);    
        t_schedule = this;  // 当前的调度线程为自己

        /**
         * caller 线程的主协程不会被调度协程run进行调度，而且，线程的调度协程停止时，应该返回到caller的主协程
         * 在 user caller 情况下，把 caller 线程的主协程暂时保存起来，等调度协程结束时，再resume caller协程
         */

        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, false));   // 创建主协程后的调度协程
        Thread::setName(m_name);               // 设置线程名称
        t_schedule_fiber = m_rootFiber.get();  // 保存调度协程
        m_rootThread = sylar::GetThreadId();   // 设置线程id
        m_tids.push_back(m_rootThread);    // 将当前线程id保存到线程id集合中
    }
    else
    {
        m_rootThread = -1;
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler()
{
    SYLAR_LOG_DEBUG(g_logger) << "Scheduler:~Scheduler()" ;
    SYLAR_ASSERT(m_stopping);
    if (GetThis() == this)
    {
        t_schedule = nullptr;
    }
}

Scheduler* Scheduler::GetThis()
{
    return t_schedule;
}

Fiber* Scheduler::GetMainFiber()
{
    return t_schedule_fiber;
}

void Scheduler::start()
{
    SYLAR_LOG_INFO(g_logger) << "start()";
    MutexType::Lock lock(m_mutex);
    if (m_stopping)
    {
        SYLAR_LOG_ERROR(g_logger) << "Scheduler is stopping";
        return;
    }
    SYLAR_ASSERT(m_threads.empty());
    m_threads.resize(m_threadCount);

    for (size_t i = 0; i < m_threads.size(); ++i)
    {
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run,this), m_name + "_" + std::to_string(i)));
        m_tids.push_back(m_threads[i]->getId());
    }
}

bool Scheduler::stopping()
{
    MutexType::Lock lock(m_mutex);
    return m_stopping && m_tasks.empty() && m_activeCount == 0;
}

void Scheduler::tickle()
{
    SYLAR_LOG_INFO(g_logger) << "tickle"; 
}

void Scheduler::idle()
{
    SYLAR_LOG_DEBUG(g_logger) << "idle";
    while(!stopping())
    {
        sylar::Fiber::GetThis()->yield();
    }
}

void Scheduler::stop()
{
    SYLAR_LOG_INFO(g_logger) << "stop";
    if (stopping())
    {
        return;
    }
    m_stopping = true;

    // 判断是否使用了caller线程
    if (m_useCaller)
    {
        SYLAR_ASSERT(GetThis() == this);
    }
    else
    {
        SYLAR_ASSERT(GetThis() != this);
    }
    for (size_t i = 0; i < m_threadCount; i++)
    {
        tickle();
    }

    if (m_rootFiber)
    {
        tickle();
    }

    // 在use caller情况下，调度器协程结束时，应该返回caller协程
    if (m_rootFiber)
    {
        m_rootFiber->resume();
        SYLAR_LOG_DEBUG(g_logger) << "m_rootFiber end";
    }

    std::vector<Thread::ptr> thres;
    {
        MutexType::Lock lock(m_mutex);
        thres.swap(m_threads);
    }
    
    for (auto& i : thres)
    {
        i->join();
    }
    
}

void Scheduler::setThis()
{
    t_schedule = this;
}

void Scheduler::run()
{
    SYLAR_LOG_INFO(g_logger) << "run";
    // 这主要用于hook系统调用
    set_hook_enable(true); 
    // 调度器创建的每个线程都会个变量t_schedule指向调度器 
    setThis();  
    if (sylar::GetThreadId() != m_rootThread)   // 这表示不是当前线程的id不是调度线程的id
    {
        t_schedule_fiber = sylar::Fiber::GetThis().get();   // 线程创建协程
    }

    Fiber::ptr idle(new Fiber(std::bind(&Scheduler::idle, this)));  // 创建懒惰携程
    Fiber::ptr cb_fiber;
    ScheduleTask task;  
    while (true)
    {
        task.reset();
        bool tickle_me = false;     // 是否tickle其他线程进行任务调度
        {
            MutexType::Lock lock(m_mutex);
            auto it = m_tasks.begin();
            while (it != m_tasks.end())
            {
                /**
                 *  @brief 情况一：当前任务指定了所属线程，但不是当前线程
                 */
                if (it->threadid != -1 && it->threadid != sylar::GetThreadId())
                {
                    ++it;
                    tickle_me = true;
                    continue;
                }

                // 判断任务至少存在
                SYLAR_ASSERT(it->fiber || it->cb);
                
                /**
                 *  @brief 情况二：当前任务存在，但是正在运行在该线程的协程上
                 */
                if (it->fiber && it->fiber->getState() == Fiber::RUNNING)
                {
                    ++it;
                    continue;
                }

                /**
                 *  @brief 情况三：当前调度线程找到了一个任务，准备开始调度（将其从任务队列中删除，活动线程数量加一）
                 */
                task = *it;
                m_tasks.erase(it++);
                ++m_activeCount;
                break;
            }
            
            // 当前线程如果拿到一个任务后，发现任务队列中还有任务，就tick其他线程
            tickle_me |= (it != m_tasks.end());
            
        }

        // 通知其他线程
        if (tickle_me)
        {
            tickle();
        }

        if (task.fiber)
        {
            // 使该task对应的协程执行，resume返回时，协程要么执行完了，要么半路yield了，总之这个任务就算完成了，活跃线程数减一
            task.fiber->resume();
            --m_activeCount;
            task.reset();
        }
        else if (task.cb)
        {
            if (cb_fiber)
            {
                cb_fiber->reset(task.cb);
            }
            else
            {
                cb_fiber.reset(new Fiber(task.cb));
            }
            task.reset();
            cb_fiber->resume();
            --m_activeCount;
            cb_fiber.reset();
        }
        else    // 进入到这个分支，说明没有在任务队列中取到task
        {
            if (idle->getState() == Fiber::TERM)
            {
                // 这说明调度器已被停止
                SYLAR_LOG_INFO(g_logger) << "idle fiber term";
                break;
            }
            ++m_idleCount;
            idle->resume();
            --m_idleCount;
        }
    }
    SYLAR_LOG_INFO(g_logger) << "Scheduler::run() exit";
}


}