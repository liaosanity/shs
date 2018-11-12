#include "service.h"
#include "framework.h"

namespace shs 
{

Service::Service()
    : status_(ServiceStatus::RUNNING)
{
    framework_ = NULL;
    event_base_ = NULL;
    conn_pool_ = NULL;
    event_timer_ = NULL;
}

void Service::Init(Framework* framework)
{
    framework_ = framework;
}

} // namespace shs
