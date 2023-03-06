#include <iostream>

#include "ast.hpp"
#include "stack.hpp"

#include "wind.hpp"

AstP pair(AstP A, AstP B) {
    AstP C = Var("C");
    return ForAll("C", Top(), Fn(Fn(A, Fn(B, C)), C));
}

int main(int argc, char *argv[]) {
    AstP T = Top();
    AstP X = Var("X");
    AstP A = Var("A");
    AstP B = Var("B");
    AstP C = Var("C");
    AstP x = var("x");
    AstP y = var("y");
    AstP a = var("a");
    AstP b = var("b");
    AstP p = var("p");

    AstP Id = ForAll("X", T, Fn(X, X));
    AstP id = fnT("X", T, fn("x", X, x));

    AstP Bool  = ForAll("X", T, Fn(X, Fn(X, X)));
    AstP True  = ForAll("X", T, Fn(X, Fn(T, X)));
    AstP False = ForAll("X", T, Fn(T, Fn(X, X)));
    AstP true_  = fnT("X", T, fn("x",X,fn("y",X,x)));
    AstP false_ = fnT("X", T, fn("x",X,fn("y",X,y)));
    AstP tt = fnT("X", T, fn("x",X,fn("y",T,x)));
    AstP ff = fnT("X", T, fn("x",T,fn("y",X,y)));
    AstP cond = fnT("X", T, fn("b",Bool,appT(var("b"),X)));

    AstP P = pair(A, B);
    /*AstP pairT = ForAll("A", T, ForAll("B", T, Fn(A, Fn(B, pair(A,B)))));
    AstP fstT  = ForAll("A", T, ForAll("B", T, Fn(P, A)));
    AstP sndT  = ForAll("A", T, ForAll("B", T, Fn(P, B)));*/
    AstP pair = fnT("A", T, fnT("B", T, fn("a", A, fn("b", B,
                            fnT("C", T,
                                fn("p", Fn(A, Fn(B, C)),
                                        app(app(p, a), b)))
                            ))));
    AstP fst  = fnT("A", T, fnT("B", T, fn("p", P,
                        app(appT(p, A), fn("a",A,fn("b",B,a)))
                   )));
    AstP snd  = fnT("A", T, fnT("B", T, fn("p", P,
                        app(appT(p, B), fn("a",A,fn("b",B,b)))
                   )));

    Stack *s_snd = new Stack(nullptr, snd, false);

    std::cout << "Initial: ";
    print_ast(snd, 0); std::cout << std::endl;

    std::cout << "Stack:   ";
    print_ast(get_ast(s_snd), 0); std::cout << std::endl;

    eval_need(s_snd);
    std::cout << "Eval-d:  ";
    print_ast(get_ast(s_snd), 0); std::cout << std::endl;

    stack_dtor(s_snd);

    return 0;
}
