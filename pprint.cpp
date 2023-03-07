#include <iostream>
#include "ast.hpp"

void print_ast(AstP a, int indent) {
    switch(a->t) {
    case Type::Var:    // type variables, X
        std::cout << ":" << a->n;
        break;
    case Type::var:     // variables, x
        std::cout << a->n;
        break;
    case Type::Top:    // largest type
        std::cout << "Top";
        break;
    case Type::Fn:     // function spaces, A->B
        std::cout << "(";
        print_ast(a->child[0]);
        std::cout << ")->";
        print_ast(a->child[1]);
        break;
    case Type::ForAll:  // bounded quantification, All(X<:A) B
        std::cout << "All(" << a->name << "<:";
        print_ast(a->child[0]);
        std::cout << ") ";
        print_ast(a->child[1]);
        break;
    case Type::Group:  // grouping, {A}
        std::cout << "Group";
        break;
    case Type::top:     // member of Top
        std::cout << "top";
        break;
    case Type::fn:      // functions, fn(x:A) b
        std::cout << "fn(" << a->name << ":";
        print_ast(a->child[0]);
        std::cout << ") ";
        print_ast(a->child[1]);
        break;
    case Type::app:     // application, b(a)
        print_ast(a->child[0]);
        std::cout << "(";
        print_ast(a->child[1]);
        std::cout << ")";
        break;
    case Type::fnT:     // polymorphic function, fn(X<:A) b
        std::cout << "fn(" << a->name << "<:";
        print_ast(a->child[0]);
        std::cout << ") ";
        print_ast(a->child[1]);
        break;
    case Type::appT:    // type application, b(:A)
        print_ast(a->child[0]);
        std::cout << "(:";
        print_ast(a->child[1]);
        std::cout << ")";
        break;
    case Type::group:   // grouping, {a}
        std::cout << "group";
        break;
    case Type::error:
        std::cout << "Error(" << a->name << ")";
        break;
    }
}
