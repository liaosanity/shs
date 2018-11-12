#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include "comm/timer.h"
#include "downstream/err_code.h"
#include "downstream/server.h"
#include "downstream/request.h"
#include "downstream/response.h"
#include "http_invoke_params.h"

NAME_SPACE_BS

using namespace std;
using namespace boost;
using namespace shs;
using namespace shs::downstream;

class Context
{
public:
    Context(); 
    ~Context();

    void OutputInfoLog();

public:
    int req_num_;
    int rsp_num_;
    int retry_;
    int body_sz_;

    double tm_enqueue_;
    double tm_inqueue_;
    string client_ip_;

    string res_;

    QTimerFactory timers_;
    map<string, int> err_code_;
    map<string, double> test_timer_;

    string sid_;
    string uid_;
    boost::shared_ptr<Request> request_;
    shs::InvokeCompleteHandler complete_handler_;
};

typedef boost::shared_ptr<Context> ContextPtr;

NAME_SPACE_ES

#endif


