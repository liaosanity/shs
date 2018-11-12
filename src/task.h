#pragma once

#include <map>
#include <string>
#include <boost/shared_ptr.hpp>

#include "types.h"

namespace shs 
{

class InvokeTimer;
class InvokeParams;

class Task 
{
public:
    Task(boost::shared_ptr<InvokeHandler> method,
        const std::map<std::string, std::string>& request_params,
        const InvokeCompleteHandler& complete_handler,
        boost::shared_ptr<InvokeParams> invoke_params);

    void SetTaskSize(size_t size);

    void Run() const;
    bool IsExpired() const;

private:
    boost::shared_ptr<InvokeHandler> method_;
    std::map<std::string, std::string> request_params_;
    InvokeCompleteHandler complete_handler_;
    boost::shared_ptr<InvokeParams> invoke_params_;
};

} // namespace shs
