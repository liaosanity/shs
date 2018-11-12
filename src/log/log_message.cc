#include "log_message.h"
#include "output.h"
#include "comm/logging.h"

namespace shs 
{

LogMessage::LogMessage(int level)
    : level_(level)
{
}

LogMessage::~LogMessage()
{
    stream() << '\0';
    const auto& buf(stream().buffer());

    output.PrintFormat("SHS", level_, "%s", buf.data());

    if (level_ == FATAL)
    {
        abort();
    }
}

} // namespace shs
