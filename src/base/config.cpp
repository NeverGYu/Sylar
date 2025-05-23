#include "config.h"

namespace sylar
{

ConfigVarBase::ptr Config::LookupBase(const std::string& name)
{
    auto it = GetDatas().find(name);
    if (it != GetDatas().end())
    {
        return it->second;
    }
    return nullptr;  
}

/**
 *  Yaml字符串 : "A.B", 10 
 *          |
 *          |
 *          |
 *      A:
 *        B: 10
 */

static void ListAllMember(const std::string& prefix,
                          const YAML::Node& node,
                          std::list<std::pair<std::string, const YAML::Node>>& output)
{
    if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") 
                != std::string::npos)
    {
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Config invaild name : "<< prefix << " : " << node;
        return;
    }
    output.push_back(std::make_pair(prefix, node));
    if (node.IsMap())
    {
        for (auto it = node.begin(); it != node.end(); ++it)
        {
            ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
        }
        
    }
    
}

void Config::LoadFromYaml(const YAML::Node& root)
{
    std::list<std::pair<std::string, const YAML::Node>> all_nodes;
    ListAllMember("",root,all_nodes);
   
    for (auto& i : all_nodes)
    {
        std::string key = i.first;
        if (key.empty())
        {
            continue;
        }
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigVarBase::ptr var = LookupBase(key);
        if (var)
        {
            if (i.second.IsScalar())
            {
                var->fromString(i.second.Scalar());
            }
            else
            {
                std::stringstream ss;
                ss << i.second;
                var->fromString(ss.str());
            }
        }
        
    }
    
}

}