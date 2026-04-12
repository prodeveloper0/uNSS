#pragma once

#include <string>
#include <functional>
#include <sstream>
#include <iomanip>


template<typename T>
std::string toHex(const T value)
{
    std::stringstream ss;
    ss <<std::setfill('0') << std::setw(sizeof(value) * 2) << std::hex << value;
    return ss.str();
}


template<typename T>
T fromHex(const std::string& hex)
{
    std::stringstream ss;
    ss << std::hex << hex;
    T value;
    ss >> value;
    return value;
}


template<typename T>
std::string padding(const T value, const size_t length)
{
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(length) << value;
    return ss.str();
}


bool inline startWith(const std::string& str, const std::string& prefix) 
{
    if (str.size() < prefix.size()) 
    {
        return false;
    }

    return str.compare(0, prefix.size(), prefix) == 0;
}


bool inline endWith(const std::string& str, const std::string& suffix) 
{
    if (str.size() < suffix.size()) 
    {
        return false;
    }

    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

template<typename T>
void freeSafely(T** ptr)
{
    if (ptr)
    {
        free(*ptr);
        *ptr = NULL;
    }
}


class Defer
{
private:
    std::function<void()> onExit;

public:
    [[nodiscard]] 
    Defer(std::function<void()> onEnter, std::function<void()> onExit)
    : onExit(onExit)
    {
        onEnter();
    }

    [[nodiscard]] 
    Defer(std::function<void()> onExit)
    : onExit(onExit)
    {
    }

    virtual ~Defer()
    {
        onExit();
    }

    Defer(const Defer&) = delete;
    Defer& operator=(const Defer&) = delete;
};
