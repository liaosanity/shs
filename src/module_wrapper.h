#pragma once

#include <set>
#include <string>
#include <boost/shared_ptr.hpp>

#include "types.h"

namespace shs 
{

class Module;
typedef Module* (*CreateModuleFn)();
typedef void (*DestroyModuleFn)(Module*);

class ModuleWrapper 
{
public:
    ModuleWrapper();
    ~ModuleWrapper();

    bool InitInMaster(const std::string& name, const std::string& path, 
        const std::string& conf);
    bool InitInMaster();
    bool InitInWorker(int32_t connection_n);

    void Invoke(const std::string& method_name,
        const std::map<std::string, std::string>& request_params,
        const InvokeCompleteHandler& complete_handler,
        boost::shared_ptr<InvokeParams> invoke_params);

    const std::string& name() const;
    std::set<std::string> GetAllMethods() const;
    bool IsValidMethod(const std::string& method) const;

    void Stop();

private:
    boost::shared_ptr<Module> Load(const std::string& path);
    boost::shared_ptr<Module> module_;
    void* dh_;
    CreateModuleFn create_module_;
    DestroyModuleFn destroy_module_;
};

} // namespace shs
