#pragma once

#include <string>
#include <memory>

struct Traceback;
struct Traceback {
    std::string name;
    Traceback(const std::string &desc) : name(desc) {}
    //struct Traceback *parent;
};

typedef std::unique_ptr<Traceback> TracebackP;

inline TracebackP mkError(const std::string &desc) {
    return std::make_unique<Traceback>(desc);
}
