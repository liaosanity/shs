#pragma once

#include <string>
#include <tr1/functional>

namespace shs 
{

class Output 
{
typedef void(*OutputFunc)(const char* category, 
    int level, const char* message);

public:
    Output();
    ~Output();

    void SetOutputFunction(OutputFunc f);
    void SetLevel(int level);
    void PrintFormat(const char* category, int level, const char *fmt, ...);

private: 
    int level_;
    OutputFunc f_;
};

extern Output output;

} // namespace shs
