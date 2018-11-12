#pragma once

#include <stdlib.h>
#include <pthread.h>
#include <boost/noncopyable.hpp>

namespace shs 
{

template <typename T>
class Singleton : boost::noncopyable
{
public:
    static T* instance()
    {
        pthread_once(&ponce_, &Singleton::Init);

        return value_;
    }

private:
    Singleton();
    ~Singleton();

    static void Init()
    {
        value_ = new T();
        ::atexit(Destroy);
    }

    static void Destroy()
    {
        delete value_;
    }

private:
    static pthread_once_t ponce_;
    static T* value_;
};

template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template<typename T>
T* Singleton<T>::value_ = NULL;

} // namespace shs
