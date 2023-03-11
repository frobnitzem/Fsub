#include <iostream>
#include "ast.hpp"

void print_indent(int n) {
    static const char spaces[32] = "                               ";
    n = n < 31 ? n : 31;
    std::cout << "\n" << spaces+31-n;
}

void print_group(AstP g, int indent) {
    if(g->t == Type::top || g->t == Type::Top) {
        return;
    }
    print_indent(indent);
    std::cout << g->name;
    if(g->t == Type::Group) {
        std::cout << " =: ";
    } else {
        std::cout << " =  ";
    }
    print_ast(g->child[0], indent);
    print_group(g->child[1], indent);
}

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
        print_ast(a->child[0], indent);
        std::cout << ") ->";
        print_ast(a->child[1], indent);
        break;
    case Type::ForAll:  // bounded quantification, All(X<:A) B
        std::cout << "All(" << a->name << "<: ";
        print_ast(a->child[0], indent);
        std::cout << ") -> ";
        print_ast(a->child[1], indent);
        break;
    case Type::fn:      // functions, fn(x:A) b
        std::cout << "fn(" << a->name << ":";
        print_ast(a->child[0], indent);
        std::cout << ") -> ";
        print_ast(a->child[1], indent);
        break;
    case Type::fnT:     // polymorphic function, fn(X<:A) b
        std::cout << "fn(" << a->name << "<:";
        print_ast(a->child[0], indent);
        std::cout << ") -> ";
        print_ast(a->child[1], indent);
        break;
    case Type::Group:  // grouping, {A}
    case Type::group:   // grouping, {a}
        std::cout << "{";
        print_group(a, indent+4);
        print_indent(indent);
        std::cout << "}";
        print_indent(indent);
        break;
    case Type::top:     // member of Top
        std::cout << "top";
        break;
    case Type::app:     // application, b(a)
        if(a->child[0]->t == Type::fn) {
            std::cout << "let " << a->name << ":(";
            print_ast(a->child[0]->child[0], indent+2);
            std::cout << ") = ";
            print_ast(a->child[1], indent+2);
            print_indent(indent); std::cout << "  in ";
            print_ast(a->child[0]->child[1], indent+4);
        } else {
            std::cout << "(";
            print_ast(a->child[0], indent);
            std::cout << ") ";
            print_ast(a->child[1], indent);
        }
        break;
    case Type::appT:    // type application, b(:A)
         if(a->child[0]->t == Type::fnT) {
            std::cout << "Let " << a->name << "<:(";
            print_ast(a->child[0]->child[0], indent+2);
            std::cout << ") =";
            print_ast(a->child[1], indent+2);
            print_indent(indent); std::cout << "  in ";
            print_ast(a->child[0]->child[1], indent+4);
        } else {
            std::cout << "(";
            print_ast(a->child[0], indent);
            std::cout << "): ";
            print_ast(a->child[1], indent);
        }
        break;
    case Type::error:
        std::cout << "Error(" << a->name << ")";
        break;
    }
}
