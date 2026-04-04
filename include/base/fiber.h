#pragma once

#include <functional>
#include <memory>
#include <ucontext.h>
#include "thread.h"

namespace sylar
{
class Fiber : public std::enable_shared_from_this<Fiber>
{
public:
    using ptr = std::shared_ptr<Fiber>;

    /**
     *  @brief 协程的状态（用于切换时来表示） 
     */
    enum State
    {
        READY,      // 就绪态，刚创建或者yeild之后的状态
        RUNNING,       // 运行态，resume之后的状态
        TERM,       // 终止态，协程的回调函数执行完的状态
    };

private:

    /**
     *  @brief 无参构造函数 
     *  @details 该构造函数主要创建线程的第一个协程，该协程为线程的主协程
     *           同时这个协程只可以有GetThis获得
     */
    Fiber();

public:
    /**
     *  @brief 有参构造函数
     *  @details 该构造函数主要用于创建子协程，该协程为线程的主协程的子协程
     */
    Fiber(std::function<void()> cb, size_t stacksize = 0, bool run_in_scheduler = true);

    /**
     *  @brief 析构函数 
     */
    ~Fiber();

    /**
     *  @brief 重置协程状态和入口函数，复用栈空间，不重新创建栈 
     */
    void reset(std::function<void()>);

    /**
     *  @brief 将当前协程切到执行状态
     *  @details 当前协程和正在运行的协程进行交换，前者状态为RUNNING，后者状态为READY 
     */
    void resume();

    /**
     *  @brief 当前协程让出执行权
     *  @details 当前协程与上次resume时退到后台的协程进行交换，前者状态变为READY，后者状态变为RUNNING
     */
    void yield();

    /**
     *   @brief 返回协程id
     */
    uint64_t getId() const { return m_id; }


    /**
     *  @brief 获得协程状态 
     */
    uint64_t getState() const { return m_state; }

    /**
     *  @brief 设置当前正在执行的协程，即设置线程局部变量 t_fiber 
     */
    static void SetThis(Fiber* f);

    /**
     *  @brief 返回正在执行的协程
     *  @details 如果当前线程还未创建协程，则创建线程的第一个协程，且该协程为当前线程的主协程，其他协程都通过这个协程来调度，也就是说，其他协程
     *           结束时,都要切回到主协程，由主协程重新选择新的协程进行resume
     *  @attention 线程如果要创建协程，那么应该首先执行一下Fiber::GetThis()操作，以初始化主函数协程
     */
    static Fiber::ptr GetThis();
    
    /**
     *  @brief 获得总的协程数 
     */
    static uint64_t getTotalFiber();

    /**
     *  @brief 协程入口函数 
     */
    static void MainFunc();

    /**
     *  @brief 获得当前协程id 
     */
    static uint64_t GetFiberId();

private:
    uint64_t m_id = 0;                  //  协程id
    uint64_t m_stacksize = 0;           //  协程栈的大小
    ucontext_t m_ctx;                   //  协程栈
    void* m_stack = nullptr;            //  协程栈地址  
    std::function<void()> m_cb;         //  协程入口函数
    State m_state = READY;              //  协程的状态
    bool m_runInScheduler;              //  本协程是否参与调度器调度
};

} 