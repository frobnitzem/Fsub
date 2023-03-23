#include <iostream>
#include "ast.hpp"

void print_indent(std::ostream &os, int n) {
    static const char spaces[32] = "                               ";
    n = n < 31 ? n : 31;
    os << "\n" << spaces+31-n;
}

void print_group(std::ostream &os, AstP g, int indent) {
    if(g->t == Type::top || g->t == Type::Top) {
        return;
    }
    print_indent(os, indent);
    os << g->name;
    if(g->t == Type::Group) {
        os << " =: ";
    } else {
        os << " =  ";
    }
    print_ast(os, g->child[0], indent);
    print_group(os, g->child[1], indent);
}

void print_ast(std::ostream &os, AstP a, int indent) {
    // TODO: print with errors, underlining if a->err is present.
    switch(a->t) {
    case Type::Var:    // type variables, X
        os << ":" << a->n;
        break;
    case Type::var:     // variables, x
        os << a->n;
        break;
    case Type::Top:    // largest type
        os << "Top";
        break;
    case Type::Fn:     // function spaces, A->B
        os << "(";
        print_ast(os, a->child[0], indent);
        os << ") ->";
        print_ast(os, a->child[1], indent);
        break;
    case Type::ForAll:  // bounded quantification, All(X<:A) B
        os << "All(" << a->name << "<: ";
        print_ast(os, a->child[0], indent);
        os << ") -> ";
        print_ast(os, a->child[1], indent);
        break;
    case Type::fn:      // functions, fn(x:A) b
        os << "fn(" << a->name << ":";
        print_ast(os, a->child[0], indent);
        os << ") -> ";
        print_ast(os, a->child[1], indent);
        break;
    case Type::fnT:     // polymorphic function, fn(X<:A) b
        os << "fn(" << a->name << "<:";
        print_ast(os, a->child[0], indent);
        os << ") -> ";
        print_ast(os, a->child[1], indent);
        break;
    case Type::Group:  // grouping, {A}
    case Type::group:   // grouping, {a}
        os << "{";
        print_group(os, a, indent+4);
        print_indent(os, indent);
        os << "}";
        print_indent(os, indent);
        break;
    case Type::top:     // member of Top
        os << "top";
        break;
    case Type::app:     // application, b(a)
        if(a->child[0]->t == Type::fn) {
            os << "let " << a->name << ":(";
            print_ast(os, a->child[0]->child[0], indent+2);
            os << ") = ";
            print_ast(os, a->child[1], indent+2);
            print_indent(os, indent); os << "  in ";
            print_ast(os, a->child[0]->child[1], indent+4);
        } else {
            os << "(";
            print_ast(os, a->child[0], indent);
            os << ") ";
            print_ast(os, a->child[1], indent);
        }
        break;
    case Type::appT:    // type application, b(:A)
         if(a->child[0]->t == Type::fnT) {
            os << "Let " << a->name << "<:(";
            print_ast(os, a->child[0]->child[0], indent+2);
            os << ") =";
            print_ast(os, a->child[1], indent+2);
            print_indent(os, indent); os << "  in ";
            print_ast(os, a->child[0]->child[1], indent+4);
        } else {
            os << "(";
            print_ast(os, a->child[0], indent);
            os << "): ";
            print_ast(os, a->child[1], indent);
        }
        break;
    }
}
