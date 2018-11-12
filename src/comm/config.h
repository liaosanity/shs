#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <stdint.h>
#include <map>
#include <vector>
#include <iostream>
#include <boost/format.hpp>
#include <tr1/functional>
#include <cJSON.h>

namespace shs_conf 
{

class OptionValue 
{
public:
    OptionValue(double value);
    OptionValue(const std::string& value);
    OptionValue(const std::vector<std::string>& value);
    OptionValue(const std::map<std::string, std::string>& value);

    std::string GetStr() const;
    std::string GetStr(const std::string& index) const;

    size_t GetSize() const;
    int Type();
    const std::map<std::string, std::string>& Value();
    bool HasKey(const std::string& key) const;
    bool HasValue(const std::string& value) const;

private:
    int type_;
    std::map<std::string, std::string> value_;
};

class ConfigOption
{
public:
    ConfigOption();
    ~ConfigOption();

    bool Init(cJSON* joption);
    void PrintToString(std::string* output, 
        const::std::string& indent = "") const;
    bool HasOption(const std::string& key) const;
    bool HasOption(const std::string& key, const std::string& index) const;
    bool HasOptionValue(const std::string& key, const std::string& value) const;
    size_t GetSize(const std::string& key) const;
    std::map<std::string, boost::shared_ptr<OptionValue> > option() const;

    std::string GetStr(const std::string& key) const;
    template<typename TIndex>
    std::string GetStr(const std::string& key, TIndex index, 
        const std::string& default_value = "") const
    {
        std::string str_index = boost::str(boost::format("%1%") % index);
        std::map<std::string, boost::shared_ptr<OptionValue> >::const_iterator iter = 
            option_.find(key);
        if (iter == option_.end())
        {
            return default_value;
        }

        return iter->second->GetStr(str_index);;
    }

    double GetNum(const std::string& key) const;
    template<typename TIndex>
    double GetNum(const std::string& key, TIndex index, 
        double default_value = 0) const
    {
        std::string str_value = boost::str(boost::format("%1%") % default_value);

        return atof(GetStr(key, index, str_value).c_str());
    }

    int GetInt(const std::string& key) const;
    template<typename TIndex>
    int GetInt(const std::string& key, TIndex index, int default_value = 0) const
    {
        return (int)GetNum(key, index, default_value);
    }       

protected:
    std::map<std::string, boost::shared_ptr<OptionValue> > option_;
};

class ConfigSection
{
public:
    ConfigSection();
    ~ConfigSection();

    bool Init(cJSON* jsection);
    void PrintToString(std::string* output, 
        const::std::string& indent = "") const;

    size_t OptionSize();
    boost::shared_ptr<ConfigOption> Option(uint32_t index = 0) const;

private:
    std::vector<boost::shared_ptr<ConfigOption> > options_;
};

class Config
{
public:
    Config();
    ~Config();

    bool Init(const std::string& conf_path);
    void Print() const;
    void PrintToString(std::string* output) const;

    boost::shared_ptr<ConfigSection> Section(const std::string& name) const;
    boost::shared_ptr<ConfigOption> section(const std::string& section_name, 
        uint32_t index = 0) const;

    std::vector<std::string> sections() const;
    bool HasSection(const std::string& section_name) const;

    const cJSON* JsonObject(const std::string& path = "/") const;

private:
    std::map<std::string, boost::shared_ptr<ConfigSection> > sections_;
    cJSON* jconfig_;
};

} // namespace shs_conf

#endif
