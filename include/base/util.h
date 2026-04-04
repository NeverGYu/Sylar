#pragma once
#include <cstdint>
#include <sys/time.h>
#include <cxxabi.h> // for abi::__cxa_demangle()
#include <string>
#include <vector>
#include <iostream>
#include <fstream>


namespace sylar
{

/*------------------------- log.h  -----------------------------*/

/**
 * @brief 获取线程id
 * @note 这里不要把pid_t和pthread_t混淆，关于它们之的区别可参考gettid(2)
 */
pid_t GetThreadId();

/**
 * @brief 获取当前启动的毫秒数，参考clock_ge ttime(2)，使用CLOCK_MONOTONIC_RAW
 */
uint64_t GetElapsedMS(); 

/**
 * @brief 获取协程id
 * @todo 桩函数，暂时返回0，等协程模块完善后再返回实际值
 */
uint64_t GetFiberId();

/**
 * @brief 获取线程名称，参考pthread_getname_np(3)
 */
std::string GetThreadName();

/**
 * @brief 日期时间转字符串
 */
std::string TimeToStr(time_t ts = time(0), const std::string &format = "%Y-%m-%d %H:%M:%S");

/*---------------------  config.h  ------------------------*/

/**
 * @brief 获取T类型的类型字符串
 */
template<class T>
const char* TypeToName()
{
    static const char* s_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
    return s_name;
}


/*---------------------  macro.h  ------------------------*/

/**
 * @brief 获得当前的调用栈
 * @param[in] size 栈的最大层数
 * @param[in] skip 跳过栈顶的层数
 * @param[in] prefix 栈信息前输出的内容
 */
void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1);

/**
 * @brief 获取当前栈信息的字符串
 * @param[in] size 栈的最大层数
 * @param[in] skip 跳过栈顶的层数
 * @param[in] prefix 栈信息前输出的内容
 */
std::string BacktraceToString(int size, int skip, const std::string prefix="");



/*---------------------  timer.h  ------------------------*/

/**
 *  @brief 获取当前时间的毫秒
 */
uint64_t GetCurrentMS();


/**
 *  @brief 获取当前时间的微妙
 */
uint64_t GetCurrentUS();

/*---------------------  http.h  ------------------------*/
/**
 * @brief 字符串辅助类
 */
class StringUtil 
{
public:
    /**
    * @brief printf风格的字符串格式化，返回格式化后的string
    */
    static std::string Format(const char* fmt, ...);
    
    /**
     * @brief vprintf风格的字符串格式化，返回格式化后的string
    */
    static std::string Formatv(const char* fmt, va_list ap);
    
    /**
     * @brief url编码
     * @param[in] str 原始字符串
     * @param[in] space_as_plus 是否将空格编码成+号，如果为false，则空格编码成%20
     * @return 编码后的字符串
     */
    static std::string UrlEncode(const std::string& str, bool space_as_plus = true);

    /**
     * @brief url解码
     * @param[in] str url字符串
     * @param[in] space_as_plus 是否将+号解码为空格
     * @return 解析后的字符串
     */
    static std::string UrlDecode(const std::string &str, bool space_as_plus = true);

    /**
     * @brief 移除字符串首尾的指定字符串
     * @param[] str 输入字符串
     * @param[] delimit 待移除的字符串
     * @return  移除后的字符串
     */
    static std::string Trim(const std::string &str, const std::string &delimit = " \t\r\n");

    /**
     * @brief 移除字符串首部的指定字符串
     * @param[] str 输入字符串
     * @param[] delimit 待移除的字符串
     * @return  移除后的字符串
     */
    static std::string TrimLeft(const std::string &str, const std::string &delimit = " \t\r\n");

    /**
     * @brief 移除字符尾部的指定字符串
     * @param[] str 输入字符串
     * @param[] delimit 待移除的字符串
     * @return  移除后的字符串
     */
    static std::string TrimRight(const std::string &str, const std::string &delimit = " \t\r\n");

    /**
     * @brief 宽字符串转字符串
     */
    static std::string WStringToString(const std::wstring &ws);

    /**
     * @brief 字符串转宽字符串
     */
    static std::wstring StringToWString(const std::string &s);
};


/**
 * @brief 文件辅助类
 */
class FSUtil {
public:
    static void ListAllFile(std::vector<std::string>& files
                                ,const std::string& path
                                ,const std::string& subfix);

    static bool Mkdir(const std::string& dirname);

    static bool IsRunningPidfile(const std::string& pidfile);

    static bool Rm(const std::string& path);

    static bool Mv(const std::string& from, const std::string& to);

    static bool Realpath(const std::string& path, std::string& rpath);

    static bool Symlink(const std::string& frm, const std::string& to);

    static bool Unlink(const std::string& filename, bool exist = false);

    static std::string Dirname(const std::string& filename);

    static std::string Basename(const std::string& filename);

    static bool OpenForRead(std::ifstream& ifs, const std::string& filename
                        ,std::ios_base::openmode mode);

    static bool OpenForWrite(std::ofstream& ofs, const std::string& filename
                        ,std::ios_base::openmode mode);
};


class FileUtil
{
public:
    FileUtil(std::string filePath)
        : filePath_(filePath)
        , file_(filePath, std::ios::binary) // 打开文件，二进制模式
    {}
    
    ~FileUtil()
    {
        file_.close();
    }
    
    // 判断是否是有效路径
    bool isValid() const { return file_.is_open(); }
        
    // 重置打开默认文件
    void resetDefaultFile()
    {
        file_.close();
        file_.open("/Gomoku/GomokuServer/resource/NotFound.html", std::ios::binary);
    }
    
    uint64_t size()
    {
        file_.seekg(0, std::ios::end); // 定位到文件末尾
        uint64_t fileSize = file_.tellg();
        file_.seekg(0, std::ios::beg); // 返回到文件开头
        return fileSize;
    }
        
    void readFile(std::vector<char>& buffer)
    {
        if (file_.read(buffer.data(), size()))
        {
            std::cout << "File content load into memory (" << size() << " bytes)" << std::endl;
        }    
        else
        {
            std::cout  << "File read failed" << std::endl;
        }
    }
    
private:
    std::string     filePath_;
    std::ifstream   file_;
};

} 


