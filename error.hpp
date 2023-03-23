#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <iostream>

// TODO: make a better error tree, e.g.
//
// Errors = Error TBPrint           // node
//        | Context TBPrint Errors  // stack context message
//        | Cons Error Errors       // list of errors in a context
//
// For now, we have a flat list of tracebacks, ErrorList(TracebackP).
struct Traceback;
typedef std::unique_ptr<Traceback> TracebackP;
typedef std::function<void(std::ostream& os)> TBPrint;

struct ErrorList {
    std::vector<TracebackP> errors;

    void append(TracebackP &&err) {
        if(!err) return;
        errors.emplace_back(std::move(err));
    }
    bool ok() {
        return errors.size() == 0;
    }
};

struct Traceback {
    int depth = 0;
    TBPrint what;
    TracebackP next;
    Traceback(const TBPrint &w)
        : what(w), next(nullptr) {}
    Traceback(const TBPrint &w, TracebackP &&next_)
        : what(w), next(std::move(next_)) {
        if(!next) {
            throw std::runtime_error("Invalid Traceback constructor.");
        }
        depth = next->depth+1;
    }
    Traceback(const std::string &_name) : next(nullptr) {
        std::string name = _name;
        what = [=](std::ostream& os) {
            os << name << std::endl;
        };
    }
};

inline std::ostream& operator <<(std::ostream& os, const Traceback& tb) {
    if(tb.next) {
        os << *tb.next;
    }
    os << tb.depth << ": ";
    tb.what(os);
    return os;
}

inline std::ostream& operator <<(std::ostream& os, const ErrorList& err) {
    constexpr int max_errors = 10;
    int n = err.errors.size();
    if(n == 0) return os;

    os << n << " errors:\n";
    for(int i=0; i<n; ++i) {
        if(i >= max_errors) {
            os << "Stopping at " << max_errors << " errors\n";
            break;
        }
        os << *(err.errors[i]);
        os << std::endl;
    }
    return os;
}

inline TracebackP mkError(const std::string &name) {
    return std::make_unique<Traceback>(name);
}

inline TracebackP mkTB(const TBPrint &what, TracebackP &&next) {
    if(!next) return next;
    return std::make_unique<Traceback>(what, std::move(next));
}
