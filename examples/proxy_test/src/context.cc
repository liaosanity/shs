#include "context.h"
#include "comm/logging.h"
#include <boost/format.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

LOG_NAME("ProxyTest");

NAME_SPACE_BS

Context::Context()
    : req_num_(0)
    , rsp_num_(0) 
    , retry_(0)
    , body_sz_(0)
{
}

Context::~Context()
{
}

void Context::OutputInfoLog()
{
    stringstream test_timer;
    map<string, double>::iterator it_timer = test_timer_.begin();
    for (int i = 0; it_timer != test_timer_.end(); ++it_timer, i++) 
    {
        test_timer << it_timer->first << "_t=" << it_timer->second;
        if (i < test_timer_.size() - 1) 
        {
            test_timer << "\t";
        }
    }

    stringstream test_ec;
    map<string, int>::iterator it_ec = err_code_.begin();
    for (int i = 0; it_ec != err_code_.end(); ++it_ec, i++) 
    {
        test_ec << it_ec->first << "_ec=" << it_ec->second;
        if (i < err_code_.size() - 1) 
        {
            test_ec << "\t";
        }
    }

    stringstream log;
    log << "\tcall_ip=" << client_ip_
	<< "\t" << test_timer.str()
	<< "\t" << test_ec.str()
	<< "\tall_t=" << timers_.Timer("all")->Elapsed() * 1000
        << "\tar_que_t=" << tm_enqueue_ 
        << "\tin_que_t=" << tm_inqueue_
	<< "\tuid=" << uid_
	<< "\tsid=" << sid_
	<< "\tbody_sz=" << body_sz_
	<< "\tres=" << res_;

    LOG(INFO) << log.str();
}

NAME_SPACE_ES

