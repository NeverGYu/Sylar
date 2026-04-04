# pragma once

#include "scheduler.h"
#include "timer.h"

namespace sylar
{

class IOManager : public Scheduler, public TimerManager
{
public:
    using ptr = std::shared_ptr<IOManager>;
    using RWMutexType = RWMutex;

    /**
     *  @brief 事件类型 
     */
    enum Event
    {
        NONE = 0x0,
        READ = 0x1,
        WRITE = 0x4
    };

private:

    /**
     *  @brief 封装句柄（文件描述符） 
     */
    struct FdContext
    {
        using MutexType = Mutex;

        /**
         *  @brief 事件绑定的回调函数 
         */
        struct EventContext
        {
            Scheduler* scheduler = nullptr;
            std::function<void()> cb;
            Fiber::ptr fiber;           
        };

        /**
         * @brief 获取事件上下文类
         * @param[in] event 事件类型
         * @return 返回对应事件的上下文
         */
        EventContext& getEventContext(Event event);

        /**
         * @brief 重置事件上下文
         * @param[in, out] ctx 待重置的事件上下文对象
         */
        void resetEventContext(EventContext &ctx);

        /**
         * @brief 触发事件
         * @details 根据事件类型调用对应上下文结构中的调度器去调度回调协程或回调函数
         * @param[in] event 事件类型
         */
        void triggerEvent(Event event);
        
        int fd = 0;                     // 对应的文件描述符
        EventContext read;              // 读事件
        EventContext write;             // 写事件
        Event m_events = NONE;          // 事件集（每一位对应读写事件）
        MutexType m_mutex;              // 互斥锁
    };

public:
    /**
     *  @brief 构造函数 
     */
    IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "Scheduler");

    /**
     *  @brief 析构函数 
     */
    ~IOManager();

    /**
     *  @brief 往句柄上添加事件
     *  @param[in] fd   
     *  @param[in] Event
     */
    int addEvent(int fd, Event event, std::function<void()> cb  = nullptr);

    /**
     *  @brief 往句柄上删除事件
     *  @param[in] fd   
     *  @param[in] Event
     */
    bool delEvent(int fd, Event event);

    /**
     *  @brief 往句柄上取消事件
     *  @param[in] fd   
     *  @param[in] Event
     */
    bool cancelEvent(int, Event event);

    /**
     *  @brief 取消句柄上的所有事件
     *  @param[in] fd   
     */
    bool cancelAll(int fd);

    /**
     *  @brief 获得当前的IOManager 
     */
    static IOManager* GetThis();

protected: 
    /**
     *  @brief 通知协程调度器有任务了 
     *  @details 写pipe让idle协程从epoll_wait退出，待idle协程yield之后Scheduler::run就可以调度其他任务
     */
    void tickle() override;

     /**
     * @brief 判断是否可以停止
     * @details 判断条件是Scheduler::stopping()外加IOManager的m_pendingEventCount为0，表示没有IO事件可调度了
     */
    bool stopping() override;

    /**
     *  @brief 无任务调度执行idle协程 
     *  @details 对于IO协程调度来说，应阻塞在等待IO事件上，idle退出的时机是epoll_wait返回，对应的操作是tickle或注册的IO事件发生
     */
    void idle() override;

     /**
     * @brief 判断是否可以停止，同时获取最近一个定时器的超时时间
     * @param[out] timeout 最近一个定时器的超时时间，用于idle协程的epoll_wait
     * @return 返回是否可以停止
     */
    bool stopping(uint64_t& timeout);

    /**
     * @brief 当有定时器插入到头部时，要重新更新epoll_wait的超时时间，这里是唤醒idle协程以便于使用新的超时时间
     */
    void onTimerInsertAtFront() override;

     /**
     * @brief 重置socket句柄上下文的容器大小
     * @param[in] size 容量大小
     */
    void contextResize(size_t size);

private:
    int m_epfd = 0;                                 // 内核事件表
    int m_tickleFds[2];                             // 管道通信--> 用于通知陷入epoll_wait的线程 --> [0]读端，[1]写端
    std::atomic<size_t> m_pendingEventCount = {0};  // 当前等待执行的IO事件数量
    RWMutexType m_mutex;                            // 读写锁
    std::vector<FdContext*> m_fdcontexts;           // 句柄数组（保存封装好的句柄）
};


}