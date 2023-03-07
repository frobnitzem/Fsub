#pragma once

#include <string>

struct Bind;
struct Stack;

/** Linked list of variable contexts.
 */
struct Bind {
    Type t;
    Bind *next;
    std::string name; // for readability only
    Stack *rht;
    Stack *rhs;
    int nref; // number of references to binding

    // "open" binding (denotes function type)
    Bind(Bind *_next, Type _t)
        : t(_t), next(_next)
        , rht(nullptr), rhs(nullptr), nref(0) {}
    // nameless binding with rhs
    Bind(Bind *_next, Type _t, Stack *_rht, Stack *_rhs)
        : t(_t), next(_next)
        , rht(_rht), rhs(_rhs), nref(0) {}
    // named binding
    Bind(Bind *_next, Type _t, const std::string &_name,
            Stack *_rht, Stack *_rhs)
        : t(_t), next(_next)
        , name(_name)
        , rht(_rht), rhs(_rhs), nref(0) {}
};

/** Cons cell for an application
 *
 * Stack1 @ (Stack2 @ Stack3) @ Stack 4
 *
 * Stack1 -app-> Stack2 -next-> Stack4
 *                 \
 *                  `-app-> Stack3
 *
 * `parent` tree -- All share Stack1's context,
 *                  since the application occurs
 *                  after Stack1's bindings.
 *
 * Since applications can be either app or appT
 * (denoting application of a function to a variable
 * or to a type, resp.).  We *could* that by storing
 * an extra variable, isT.  However, we could also
 * just use the type marking as a check on the way down
 * (which is what we do).
 *
 * When rolling back up to an Ast, we need to know
 * whether every right-hand side represents a type.
 * This is easy to do by inspecting the head term.
 *
 *      Stack1
 *     /      \
 * Stack2    Stack4
 *    |
 * Stack3
 *
 */
struct Stack {
    Type t;
    Stack *parent;   // parent chain for binding location of stack
                     // (creating a chain of "head" terms)
    Bind *ctxt;  // linked list of bindings (local to this stack)
    Stack *app;  // linked list of right-hand sides
    Stack *next; // linker for right-hand sides

    Bind *ref;        // TVar / var
    std::string err;  // holding an error message

    // Creation of a stack "winds up" the Ast.
    Stack(Stack *parent, AstP a, bool isT, Stack *next=nullptr);
    // Used during construction of the stack from an Ast.
    Bind *lookup(int n, bool initial=false);
    Bind *outer_ctxt() const;
    bool number_var(intptr_t *n, Bind *ref, Stack *parent) const;
    // Used to wind an Ast onto the head term of the stack.
    bool wind(AstP a, bool isT);

    void set_error(const std::string &msg) {
        err = msg;
        t = Type::error;
    }
};

// Multiple unwind functions are possible.
template <typename SFold>
void unwind(SFold *f, struct Stack *s) {
    while( ! f->val(s) ); // trampoline
    // Caution! f->val() may deref Var-s,
    // and change `s->c, s->stack'

    Stack *next;
    for(Stack *sup = s->app; sup != NULL; sup = next) {
        next = sup->next;
        f->apply(sup); // ascend application
    }
    Bind *cn;
    for(Bind *c = s->ctxt; c != NULL; c = cn) {
        cn = c->next;
        f->bind(c); // ascend binder
    }
} 

void numberAst(AstP x, Bind *assoc = nullptr);
