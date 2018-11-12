#include "output.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/timeb.h>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

namespace shs 
{

using namespace std;

Output output;

Output::Output()
    : level_(-1)
    , f_(NULL)
{
}

Output::~Output()
{
}

void Output::SetOutputFunction(OutputFunc f) 
{
    f_ = f; 
}

void Output::SetLevel(int level)
{
    level_ = level;
}

void Output::PrintFormat(const char* category, int level, 
    const char *fmt, ...)
{
    if (level_ > level)
    {
        return;
    }

    if (f_ == NULL)
    {
        return;
    }

    int n, size = 1024;
    char *buffer = NULL;
    char *new_buffer = NULL;
    va_list ap;

    if (NULL == (buffer = (char*)malloc(size)))
    {
        return;
    }

    while (1) 
    {
        va_start(ap, fmt);
        n = vsnprintf(buffer, size, fmt, ap);
        va_end(ap);
        if (n > -1 && n < size)
        {
            break;
        }

        if (n > -1)
        {
            size = n + 1;
        }
        else
        {
            size *= 2;
        }

        if (NULL == (new_buffer = (char*)realloc(buffer, size)))
        {
            free(buffer);

            return;
        }
        else
        {
            buffer = new_buffer;
        }
    }

    f_(category, level, buffer);
    free(buffer);
}

} // namespace shs 
