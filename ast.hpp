#pragma once

#include <string>
#include <memory>

// We use pointers to struct Bind to implement locally nameless
// Ast-s, which are short-lived intermediate data structures.
struct Bind;

struct Ast;
using AstP = std::shared_ptr<Ast>;

#include "error.hpp"

enum class Type {
    Var=1,   // type variables, X
    Top,     // largest type
    Fn,      // function spaces, A->B
    ForAll,  // bounded quantification, All(X<:A) B
    Group,   // grouping, {A}
    var,     // variables, x
    top,     // member of Top
    fn,      // functions, fn(x:A) b
    app,     // application, a(b) // note: reversed from FSubImpl
    fnT,     // polymorphic function, fn(X<:A)b
    appT,    // type application, a(:B) // note: reversed from FSubImpl
    group,   // grouping, {a}
    error
};


struct Ast {
    Type t;
    bool isPtr = false; // whether ref() is active [true] or n [false]
    std::string name; // informational only - for named variables
    intptr_t n = -1;  // de-Bruijn index for variables
    Bind *ref() {
        return reinterpret_cast<Bind *>(n);
    }

    AstP child[2];
    Ast(Type _t)                : t(_t), child{nullptr,nullptr} {}
    Ast(Type _t, AstP c0)         : t(_t), child{c0,nullptr} {}
    Ast(Type _t, AstP c0, AstP c1) : t(_t), child{c0,c1} {}

    Ast(Type _t, const int _n)
                          : t(_t), n(_n), child{nullptr,nullptr} {}
    Ast(Type _t, const std::string& _name)
                          : t(_t), name(_name), child{nullptr,nullptr} {}
    Ast(Type _t, const std::string& _name,
            AstP c0)         : t(_t), name(_name), child{c0,nullptr} {}
    Ast(Type _t, const std::string& _name,
            AstP c0, AstP c1) : t(_t), name(_name), child{c0,c1} {}
};

inline AstP Var(const std::string& name) {
    return std::make_shared<Ast>(Type::Var, name);
}
inline AstP var(const std::string& name) {
    return std::make_shared<Ast>(Type::var, name);
}
inline AstP Var(int n) {
    return std::make_shared<Ast>(Type::Var, n);
}
inline AstP Top() {
    return std::make_shared<Ast>(Type::Top);
}
inline AstP top() {
    return std::make_shared<Ast>(Type::top);
}
inline AstP Fn(AstP A, AstP B) {
    return std::make_shared<Ast>(Type::Fn, A, B);
}
inline AstP ForAll(const std::string& name, AstP A, AstP B) {
    return std::make_shared<Ast>(Type::ForAll, name, A, B);
}
inline AstP fn(const std::string& name, AstP A, AstP b) {
    return std::make_shared<Ast>(Type::fn, name, A, b);
}
inline AstP fnT(const std::string& name, AstP A, AstP b) {
    return std::make_shared<Ast>(Type::fnT, name, A, b);
}
inline AstP var(int n) {
    return std::make_shared<Ast>(Type::var, n);
}
inline AstP app(AstP a, AstP b) {
    return std::make_shared<Ast>(Type::app, a, b);
}
inline AstP appT(AstP a, AstP B) {
    return std::make_shared<Ast>(Type::appT, a, B);
}
inline AstP group(const std::string& name, AstP a, AstP b) {
    return std::make_shared<Ast>(Type::group, name, a, b);
}
inline AstP Group(const std::string& name, AstP a, AstP b) {
    return std::make_shared<Ast>(Type::Group, name, a, b);
}
inline AstP error(const std::string& name) {
    return std::make_shared<Ast>(Type::error, name);
}
/*  Var=1,   // type variables, X
    Top,     // largest type
    Fn,      // function spaces, A->B
    ForAll,  // bounded quantification, All(X<:A) B
    Group,   // grouping, {A}
    var,     // variables, x
    top,     // member of Top
    fn,      // functions, fn(x:A) b
    app,     // application, b(a)
    fnT,     // polymorphic function, fn(X<:A)b
    appT,    // type application, b(:A)
    group    // grouping, {a}
*/

// Does this term represent a binding construct?
static constexpr bool isBind(Type t) {
    return t == Type::Fn || t == Type::ForAll || t == Type::fn
           || t == Type::fnT;
}

// Does this term represent a type?
static constexpr bool isType(Type t) {
    return t == Type::Var || t == Type::Top || t == Type::Fn
              || t == Type::ForAll || t == Type::Group;
}

// Does this term represent a value?
static constexpr bool isValue(Type t) {
    return t == Type::Top || t == Type::top;
}

// Should the right-hand side value of this binder be a type?
static constexpr bool bindType(Type t) {
    return t == Type::fnT || t == Type::ForAll || t == Type::Fn
                || t == Type::Group;
}

struct NChild {
    static constexpr int n[] = {
        0,
        0,//Var,     // type variables, X
        0,//Top,     // largest type
        2,//Fn,      // function spaces, A->B
        2,//ForAll,  // bounded quantification, All(X<:A) B
        2,//Group,   // grouping, {A}
        0,//var,     // variables, x
        0,//top,     // member of Top
        2,//fn,      // functions, fn(x:A) b
        2,//app,     // application, b(a)
        2,//fnT,     // polymorphic function, fn(X<:A)b
        2,//appT,    // type application, b(:A)
        2 //group    // grouping, {a}
        };
};

static constexpr int getNChild(Type t) {
    return NChild::n[(int)t];
}

// pprint.cpp
void print_ast(AstP a, int indent=0);

// type.cpp
TracebackP subType(AstP A, AstP B);
