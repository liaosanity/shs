#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <tr1/functional>
#include <iomanip>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "config.h"
#include "util.h"

namespace shs_conf 
{

using namespace std;
using namespace boost;
using namespace std::tr1::placeholders;

OptionValue::OptionValue(const string& str)
{
    type_ = cJSON_String;
    string index_zero = boost::str(boost::format("%1%") % 0);
    value_[index_zero] = str;
}

OptionValue::OptionValue(double value)
{
    type_ = cJSON_Number;
    string index_zero = boost::str(boost::format("%1%") % 0);
    value_[index_zero] = boost::str(boost::format("%1%") % value);
}

OptionValue::OptionValue(const vector<string>& value)
{
    type_ = cJSON_Array;

    for (size_t i = 0; i < value.size(); ++i)
    {
        string str_index = boost::str(boost::format("%1%") % i);
        value_[str_index] = value[i];
    }
}

OptionValue::OptionValue(const map<string, string>& value)
{
    type_ = cJSON_Object;
    value_ = value;
}

string OptionValue::GetStr() const
{
    return GetStr("0");
}

string OptionValue::GetStr(const string& index) const
{
    map<string, string>::const_iterator iter = value_.find(index);
    if (iter == value_.end())
    {
        return "";
    }

    return iter->second;
}

size_t OptionValue::GetSize() const
{
    return value_.size();
}

int OptionValue::Type()
{
    return type_;
}

const map<string, string>& OptionValue::Value()
{
    return value_;
}

bool OptionValue::HasKey(const string& key) const
{
    map<string, string>::const_iterator iter = value_.find(key);
    if (iter == value_.end())
    {
        return false;
    }

    return true;
}

bool OptionValue::HasValue(const string& value) const
{
    map<string, string>::const_iterator iter = value_.begin();
    for (; iter != value_.end(); ++iter)
    {
        if (iter->second == value)
        {
            return true;
        }
    }

    return false;
}

ConfigOption::ConfigOption()
{
}

ConfigOption::~ConfigOption()
{
}

bool ConfigOption::Init(cJSON* joption)
{
    if (NULL == joption)
    {
        return false;
    }

    cJSON* jnode = joption->child;
    string index_zero = boost::str(boost::format("%1%") % 0);

    while (jnode != NULL)
    {
        cJSON* jcurr = jnode;
        jnode = jnode->next;

        if (jcurr->type == cJSON_String)
        {
            if (jcurr->valuestring != NULL)
            {
                boost::shared_ptr<OptionValue> value(
                    new OptionValue(jcurr->valuestring));
                option_[jcurr->string] = value;
            }
        }
        else if (jcurr->type == cJSON_Number)
        {
            boost::shared_ptr<OptionValue> value(
                new OptionValue(jcurr->valuedouble));
            option_[jcurr->string] = value;
        }
        else if (jcurr->type == cJSON_True || jcurr->type == cJSON_False)
        {
            boost::shared_ptr<OptionValue> value(
                new OptionValue((double)jcurr->valueint));
            option_[jcurr->string] = value;
        }
        else if (jcurr->type == cJSON_Array)
        {
            int size = cJSON_GetArraySize(jcurr);
            vector<string> vec_value;
            for (int i = 0; i < size; ++i)
            {
                cJSON* jitem = NULL;
                jitem = cJSON_GetArrayItem(jcurr, i);
                if (NULL == jitem)
                {
                    continue;
                }

                if (jitem->type == cJSON_String)
                {
                    if (jitem->valuestring != NULL)
                    {
                        vec_value.push_back(jitem->valuestring);
                    }
                }
                else if (jitem->type == cJSON_Number)
                {
                    string str_value = boost::str(boost::format("%1%") % jitem->valuedouble); 
                    vec_value.push_back(str_value);
                }
                else if (jitem->type == cJSON_True || jitem->type == cJSON_False)
                {
                    string str_value = boost::str(boost::format("%1%") % jitem->valueint); 
                    vec_value.push_back(str_value);
                }
            }

            if (vec_value.size() != 0)
            {
                boost::shared_ptr<OptionValue> value(new OptionValue(vec_value));
                option_[jcurr->string] = value;
            }
        }
        else if (jcurr->type == cJSON_Object)
        {
            map<string, string> map_value;
            cJSON* jitem = jcurr->child;
            while (jitem != NULL)
            {
                cJSON* jcurr_item = jitem;
                jitem = jitem->next;

                if (jcurr_item->type == cJSON_String && jcurr_item->valuestring != NULL)
                {
                    map_value.insert(make_pair(jcurr_item->string, jcurr_item->valuestring));
                }
                else if (jcurr_item->type == cJSON_Number)
                {
                    string str_value = boost::str(boost::format("%1%") % jcurr_item->valuedouble); 
                    map_value.insert(make_pair(jcurr_item->string, str_value));
                }
                else if (jcurr_item->type == cJSON_True || jcurr_item->type == cJSON_False)
                {
                    string str_value = boost::str(boost::format("%1%") % jcurr_item->valueint); 
                    map_value.insert(make_pair(jcurr_item->string, str_value));
                }
            }

            if (map_value.size() != 0)
            {
                boost::shared_ptr<OptionValue> value(new OptionValue(map_value));
                option_[jcurr->string] = value;
            }
        }
    }

    return true;
}

bool ConfigOption::HasOption(const string& key) const
{
    map<string, boost::shared_ptr<OptionValue> >::const_iterator iter = option_.find(key);
    if (iter == option_.end())
    {
        return false;
    }

    return true;
}

bool ConfigOption::HasOption(const string& key, const string& index) const
{
    map<string, boost::shared_ptr<OptionValue> >::const_iterator iter = option_.find(key);
    if (iter == option_.end())
    {
        return false;
    }

    if (!iter->second->HasKey(index))
    {
        return false;
    }

    return true;
}

bool ConfigOption::HasOptionValue(const string& key, const string& value) const
{
    map<string, boost::shared_ptr<OptionValue> >::const_iterator iter = option_.find(key);
    if (iter == option_.end())
    {
        return false;
    }

    if (!iter->second->HasValue(value))
    {
        return false;
    }

    return true;
}

size_t ConfigOption::GetSize(const string& key) const
{
    map<string, boost::shared_ptr<OptionValue> >::const_iterator iter = option_.find(key);
    if (iter != option_.end())
    {
        return iter->second->GetSize();
    }

    return 0;
}

map<string, boost::shared_ptr<OptionValue> > ConfigOption::option() const
{
    return option_;
}

string ConfigOption::GetStr(const string& key) const
{
    return GetStr(key, 0, "");
}

double ConfigOption::GetNum(const string& key) const
{
    return GetNum(key, 0, 0);
}

int ConfigOption::GetInt(const string& key) const
{
    return GetInt(key, 0, 0);
}

void ConfigOption::PrintToString(string* output, const string& indent) const
{
    map<string, boost::shared_ptr<OptionValue> >::const_iterator iter = option_.begin();
    for (; iter != option_.end(); ++iter)
    {
        if (iter != option_.begin())
        {
            output->append(",\n");
        }

        string str_value;
        boost::shared_ptr<OptionValue> value = iter->second;
        if (value->Type() == cJSON_String)
        {
            str_value = boost::str(boost::format("\"%1%\"") % value->GetStr());
        }
        else if (value->Type() == cJSON_Number)
        {
            str_value = value->GetStr();
        }
        else if (value->Type() == cJSON_Array)
        {
            str_value.append("\n");
            str_value.append(indent  + "[\n");
            for (size_t i = 0; i < value->GetSize(); ++i)
            {
                if (i != 0)
                {
                    str_value.append(",\n");
                }

                str_value.append(indent + "    \"" + GetStr(iter->first, i) + "\"");
            }
            str_value.append("\n" + indent  + "]");
        }
        else if (value->Type() == cJSON_Object)
        {
            const map<string, string>& values = value->Value();
            map<string, string>::const_iterator iter = values.begin();

            str_value.append("\n");
            str_value.append(indent  + "{\n");
            for (; iter != values.end(); ++iter)
            {
                if (iter != values.begin())
                {
                    str_value.append(",\n");
                }
                str_value.append(indent + "    \"" + iter->first + "\" : \"" + iter->second + "\"");
            }
            str_value.append("\n" + indent  + "}");
        }

        output->append(boost::str(boost::format("%1%\"%2%\" : %3%") % indent % iter->first % str_value));
    }
}

ConfigSection::ConfigSection()
{
}

ConfigSection::~ConfigSection()
{
}

bool ConfigSection::Init(cJSON* jsection)
{
    if (NULL == jsection)
    {
        return false;
    }

    if (jsection->type == cJSON_Object)
    {
        boost::shared_ptr<ConfigOption> option(new ConfigOption);
        if (!option->Init(jsection))
        {
            return false;
        }

        options_.push_back(option);
    }
    else if (jsection->type == cJSON_Array)
    {
        int size = cJSON_GetArraySize(jsection);

        for (int i = 0; i < size; ++i)
        {
            cJSON* jitem = NULL;
            jitem = cJSON_GetArrayItem(jsection, i);

            if (jitem == NULL || jitem->type != cJSON_Object)
            {
                continue;
            }

            boost::shared_ptr<ConfigOption> option(new ConfigOption);
            if (!option->Init(jitem))
            {
                continue;
            }

            options_.push_back(option);
        }
    }

    if (options_.size() == 0)
    {
        return false;
    }

    return true;
}

size_t ConfigSection::OptionSize()
{
    return options_.size();
}

boost::shared_ptr<ConfigOption> ConfigSection::Option(uint32_t index) const
{
    if (index < options_.size())
    {
        return options_[index];
    }

    return boost::shared_ptr<ConfigOption>(new ConfigOption);
}

void ConfigSection::PrintToString(string* output, const string& indent) const
{
    for (size_t i = 0; i < options_.size(); ++i)
    {
        if (i != 0)
        {
            output->append(",\n");
        }

        if (options_.size() > 1)
        {
            output->append(indent + "{\n");
            string output_section;
            options_[i]->PrintToString(&output_section, indent + "    ");
            output->append(output_section);
            output->append("\n" + indent + "}");
        }
        else
        {
            string output_section;
            options_[i]->PrintToString(&output_section, indent + "    ");
            output->append(output_section);
        }
    }
}

Config::Config()
    : jconfig_(NULL)
{
}

Config::~Config()
{
    if (jconfig_ != NULL)
    {
        cJSON_Delete(jconfig_);
    }
}

bool Config::Init(const string& conf_path)
{
    bool rtn = false;
    cJSON* jconfig = NULL;
    cJSON* jnode = NULL;
    char* file_content = NULL;

    if (get_file_content(conf_path.c_str(), &file_content) <= 0)
    {
        return false;
    }

    jconfig = cJSON_Parse(file_content);
    if (jconfig == NULL || jconfig->type != cJSON_Object)
    {
        goto Return;
    }

    jnode = jconfig->child;
    while (jnode != NULL)
    {
        cJSON* jcurr = jnode;
        jnode = jnode->next;

        if (jcurr->type == cJSON_Object || jcurr->type == cJSON_Array)
        {
            boost::shared_ptr<ConfigSection> section(new ConfigSection);
            if (!section->Init(jcurr))
            {
                continue;
            }

            sections_[jcurr->string] = section;
        }
    }

    jconfig_ = jconfig;
    rtn = true;

Return:
    free(file_content);

    return rtn;
}

boost::shared_ptr<ConfigSection> Config::Section(const string& name) const
{
    map<string, boost::shared_ptr<ConfigSection> >::const_iterator iter = sections_.find(name);
    if (iter == sections_.end())
    {
        return boost::shared_ptr<ConfigSection>(new ConfigSection);
    }

    return iter->second;
}

boost::shared_ptr<ConfigOption> Config::section(const string& section_name, uint32_t index) const
{
    return Section(section_name)->Option(index);
}

void Config::PrintToString(string* output) const
{
    output->append("{\n");
    map<string, boost::shared_ptr<ConfigSection> >::const_iterator iter_section;
    for (iter_section = sections_.begin(); iter_section != sections_.end(); ++iter_section)
    {
        if (iter_section != sections_.begin())
        {
            output->append(",\n");
        }

        output->append(boost::str(boost::format("    \"%s\" : \n") % iter_section->first));
        if (iter_section->second->OptionSize() == 1)
        {
            output->append("    {\n");
        }
        else
        {
            output->append("    [\n");
        }

        string output_section;
        iter_section->second->PrintToString(&output_section, "        ");
        output->append(output_section);

        if (iter_section->second->OptionSize() == 1)
        {
            output->append("\n    }");
        }
        else
        {
            output->append("\n    ]");
        }
    }

    output->append("\n}\n");
}

void Config::Print() const
{
    return;
}

bool Config::HasSection(const string& section_name) const
{
    if (sections_.find(section_name) != sections_.end())
    {
        return true;
    }

    return false;
}

vector<string> Config::sections() const
{
    vector<string> keys;
    map<string, boost::shared_ptr<ConfigSection> >::const_iterator iter = sections_.begin();
    for (; iter != sections_.end(); ++iter)
    {
        keys.push_back(iter->first);
    }

    return keys;
}

const cJSON* Config::JsonObject(const string& path) const
{
    if (!starts_with(path, "/"))
    {
        return NULL;
    }

    vector<string> vs;
    boost::split(vs, path, boost::is_any_of("/"), boost::token_compress_on);
    cJSON* jnode = jconfig_;
    for (size_t i = 1; i < vs.size(); ++i)
    {
        jnode = jnode->child;
        while (jnode != NULL)
        {
            if (jnode->string != NULL && strlen(jnode->string) != 0 
                && strcmp(vs[i].c_str(), jnode->string) == 0)
            {
                break;
            }
            jnode = jnode->next;
        }

        if (NULL == jnode)
        {
            return NULL;
        }
    }

    return jnode;
}

} // namespace shs_conf
