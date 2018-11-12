#pragma once

#include <string.h>
#include <string>
#include "log/log_buffer.h"

namespace shs 
{

class LogStream
{
public:
    typedef LogStream self;
    typedef LogBuffer<kSmallBuffer> Buffer;

    self& operator<<(bool v)
    {
        buffer_.Append(v ? "1" : "0", 1);

        return *this;
    }

    self& operator<<(char v)
    {
        buffer_.Append(&v, 1);

        return *this;
    }

    self& operator<<(short);
    self& operator<<(unsigned short);
    self& operator<<(int);
    self& operator<<(unsigned int);
    self& operator<<(long);
    self& operator<<(unsigned long);
    self& operator<<(long long);
    self& operator<<(unsigned long long);

    self& operator<<(const void *);

    self& operator<<(float v)
    {
        *this << static_cast<double>(v);

        return *this;
    }

    self& operator<<(double v);

    self& operator<<(const char* v)
    {
        if (v)
        {
            Append(v, strlen(v));
        }
        else
        {
            Append("null", 4);
        }

        return *this;
    }

    self& operator<<(const std::string& s)
    {
        Append(s.data(), s.size());

        return *this;
    }

    self& operator<<(std::string&& s)
    {
        Append(s.data(), s.size());

        return *this;
    }

    void set_ctr(int v)
    {
        ctr_ = v;
    }

    int ctr() const
    {
        return ctr_;
    }

    const Buffer& buffer() const
    {
        return buffer_;
    }

    void ResetBuffer()
    {
        buffer_.Reset();
    }

    void Append(const char* msg, size_t len)
    {
        buffer_.Append(msg, len);
    }

private:
    template<typename T>
    void FormatInteger(T);

    Buffer buffer_;
    int ctr_;
};

} // namespace shs
