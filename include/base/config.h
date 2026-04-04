#pragma once

#include <memory>
#include <string>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <yaml-cpp/yaml.h>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "mutex.h"
#include "log.h"
#include "util.h"
#include "../middleware/cors/CorsConfig.h"

namespace sylar
{
/**
 * @brief 配置基类 
 */
class ConfigVarBase
{
public:
    using ptr = std::shared_ptr<ConfigVarBase>;

    /**
     * @brief 构造函数
     */
    ConfigVarBase(const std::string& name, const std::string& description)
        : m_name(name)
        , m_description(description)
    {}

    /**
     * @brief 析构函数
    */
    virtual ~ConfigVarBase() {}

    /**
     * @brief 返回配置的名称
    */
    std::string& getName() { return m_name; }

    /**
     * @brief 返回配置的描述
    */
    std::string& getDescription() { return m_description; }

    /**
     * @brief 解析成字符串
    */
    virtual std::string toString() = 0;

    /**
     * @brief 从字符串转换
    */
    virtual bool fromString(const std::string& val) = 0;

    /**
     * @brief 返回配置参数值的类型名称 
     */
    virtual std::string getTypeName() const = 0;
protected:
    std::string m_name;         // 配置的名称
    std::string m_description;  // 配置的描述
};


/*-----------------------  类型转换模板类  -----------------------------------*/
/**
 * @brief     F:原类型 <--> T:目标类型
 * @details   使用函数对象机制
*/
template<class F, class T>
class lexicalCast
{
public:
    T operator()(const F & v)
    {
        return boost::lexical_cast<T>(v);
    }
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::vector<T>)
 */
template<class T>
class lexicalCast<std::string, std::vector<T>>
{
public:
    std::vector<T> operator() (const std::string& val)
    {
        YAML::Node node = YAML::Load(val);
        typename std::vector<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i)
        {
            ss.str("");
            ss << node[i];
            vec.push_back(lexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};


/**
 * @brief 类型转换模板类偏特化(std::vector<T> 转换成 YAML String)
 */
template<class T>
class lexicalCast<std::vector<T>, std::string>
{
public:
    std::string operator()(const std::vector<T> vec)
    {
        YAML::Node node(YAML::NodeType::Sequence);
        for(auto& i : vec)
        {
            node.push_back(YAML::Load(lexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
    
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::list<T>)
 */
template<class T>
class lexicalCast<std::string, std::list<T>>
{
public:
    std::list<T> operator()(const std::string& val)
    {
        YAML::Node node = YAML::Load(val);
        typename std::list<T> lst;
        std::stringstream ss;
        for(int i = 0; i < node.size(); ++i)
        {
            ss.str("");
            ss << node[i];
            lst.push_back(lexicalCast<std::string, T>()(ss.str()));
        }
        return lst;
    }
};

/**
 * @brief 类型转换模板类偏特化(std::list<T> 转换成 YAML String)
 */
template<class T>
class lexicalCast<std::list<T>, std::string>
{
public:
    std::string operator()(std::list<T> lst)
    {
        YAML::Node node(YAML::NodeType::Sequence);
        for (auto& i : lst)
        {
            node.push_back(YAML::Load(lexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成std::set<T> )
 */
template<class T>
class lexicalCast<std::string, std::set<T>>
{
public:
    std::set<T> operator()(const std::string& val)
    {
        YAML::Node node = YAML::Load(val);
        typename std::set<T> s;
        std::stringstream ss;
        for (int i = 0; i < node.size(); ++i)
        {   
            ss.str("");
            ss << node[i];
            s.insert(lexicalCast<std::string, T>()(ss.str()));
        }
        return s;
    }
};

/**
 * @brief 类型转换模板类偏特化(std::set<T> 转换成 YAML String)
 */
template<class T>
class lexicalCast<std::set<T>, std::string>
{
public:
    std::string operator()(const std::set<T> s)
    {
        YAML::Node node(YAML::NodeType::Sequence);
        for (auto& i : s)
        {
            node.push_back(YAML::Load(lexicalCast<T,std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};


/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::unordered_set<T>)
 */
template <class T>
class lexicalCast<std::string, std::unordered_set<T>> {
public:
    std::unordered_set<T> operator()(const std::string &v) {
       YAML::Node node = YAML::Load(v);
       typename std::unordered_set<T> us;
       std::stringstream ss;
       for (int i = 0; i < node.size(); i++)
       {
            ss.str("");
            ss << node[i];
            us.insert(lexicalCast<std::string, T>()(ss.str()));
       }
       return us;
    }
};

/**
 * @brief 类型转换模板类片特化(std::unordered_set<T> 转换成 YAML String)
 */
template <class T>
class lexicalCast<std::unordered_set<T>, std::string> {
public:
    std::string operator()(const std::unordered_set<T> &v) {
       YAML::Node node(YAML::NodeType::Sequence);
       std::stringstream ss;
       for (auto& i : v)
       {
            node.push_back(YAML::Load(lexicalCast<T, std::string>()(i)));
       }
       ss << node;
       return ss.str();
    }
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::map<std::string, T>)
 */
template<class T>
class lexicalCast<std::string, std::map<std::string, T>>
{
public:
    std::map<std::string, T> operator()(const std::string& val)
    {
        YAML::Node node = YAML::Load(val);
        typename std::map<std::string, T> m;
        std::stringstream ss;
        for (auto it = node.begin(); it != node.end(); ++it)
        {
            ss.str("");
            ss << it->second;
            m.insert(std::make_pair(it->first.Scalar(), lexicalCast<std::string, T>()(ss.str())));
        }
        return m;
    }
};

/**
 * @brief 类型转换模板类偏特化(std::map<std::string, T> 转换成 YAML String)
 */
template<class T>
class lexicalCast<std::map<std::string, T>, std::string>
{
public:
    std::string operator()(std::map<std::string, T> &m)
    {
        YAML::Node node(YAML::NodeType::Map);
        for(auto& i : m)
        {
            node[i.first] = YAML::Node(lexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::unorder_map<std::string, T>)
 */
template<class T>
class lexicalCast<std::string, std::unordered_map<std::string, T>>
{
public:
    std::unordered_map<std::string, T> operator()(const std::string& val)
    {
        YAML::Node node = YAML::Load(val);
        typename std::unordered_map<std::string, T> um;
        std::stringstream ss;
        for (auto it = node.begin(); it != node.end(); ++it)
        {
            ss.str("");
            ss << it->second;
            um.insert(std::make_pair(it->first.Scalar(), lexicalCast<std::string, T>()(ss.str())));
        }
        return um;
    }
};

/**
 * @brief 类型转换模板类偏特化(std::unorder_map<std::string, T> 转换成 YAML String)
 */
template<class T>
class lexicalCast<std::unordered_map<std::string, T>, std::string>
{
public:
    std::string operator()(const std::unordered_map<std::string, T> um)
    {
        YAML::Node node(YAML::NodeType::Map);
        std::stringstream ss;
        for (auto& i : um)
        {
            node[i.first] = YAML::Load(lexicalCast<T, std::string>()(i.second));
        }
        ss << node;
        return ss.str();
    }
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 http::middleware::CorsConfig)
 */
template<>
class lexicalCast<std::string, sylar::middleware::CorsConfig>
{
public:
    sylar::middleware::CorsConfig operator()(const std::string& val)
    {
        // 先将字符串转换成YAML::Node
        YAML::Node node = YAML::Load(val);
        // 定义需要的类型middle::CorsConfig
        sylar::middleware::CorsConfig config;
        // 进行转化
        if (node["allowedOrigins"])
        {
            config.allowedOrigins = node["allowedOrigins"].as<std::vector<std::string>>();
        }
        if (node["allowedMethods"])
        {
            config.allowedMethods = node["allowedMethods"].as<std::vector<std::string>>();
        }
        if (node["allowedHeaders"])
        {
            config.allowedHeaders = node["allowedHeaders"].as<std::vector<std::string>>();
        }
        if (node["allowCredentials"])
        {
            config.allowCredentials = node["allowCredentials"].as<bool>();
        }
        if (node["maxAge"])
        {
            config.maxAge = node["maxAge"].as<int>();
        }
        return config;
    }
};

/**
 * @brief 类型转换模板类偏特化(http::middleware::CorsConfig 转换成 YAML String)
 */
template<>
class lexicalCast<sylar::middleware::CorsConfig, std::string>
{
public:
    std::string operator()(const sylar::middleware::CorsConfig& config)
    {
        YAML::Node node;
        std::stringstream ss;
        node["allowedOrigins"] = config.allowedOrigins;
        node["allowedMethods"] = config.allowedMethods;
        node["allowedHeaders"] = config.allowedHeaders;
        node["allowCredentials"] = config.allowCredentials;
        node["maxAge"] = config.maxAge;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief 配置的子类
*/
template<class T, class FromStr = lexicalCast<std::string, T>, class ToStr = lexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase
{
public:
    using ptr = std::shared_ptr<ConfigVar>;
    using on_change_cb = std::function<void(const T& old_value, const T& new_value)>;

    /**
     * @brief 构造函数
     */
    ConfigVar(const std::string& name, const std::string& description, const T& value)
        : ConfigVarBase(name,description)
        , m_value(value)
    {}
    
    /**
     * @brief   将参数转换成YAML String
     * @details 转换失败则抛出异常
    */
    std::string toString() override
    {
        try
        {
            return  ToStr()(m_value);
        }
        catch(const std::exception& e)
        {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception "
                                              << e.what() << " convert: " << TypeToName<T>() << " to string "
                                              << "name =" << m_name;
        }
        return "";
    }

    /**
     * @brief 将String转换成YAML类型
    */
    bool fromString(const std::string& val) override
    {
        try
        {
            setValue(FromStr()(val));
        }
        catch(const std::exception& e)
        {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::fromString exception "
                                              << e.what() << " convert: string to" << TypeToName<T>()
                                              << "name =" << m_name
                                              << " - " << val;
        
        }
        return false;
    }


    /**
     * @brief 获取当前参数的值
    */
    const T getValue() {  return m_value; }

    /**
     * @brief 设置当前参数的值
    */
    void setValue(const T& value)
    {
        if (value == m_value)
        {
            return;
        }
        for (auto& i :m_cbs)
        {
            i.second(m_value,value);
        }
        m_value = value;
    }

    /**
     * @brief 返回配置参数值的类型名称 
     */
    std::string getTypeName() const override
    {
        return TypeToName<T>();
    }

    /**
     * @brief 监听函数
     * @return 返回该回调函数对应的唯一id,用于删除回调
     */
    uint64_t addlistener(on_change_cb cb)
    {
        static uint64_t s_fun_id = 0;
        ++s_fun_id;
        m_cbs[s_fun_id] = cb;
        return s_fun_id;
    } 

    /**
     * @brief 删除函数
     */
    void deletelistener(uint64_t key)
    {
        m_cbs.erase(key);
    }

    /**
     * @brief 清理函数
     */
    void clearlistener()
    {
        m_cbs.clear();
    }

    /**
     * @brief 返回回调函数
     */
    on_change_cb getlistener(uint64_t key)
    {
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it->seocnd ; 
    }

private:
    T m_value;
    std::unordered_map<uint64_t, on_change_cb> m_cbs;     // 回调函数组： key: 要求唯一，一般用hash
};

/**
 * @brief ConfigVar的管理类
 * @details 提供便捷的方法创建/访问ConfigVar
 */
class Config
{
public:
    using ConfigVarMap = std::unordered_map<std::string, ConfigVarBase::ptr>;

    using RWMutexType = RWMutex ;
    /**
     * @brief 寻找是否有对应名称的参数 
     * @details 如果没有就创建配置参数
     * @return ConfigVar<T>::ptr
    */
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name, const std::string& description, const T& val)
    {
        auto it = GetDatas().find(name);
        if (it != GetDatas().end())
        {
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
            if (tmp)
            {
                SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name= " << name << "exists";
                return tmp;
            }
            else
            {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name= " << name << " exists but type not"
                                                 << TypeToName<T>() << " real type=" << it->second->getTypeName()
                                                 <<" " << it->second->toString();
            }
        }

        if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos)
        {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }

        typename ConfigVar<T>::ptr newConfigVar(std::make_shared<ConfigVar<T>>(name, description, val));
        GetDatas()[name] = newConfigVar;
        return newConfigVar;
    }

    /**
     * @brief 寻找是否有对应名称的参数
     * @return ConfigVar<T>::ptr
    */
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name)
    {
        auto it = GetDatas().find(name);
        if (it == GetDatas().end())
        {
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);  
    }

    /**
     * @brief 使用YAML::Node初始化配置模块
     */
    static void LoadFromYaml(const YAML::Node& node);

    /**
     * @brief 加载path文件夹里面的配置文件
     */
    static void LoadFromConfDir(const std::string& path, bool force = false);

    /**
     * @brief 查找配置参数,返回配置参数的基类
     * @param[in] name 配置参数名称
     */
    static ConfigVarBase::ptr LookupBase(const std::string& name);

    /**
     * @brief 遍历配置模块里面所有配置项
     * @param[in] cb 配置项回调函数
     */
    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);

private:
    /**
     * @brief 返回所有的配置项
     */
    static ConfigVarMap& GetDatas()
    {
        static ConfigVarMap s_datas;
        return s_datas;
    }

    /**
     * @brief 配置项的RWMutex
     */
    static RWMutexType& GetMutex() {
        static RWMutexType s_mutex;
        return s_mutex;
    }
};



} // namespace sylar

