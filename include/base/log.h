#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstdarg>
#include <list>
#include <map>
#include "util.h"
#include "mutex.h"
#include "singleton.h"
 
namespace sylar
{

/*------------- 宏定义 ---------------*/

/**
 * @brief 获得root日志器
 */
#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()

/**
 * @brief 获得指定名称的日志器
 */
#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)

/**
 * @brief 使用流式方式将日志级别为level的日志写入到logger中
 * @details 构造一个LoggerWrap对象，其中包含日志器和日志事件，在对象析构的时候自动将日志写入到日志器
 */
#define SYLAR_LOG_LEVEL(logger,level)\
    if(level <= logger->getLoggerLevel()) \
    sylar::LogEventWrap(logger, sylar::LogEvent::ptr(new sylar::LogEvent(logger->getLoggerName(), \
    level, __FILE__, __LINE__, sylar::GetElapsedMS() - logger->getCreateTime(), \
    sylar::GetThreadId(), sylar::GetFiberId(), time(0), sylar::GetThreadName()))).getLogEvent()->getSS()
    
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define SYLAR_LOG_ALERT(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ALERT)
            
#define SYLAR_LOG_CRIT(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::CRIT)
            
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
            
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
            
#define SYLAR_LOG_NOTICE(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::NOTICE)
            
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
            
#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)


/*------------- 类的定义 ---------------*/

/**
 * @brief 日志级别   
 */ 
class LogLevel
{
public:
    enum Level
    {
        FATAL = 0,      // 致命情况，系统不可用
        ALERT = 100,    // 高优先级情况，例如数据库系统崩溃
        CRIT = 200,     // 严重错误，例如硬盘错误
        ERROR = 300,    // 错误
        WARN = 400,     // 警告
        NOTICE = 500,   // 正常但值得注意
        INFO = 600,     // 一般信息
        DEBUG = 700,    // 调试信息
        NOTSET = 800    // 未设置
    };

    /**
     * @brief 日志级别转字符串
     * @param Level 日志级别
     * @return string  字符串形式的日志级别
     */
    static const char* LevelToString(LogLevel::Level level);


    /**
     * @brief 字符串形式的日志级别转换成LogLevel::Level
     * @param const char* 
     * @return LogLevel::Level
    */
   static const LogLevel::Level StringToLevel(const std::string& str);
};

/**
 * @brief 日志事件
 */
class LogEvent
{
public:
    using ptr = std::shared_ptr<LogEvent>;
    LogEvent(const std::string &loggerName, LogLevel::Level level, const char *fileName, 
        int32_t line, int64_t elapse, uint32_t threadid, uint64_t fiberid, 
        time_t time, const std::string &threadName)
        : m_level(level)
        , m_filename(fileName)
        , m_line(line)
        , m_elapse(elapse)
        , m_threadid(threadid)
        , m_fiberid(fiberid)
        , m_time(time)
        , m_threadName(threadName)
        , m_loggerName(loggerName)
    {}
    
    /**
     * @brief 用来获取对应的成员
     */
    LogLevel::Level getLevel() const { return m_level; } 
    std::string getContent() const { return m_ss.str(); }
    const char* getFileName() const {return m_filename;}
    int32_t getLine() const { return m_line; }
    uint64_t getElapse() const { return m_elapse; }
    uint32_t getThreadid() const { return m_threadid; }
    uint64_t getFiberid() const { return m_fiberid; }
    time_t getTime() const { return m_time; }
    std::string& getThreadName() { return m_threadName; }
    std::string& getLoggerName() { return m_loggerName; }
    std::stringstream& getSS() { return m_ss; }

    
private:
    LogLevel::Level m_level;    // 日志级别
    std::stringstream m_ss;     // 日志内容，用于流式存储
    const char* m_filename = 0; // 文件名
    int32_t m_line = 0;         // 行号
    uint64_t m_elapse = 0;      // 从日志器创建到当前日志的时间
    uint32_t m_threadid = 0;    // 线程号
    uint64_t m_fiberid = 0;     // 协程号
    time_t m_time;              // 时间戳
    std::string m_threadName;   // 线程名称
    std::string m_loggerName;   // 日志器的名称
};

/**
 * @brief 日志格式化器，仅用来初始化日志的输出模板
 * @param
 * 默认格式：%%d{%%Y-%%m-%%d %%H:%%M:%%S}%%T%%t%%T%%N%%T%%F%%T[%%p]%%T[%%c]%%T%%f:%%l%%T%%m%%n
 * 默认格式描述：年-月-日 时:分:秒 [累计运行毫秒数] \\t 线程id \\t 线程名称 \\t 协程id \\t [日志级别] \\t [日志器名称] \\t 文件名:行号 \\t 日志消息 换行符
 */
class LogFormatter
{
public:
    using ptr = std::shared_ptr<LogFormatter>;
    LogFormatter(const std::string &pattern = "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");
    void init();
    bool isError() { return m_error; }
    std::string getPattern() const { return m_pattern; }

    /**
     *  @brief 对日志事件进行格式化，返回ostream，里面是格式化日志
     *  @param os, event
     *  @return ostream
     */
    std::ostream& format(std::ostream& os, LogEvent::ptr event);

    /**
     *  @brief  对日志时间进行格式化，返回string
     *  @param  event
     *  @return string
     */
    std::string format(LogEvent::ptr event);

public:

    /**
    * @brief 日志处理类，专门用来格式化日志
    */
    class FormateItem
    {
    public:
        using ptr = std::shared_ptr<FormateItem>;
        virtual ~FormateItem() {}
        virtual void format(std::ostream& os, LogEvent::ptr event) = 0;
    };

private:
    std::string m_pattern;  // 日志模板格式
    std::vector<FormateItem::ptr> m_items;  // 用于存放不同的日志处理类
    bool m_error = false;   // 用于判断日志模板格式化的时候是否出错
};

/**
 * @brief 日志输出地，虚基类，用于派生出不同的LogAppender
 * @details 参考log4cpp，Appender自带一个默认的LogFormatter，以控件默认输出格式
 */
class LogAppender
{
public:
    using ptr = std::shared_ptr<LogAppender>;
    using MutexType = Spinlock;
    
    LogAppender(LogFormatter::ptr formatter);
    ~LogAppender() {}

    /**
     * @brief 设置日志格式器
     */
    void setLogFormatter(LogFormatter::ptr fmt);

    /**
     * @brief 获取日志格式器
     */
    LogFormatter::ptr getLogFormatter();

    /**
     * @brief 写入日志
     */
    virtual void log(LogEvent::ptr event) = 0;

     /**
     * @brief 将日志输出目标的配置转成YAML String
     */
    virtual std::string toYamlString() = 0;

protected:
    MutexType m_mutex;      // 互斥量
    LogFormatter::ptr m_formatter;       // 日志格式化器
    LogFormatter::ptr defalut_formatter; // 默认日志格式化器
};

class StdoutLogAppender : public LogAppender
{
public:
    using ptr = std::shared_ptr<StdoutLogAppender>;

    /**
     * @brief 构造函数
     */
    StdoutLogAppender();

    /**
     * @brief 用于覆写父类Appender的写日志方法
     */
    void log(LogEvent::ptr event) override;

    /**
     * @brief 将日志输出目标的配置转成YAML String
     */
    std::string toYamlString() override;
};

class FileAppender : public LogAppender
{
public:
    using ptr = std::shared_ptr<FileAppender>;

      /**
     * @brief 构造函数
     * @param[in] file 日志文件路径
     */
    FileAppender(const std::string& file);

    /**
     * @brief 写日志
     */
    void log(LogEvent::ptr event) override;

    /**
     * @brief 重新打开日志文件
     * @return 成功返回true
     */
    bool reopen();

    /**
     * @brief 将日志输出目标的配置转成YAML String
     */
    std::string toYamlString() override;

private:
    std::string m_filePath;     // 文件路径
    std::ofstream m_fileStream; // 文件流
    uint64_t m_lastTime = 0;        // 上次打开时间
    bool m_reopen_error  = false;        // 文件打开错误标识
};

class Logger
{
public:
    using ptr = std::shared_ptr<Logger>;
    using MutexType = Spinlock;

    /**
     * @brief 构造函数，默认值为default
     */
    Logger(const std::string& name= "default");

    /**
     * @brief 获取日志器名称
     */
    const std::string& getLoggerName() const { return m_name; }
    
    /**
     * @brief 获取创建时间
     */
    const uint64_t getCreateTime() const { return m_createTime; }

    /**
     * @brief 设置日志级别
     */
    void setLoggerLevel(LogLevel::Level level) { m_level =  level;}

    /**
     * @brief 获取日志级别
     */
    LogLevel::Level getLoggerLevel() const { return m_level; }

    /**
     * @brief 将appender添加到appender列表中
     */
    void addAppender(LogAppender::ptr appender);

    /**
     * @brief 将appender从appender列表中删除
     */
    void deleteAppender(LogAppender::ptr appender);

    /**
     *  @brief 清空appender列表
     */
    void clearAppenders();

    /**
     * @brief 将日志写入到每个appender 
     */
    void log(LogEvent::ptr event);

    /**
     * @brief 将日志器的配置转成YAML String
     */
    std::string toYamlString();

    /**
     *  @brief 获得appenders 
     */
    std::list<LogAppender::ptr> getAppenders()
    {
        return m_appenders;
    }

private:
    MutexType m_mutex;                          // 互斥量
    std::string m_name;                         // 日志器名称
    LogLevel::Level m_level;                    // 日志器的日志等级
    std::list<LogAppender::ptr> m_appenders;    // 日志器存放了一组日志输出地
    uint64_t m_createTime;                      // 日志器创建的时间
};

/**
 * @brief 日志事件包装器
 * @details 方便宏定义，内部包含日志时间和日志器
 */
class LogEventWrap
{
public:
    LogEventWrap(Logger::ptr logger, LogEvent::ptr event)
        : m_logger(logger)
        , m_event(event)
    {}

    /**
     *  @brief 析构函数
     *  @details 主要用于将日志事件自动写入到日志器并输出，类似于智能指针的自动析构 
     */
    ~LogEventWrap() { m_logger->log(m_event); }

    /**
     *  @brief 返回日志事件 
     */
    LogEvent::ptr getLogEvent() { return m_event; }
private:
    Logger::ptr m_logger;   // 日志器
    LogEvent::ptr m_event;  // 日志事件
};


class LoggerManager
{
public:
    using MutexType = Spinlock;
    LoggerManager();
    ~LoggerManager() {}
    void init();
    Logger::ptr getLogger(const std::string& name);
    Logger::ptr getRoot() { return m_root; };

     /**
     * @brief 将所有的日志器配置转成YAML String
     */
    std::string toYamlString();
private:
    MutexType m_mutex;   // 互斥量
    Logger::ptr m_root;  // root日志
    std::map<std::string, Logger::ptr> m_loggers;   // string(logger名称) <--> Logger：：ptr(日志器)
};

/**
 *  单例模式的日志管理器 
 */
using LoggerMgr = sylar::SingleTon<LoggerManager>;

} 



