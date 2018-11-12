#include <sys/types.h>
#include <string.h>
#include <dlfcn.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <gflags/gflags.h>

#include "log/logging.h"

#include "config.h"
#include "module.h"
#include "module_wrapper.h"
#include "module_impl.h"

namespace shs 
{

using namespace std;
using namespace boost;

const char* kCreateModuleFnName = "CreateModuleInstance";
const char* kDestroyModuleFnName = "DestroyModuleInstance";

ModuleWrapper::ModuleWrapper()
    : dh_(NULL)
    , create_module_(NULL)
    , destroy_module_(NULL)
{
}

ModuleWrapper::~ModuleWrapper()
{
    if (module_) 
    {
        module_->Stop();
        module_.reset();
    }

    if (NULL != dh_) 
    {
        dlclose(dh_);
        create_module_ = NULL;
        destroy_module_ = NULL;
    }
}

bool ModuleWrapper::InitInMaster() 
{
    Module* module = NULL;

    module = create_module_();
    if (NULL == module) 
    {
        fprintf(stderr, "create_module_ failed\n");

        return false;
    }   

    boost::shared_ptr<Module> module_new = boost::shared_ptr<Module>(
        module, destroy_module_);

    if (!ModuleImpl::Get(module_new)->InitInMaster(
        module_->name(), module_->conf())) 
    {
        return false;
    }

    module_ = module_new;

    return true;
}

bool ModuleWrapper::InitInMaster(const string& name, 
    const string& path, const string& conf)
{
    module_ = Load(path);
    if (!module_) 
    {
        return false;
    }

    if (!ModuleImpl::Get(module_)->InitInMaster(name, conf)) 
    {
        return false;
    }

    return true;
}

bool ModuleWrapper::InitInWorker(int32_t connection_n)
{
    if (!ModuleImpl::Get(module_)->InitInWorker(connection_n)) 
    {
        SLOG(ERROR) << "ModuleWrapper::InitInWorker(" << name() << ") failed.";

        return false;
    }

    return true;
}

void ModuleWrapper::Invoke(const std::string& method_name,
    const std::map<std::string, std::string>& request_params,
    const InvokeCompleteHandler& complete_handler,
    boost::shared_ptr<InvokeParams> invoke_params) 
{
    ModuleImpl::Get(module_)->Invoke(method_name, request_params, 
        complete_handler, invoke_params);
}

const std::string& ModuleWrapper::name() const 
{
    return ModuleImpl::Get(module_)->name();
}

boost::shared_ptr<Module> ModuleWrapper::Load(const std::string& path)
{
    Module* module = NULL;
    void * dlhandle = NULL;
    CreateModuleFn create_module = NULL;
    DestroyModuleFn destroy_module = NULL;

    int flag = RTLD_LAZY;

    if (!FLAGS_dlopen_flag.empty()) 
    {
        vector<string> dl_flags;
        boost::split(dl_flags, FLAGS_dlopen_flag, boost::is_any_of("|"), 
            boost::token_compress_on);                                         
        for (size_t i = 0; i < dl_flags.size(); ++i) 
        {
            string dl_flag = dl_flags[i];
            boost::trim(dl_flag);
            if (dl_flag == "RTLD_LAZY") 
            {
                flag |= RTLD_LAZY;
            } 
            else if (dl_flag == "RTLD_NOW") 
            {
                flag |= RTLD_NOW;
            } 
            else if (dl_flag == "RTLD_GLOBAL") 
            {
                flag |= RTLD_GLOBAL;
            } 
            else if (dl_flag == "RTLD_LOCAL")
            {
                flag |= RTLD_LOCAL;
            } 
            else if (dl_flag == "RTLD_NODELETE") 
            {
                flag |= RTLD_NODELETE;
            } 
            else if (dl_flag == "RTLD_NOLOAD") 
            {
                flag |= RTLD_NOLOAD;
            } 
            else if (dl_flag == "RTLD_DEEPBIND") 
            {
                flag |= RTLD_DEEPBIND;
            }
        }
    }

    if (FLAGS_t)
    {
        flag = RTLD_NOW;
    }

    dlhandle = dlopen(path.c_str(), flag);
    if (!dlhandle) 
    {
        fprintf(stderr, "dlopen failed, err=%s\n", dlerror());

        goto failed;
    }   

    google::ReparseCommandLineNonHelpFlags();

    create_module = (CreateModuleFn)dlsym(dlhandle, kCreateModuleFnName);
    destroy_module = (DestroyModuleFn)dlsym(dlhandle, kDestroyModuleFnName);
    if (!create_module || !destroy_module) 
    {
        fprintf(stderr, "dlsym failed, err=%s\n", dlerror());

        goto failed;
    }   

    module = create_module();
    if (!module) 
    {
        fprintf(stderr, "create moudle failed!\n"); 

        goto failed;
    }   

    dh_ = dlhandle;
    create_module_ = create_module;
    destroy_module_ = destroy_module;

    return boost::shared_ptr<Module>(module, destroy_module);

failed:
    if (dlhandle) 
    {
        dlclose(dlhandle);
    }

    return boost::shared_ptr<Module>();
}

std::set<std::string> ModuleWrapper::GetAllMethods() const
{
    return ModuleImpl::Get(module_.get())->GetAllMethods();
}

bool ModuleWrapper::IsValidMethod(const std::string& method) const
{
    return ModuleImpl::Get(module_.get())->IsValidMethod(method);
}

void ModuleWrapper::Stop()
{
    ModuleImpl::Get(module_.get())->Stop();
}

} // namespace shs
