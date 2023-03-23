#pragma once

#include <string>
#include "error.hpp"

struct Bind;
struct Stack;

/** Linked list of variable contexts.
 */
struct Bind {
    Type t;
    Bind *next;
    std::string name; // for readability only
    Stack *rht; // Note: this could just as easily be an AstP
    Stack *rhs;
    int nref; // number of references to binding

    // "open" binding (denotes function type)
    Bind(Bind *_next, Type _t)
        : t(_t), next(_next)
        , rht(nullptr), rhs(nullptr), nref(0) {}
    // "open" named binding (denotes function type)
    Bind(Bind *_next, Type _t, const std::string &_name)
        : t(_t), next(_next), name(_name)
        , rht(nullptr), rhs(nullptr), nref(0) {}
    // nameless binding with rhs
    Bind(ErrorList &err, Bind *_next, Type _t, Stack *_rht, Stack *_rhs)
        : t(_t), next(_next)
        , rht(_rht), rhs(_rhs), nref(0) { check_rhs(err); }

    // named binding
    Bind(ErrorList &err, Bind *_next, Type _t, const std::string &_name,
            Stack *_rht, Stack *_rhs)
        : t(_t), next(_next)
        , name(_name)
        , rht(_rht), rhs(_rhs), nref(0) { check_rhs(err); }

    void check_rhs(ErrorList &err);
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
    Stack *parent;   ///< parent chain for binding location of stack
                     //   (creating a chain of "head" terms)
    Bind *ctxt;      ///< linked list of bindings (local to this stack)
    Stack *app;      ///< linked list of right-hand sides
    Stack *next;     ///< linker for right-hand sides

    Bind *ref;       ///< TVar / var
    /// Weak-pointer to traceback. For print only.
    //  Do not dereference this pointer!
    Traceback const *err = nullptr;

    // Construct a "blank" stack with nothing on it.
    Stack(Stack *_parent) : parent(_parent), ctxt(nullptr),
                            app(nullptr), next(nullptr) {}
    // Creation of a stack "winds up" the Ast.
    Stack(ErrorList &, Stack *parent, AstP a, bool isT, Stack *next=nullptr);
    // Used during construction of the stack from an Ast.
    Bind *lookup(int n, bool initial=false);
    bool deref(AstP a, bool initial);
    Bind *outer_ctxt() const;
    bool isTrivial() const;

    bool number_var(intptr_t *n, Bind *ref, const Stack *parent) const;
    // Used to wind an Ast onto the head term of the stack.
    void wind(ErrorList &, AstP a);
    void windType(ErrorList &, AstP a, Stack *app = nullptr);

    /** Add traceback information during an operation that might
     *  throw an error.  Does nothing if no error is present.
     */
    TracebackP traceback(const TBPrint &w, TracebackP &&next) {
        if(next == nullptr) return next;
        TracebackP tb = mkTB(w, std::move(next));
        err = tb.get();
        return tb;
    }
    /** Create an error message and mark the current stack as its source.
     *  When searching through the stack later, the marking can be
     *  used to refer back to the corresponding error description
     *  (via numerical referencing).
     */
    TracebackP set_error(const std::string &name) {
        TracebackP tb = mkError(name);
        err = tb.get();
        return tb;
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

// Multiple wind functions are possible.
template <typename windFn>
void wind(windFn *F, AstP a) {
    while(a != nullptr) {
        switch(a->t) {
        case Type::Fn:
        case Type::ForAll:
        case Type::fn:
        case Type::fnT:
            a = F->bind(a);
            // usually a->child[1];
            break;
        case Type::app:
        case Type::appT:
            a = a = F->apply(a);
            // usually a->child[0];
            break;
        case Type::var:
        case Type::Var:
        default:
            a = F->val(a);
            break;
        }
    }
}

void numberAst(ErrorList &err, AstP *x, Bind *assoc = nullptr);
