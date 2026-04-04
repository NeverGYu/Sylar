#include "fiber.h"
#include "log.h"
#include "atomic"
#include "config.h"
#include "ucontext.h"
#include "macro.h"
#include "scheduler.h"

namespace sylar
{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system"); 

// 全局静态变量，用于生成协程id
static std::atomic<uint64_t> s_fiber_id{0};
// 全局静态变量，用于统计协程的数量
static std::atomic<uint64_t> s_fiber_count{0};

// 线程局部变量，当前线程正在运行的协程
static thread_local Fiber* t_fiber = nullptr;
// 线程局部变量，当前线程的主协程，切换到这个协程，就相当于切换到了主线程中运行，智能指针形式
static thread_local Fiber::ptr t_thread_fiber;

// 配置变量，如果配置规定了协程栈的大小，就是配置优先
static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
    Config::Lookup<uint32_t>("fiber.stack_size", "fiber.stack_size", 128 * 1024);

/**
 * @brief malloc栈内存分配器
 */
class MallocStackAllocator
{
public:
    static void* Alloc(size_t size) { return malloc(size); }
    static void DeAlloc(void* p ,size_t size) { return free(p); }
};

using StackAllocator = MallocStackAllocator;

/**
 *  @brief 无参构造函数
 *  @details 该构造函数主要创建线程的第一个协程，该协程为线程的主协程
 *           同时这个协程只可以由GetThis获得
 */
Fiber::Fiber()
{
    SetThis(this);
    m_state = RUNNING;
    if (getcontext(&m_ctx))
    {
        SYLAR_ASSERT2(false,"getcontent");
    }
    ++s_fiber_count;
    m_id = s_fiber_id++;

    SYLAR_LOG_INFO(g_logger) << "Fiber::Fiber() main id = " << m_id;
}

/**
 *  @brief 有参构造函数
 *  @details 该构造函数主要用于创建子协程，该协程为线程的主协程的子协程
 */
Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler)
    : m_id(s_fiber_id++)
    , m_cb(cb)
    , m_runInScheduler(run_in_scheduler) 
{
    ++s_fiber_count;
    m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();
    m_stack = StackAllocator::Alloc(m_stacksize);
    if (getcontext(&m_ctx))
    {
        SYLAR_ASSERT2(false, "getcontext");
    }
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, Fiber::MainFunc,0);

    SYLAR_LOG_INFO(g_logger) << "Fiber::Fiber() id=" << m_id;
}

/**
 *  @brief 析构函数
 */
Fiber::~Fiber()
{
    SYLAR_LOG_INFO(g_logger) << "Fiber::~Fiber() id = " << m_id;
    --s_fiber_count;
    if (m_stack)    // 这说明有栈，这是个子协程
    {
        SYLAR_ASSERT(m_state == TERM);
        StackAllocator::DeAlloc(m_stack, m_stacksize);
        SYLAR_LOG_DEBUG(g_logger) << "dealloc stack, id = " << m_id;
    }
    else            // 这说明没栈，这是个主协程
    {
        SYLAR_ASSERT(!m_cb);
        SYLAR_ASSERT(m_state == RUNNING); // 主协程一定是执行状态

        Fiber* cur = t_fiber;     // 当前协程就是自己
        if (cur == this)
        {
            SetThis(nullptr);
        }
    }
    
}

/**
 *  @brief 重置协程状态和入口函数，复用栈空间，不重新创建栈
 */
void Fiber::reset(std::function<void()> cb)
{
    SYLAR_ASSERT(m_stack);
    SYLAR_ASSERT(m_state == TERM);
    m_cb = cb;
    if (getcontext(&m_ctx))
    {
        SYLAR_ASSERT2(false,"getcontext");
    }
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_size = m_stacksize;
    m_ctx.uc_stack.ss_sp = m_stack;

    makecontext(&m_ctx,Fiber::MainFunc,0);
    m_state = READY;
}


/**
 *  @brief 将当前协程切到执行状态
 *  @details 当前协程和正在运行的协程进行交换，前者状态为RUNNING，后者状态为READY
 */
void Fiber::resume()
{
    SYLAR_ASSERT(m_state != TERM && m_state != RUNNING);
    SetThis(this);
    m_state = RUNNING;

    // 如果协程参与调度器调度，那么应该和调度器的主协程进行swap，而不是线程主协程
    if (m_runInScheduler) 
    {
        if (swapcontext(&(Scheduler::GetMainFiber()->m_ctx), &m_ctx))
         {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    } 
    else 
    {
        if (swapcontext(&(t_thread_fiber->m_ctx), &m_ctx)) 
        {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }

}

/**
 *  @brief 当前协程让出执行权
 *  @details 当前协程与上次resume时退到后台的协程进行交换，前者状态变为READY，后者状态变为RUNNING
 */
void Fiber::yield()
{
    // 协程运行完之后会自动yield一次，用于回到主协程，此时状态已为结束状态
    SYLAR_ASSERT(m_state == RUNNING || m_state == TERM);
    if (m_state != TERM) 
    {
        m_state = READY;
    }
    // 如果协程参与调度器调度，那么应该和调度器的主协程进行swap，而不是线程主协程
    if (m_runInScheduler) 
    {
        SetThis(Scheduler::GetMainFiber());
        if (swapcontext(&m_ctx, &(Scheduler::GetMainFiber()->m_ctx))) 
        {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    } 
    else 
    {
        SetThis(t_thread_fiber.get());
        if (swapcontext(&m_ctx, &(t_thread_fiber->m_ctx))) 
        {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }
}


/**
 *  @brief 设置当前正在执行的协程，即设置线程局部变量 t_fiber
 */
void Fiber::SetThis(Fiber *f)
{
    t_fiber = f;
}

/**
 *  @brief 返回正在执行的协程
 *  @details 如果当前线程还未创建协程，则创建线程的第一个协程，且该协程为当前线程的主协程，其他协程都通过这个协程来调度，也就是说，其他协程
 *           结束时,都要切回到主协程，由主协程重新选择新的协程进行resume
 *  @attention 线程如果要创建协程，那么应该首先执行一下Fiber::GetThis()操作，以初始化主函数协程
 */
Fiber::ptr Fiber::GetThis()
{
    if (t_fiber)
    {
        return t_fiber->shared_from_this();
    }

    Fiber::ptr main_fiber(new Fiber);
    SYLAR_ASSERT(main_fiber.get() == t_fiber);
    t_thread_fiber = main_fiber;
    return t_fiber->shared_from_this();
}

/**
 *  @brief 获得总的协程数
 */
uint64_t Fiber::getTotalFiber()
{
    return s_fiber_count;
}

/**
 *  @brief 协程入口函数
 */
void Fiber::MainFunc()
{
    Fiber::ptr cur = GetThis();
    SYLAR_ASSERT(cur);
    cur->m_cb();
    cur->m_cb    = nullptr;
    cur->m_state = TERM;

    auto raw_ptr = cur.get(); // 手动让t_fiber的引用计数减1
    cur.reset();
    raw_ptr->yield();

    SYLAR_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
}

/**
 *  @brief 获得当前协程id
 */
uint64_t Fiber::GetFiberId()
{
    if (t_fiber)
    {
        return t_fiber->getId();
    }
    return 0;
}

}