#include <iostream>

#include "ast.hpp"
#include "stack.hpp"

#include "unwind.hpp"

AstP Pair(AstP A, AstP B) {
    AstP C = Var("C");
    return ForAll("C", Top(), Fn(Fn(A, Fn(B, C)), C));
}

void process(AstP a, bool isT) {
    Stack *s = new Stack(nullptr, a, isT);

    a = get_ast(s);
    std::cout << "  Stack:   ";
    print_ast(a, 0); std::cout << std::endl;
    AstP t = get_type(s);
    std::cout << "  Type:   ";
    print_ast(t, 0); std::cout << std::endl;

    //eval_need(s);
    //std::cout << "Eval-d:  ";
    //print_ast(get_ast(s), 0); std::cout << std::endl;

    stack_dtor(s);
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
    AstP Bool = ForAll("X", T, Fn(X, Fn(X, X)));

    AstP Id = ForAll("X", T, Fn(X, X));
    AstP id = fnT("X", T, fn("x", X, x));
    AstP g = Group("Id", Id, top());
    g = group("id", id, g);
    g = group("id2", app(appT(id, Id), id), g);
    g = Group("Bool",  Bool, g);
    g = Group("True", ForAll("X", T, Fn(X, Fn(T, X))), g);
    g = Group("False", ForAll("X", T, Fn(T, Fn(X, X))), g);
    g = group("true", fnT("X", T, fn("x",X,fn("y",X,x))), g);
    g = group("false", fnT("X", T, fn("x",X,fn("y",X,y))), g);
    g = group("tt", fnT("X", T, fn("x",X,fn("y",T,x))), g);
    g = group("ff", fnT("X", T, fn("x",T,fn("y",X,y))), g);
    g = group("cond", fnT("X", T, fn("b",Bool,appT(var("b"),X))), g);

    AstP P = Pair(A, B);
    AstP pair = fnT("A", T, fnT("B", T, fn("a", A, fn("b", B,
                            fnT("C", T,
                                fn("p", Fn(A, Fn(B, C)),
                                        app(app(p, a), b)))
                            ))));
    /*AstP pairT = ForAll("A", T, ForAll("B", T, Fn(A, Fn(B, pair(A,B)))));
    AstP fstT  = ForAll("A", T, ForAll("B", T, Fn(P, A)));
    AstP sndT  = ForAll("A", T, ForAll("B", T, Fn(P, B)));*/
    g = group("pair", pair, g);
    g = group("fst", fnT("A", T, fnT("B", T, fn("p", P,
                        app(appT(p, A), fn("a",A,fn("b",B,a)))
                   ))), g);
    g = group("snd", fnT("A", T, fnT("B", T, fn("p", P,
                        app(appT(p, B), fn("a",A,fn("b",B,b)))
                   ))), g);
    AstP once = fnT("A",T,fn("f", Fn(A, A),
                                fn("x",A, app(var("f"),x) )));
    g = group("once", once, g);
    AstP twice = fnT("A",T,fn("f", Fn(A, A),
                           fn("x", A,
                               app(var("f"), app(var("f"), x))) ));
    // TODO: improve error message for:
                               //app(app(var("f"), x), x)) ));
    g = group("twice", twice, g);
    g = group("id1x", app(app(appT(once,Id), appT(id,Id)), id), g);
    g = group("id2x", app(app(appT(twice,Id), appT(id,Id)), id), g);

    numberAst(&g);
    std::cout << "Initial = ";
    print_ast(g, 0); std::cout << std::endl;

    for(; g->t == Type::group || g->t == Type::Group; g=g->child[1]) {
        printf("========== %s ==========\n", g->name.c_str());
        process(g->child[0], g->t == Type::Group);
    }
    return 0;
}
