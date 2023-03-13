#include <stdio.h>

#include "ast.hpp"
#include "stack.hpp"
#include "wind.hpp"

// Resolve a name to a de-Bruijn index.
static int lookup1(const std::string &name, Bind *assoc) {
    int n=0;
    for(; assoc != nullptr; ++n, assoc=assoc->next) {
        if(assoc->name == name) {
            return n;
        }
    }
    return -1;
};

// Traverse x and number all named Var-s.
// Replaces x with a numbered Ast.
//
// We use a hacked Bind chain here to track binding depth
// but a linked-list with names would work just as well.
void numberAst(AstP *x, Bind *assoc) {
    Bind *const first = assoc;
    while(true) {
        AstP y = std::make_shared<Ast>((*x)->t);
        if((*x)->t == Type::Group || (*x)->t == Type::group) {
            y->name = (*x)->name;
        }
        if((*x)->t == Type::Var || (*x)->t == Type::var) {
            y->n = lookup1((*x)->name, assoc);
            if(y->n < 0) {
                y->t = Type::error;
                y->name = std::string("Undefined variable " + (*x)->name);
            }
            *x = y;
            break;
        }

        // Handle all other cases.
        int nchild = getNChild((*x)->t);
        if(nchild == 0) break; // retain old Ast
        for(int i=0; i<nchild; ++i) {
            y->child[i] = (*x)->child[i];
        }

        // Generic recursion over first nchild-1 binders.
        for(int i=0; i<nchild-1; ++i) {
            numberAst(&y->child[i], assoc);
        }
        // The last child of a binding contains the binding.
        if(isBind((*x)->t)) {
            assoc = new Bind(assoc, (*x)->t, (*x)->name, nullptr, nullptr);
        }
        // continue the while loop on the last child
        *x = y;
        x = &y->child[nchild-1];
    }
    // GC by deleting (assoc -> ... ->) first
    Bind *c;
    while(assoc != first) {
        c = assoc;
        assoc = c->next;
        delete c;
    }
}

/** Search the parent stack's context to find the binder
 *  corresponding to the context that `s` should have.
 */
Bind *Stack::outer_ctxt() const {
    const Stack *s = this;
    const Stack *p = s->parent;
    if(p == nullptr) {
        return nullptr;
    }
    for(const Stack *app = p->app; app != nullptr; app = app->next) {
        if(app == s) { // application rhs-s get full context
            return p->ctxt;
        }
    }
    for(Bind *c = p->ctxt; c != nullptr; c = c->next) {
        // type annotations get 'next' context
        if(c->rht == s) {
            return c->next;
        }
        // application right-hand sides get 'next' context
        if(c->rhs == s) {
            return c->next;
        }
    }
    throw std::runtime_error("Stack not found in parent."); // s in p
}

/**
 * Step through the binding stack until
 * a non-null c is found or the `parent'
 * stack has been reached.  If c == nullptr
 * on return, there are no more bindings.
 *
 * Returns true if `c` is defined (non-null).
 *
 * Note: to count binders from a head term,
 * initialize c = s->ctxt before calling this function.
 */
bool cur_bind(Stack const *&s, Bind const *&c, Stack const *parent) {
    while(c == nullptr && s != parent) {
        if(s->parent == parent) {
            s = parent;
            break;
        }
        c = s->outer_ctxt();
        s = s->parent;
    }
    return c != nullptr;
}
bool cur_bind(Stack *&s, Bind *&c,
              Stack *parent, bool initial=false) {
    while(c == nullptr && s != parent) {
        if(s->parent == parent) {
            s = parent;
            break;
        }
        if(initial) {
            if(s->parent == nullptr) {
                throw std::runtime_error("Missing parent in chain.");
                s = nullptr; c = nullptr; break;
            }
            c = s->parent->ctxt;
        } else {
            c = s->outer_ctxt();
        }
        s = s->parent;
    }
    return c != nullptr;
}

/** Find the de-Bruijn index of `ref` with respect to the
 *  head term of s.  Inverse of `lookup`.
 *
 *  Note: s->number_var(var, s->outer_ctxt(), nullptr)
 *        is the de-Bruijn index of the first variable
 *        outside this stack's local context.
 *
 *        s->number_var(var, nullptr, nullptr)
 *        is the total number of binders up to the
 *        root of the term.
 */
bool Stack::number_var(intptr_t *nv, Bind *ref, const Stack *parent) const {
    const Stack *s = this;
    const Bind *c = s->ctxt;
    int n = 0;
    for(; cur_bind(s, c, parent); ++n, c = c->next) {
        if(c == ref) {
            *nv = n;
            return false;
        }
    }
    if(ref == nullptr) {
        *nv = n;
        return false;
    }
    if(s == parent) { // c is now in the parent stack's scope.
                      // Pass through pointer directly
        *nv = (intptr_t)ref; // as a "global" named variable.
        return true;
    }
    *nv = -1; // indicates an error (ref not found in binding stack)
    return false;
}

/** Look up the variable binding in this stack,
 *  resolving a de-Bruijn index to its corresponding Bind.
 *
 *  If incr is true, increments the binding's reference
 *  count.
 *
 *  @returns pointer to the binder, or nullptr if not found
 *
 *  Note: Use incr if you intend to add this reference
 *  somewhere in the tree rooted at `this`.
 */
Bind *Stack::lookup(int n, bool initial) {
    Stack *s = this;
    Bind *c = s->ctxt;

    if(n < 0) return nullptr;

    while(cur_bind(s, c, nullptr, initial) && --n >= 0) {
        c = c->next;
    }
    return c;
}

/** Lookup the variable binding in the stack,
 *  and set s->ref to the corresponding Bind
 *  expression.  Note this does not increment
 *  the refcount, so do that on your own
 *  if you are creating a refernce to this binder.
 *
 *  On failure, sets s to an error message
 *  and returns false.
 *
 *  Returns true on success.
 */
bool Stack::deref(AstP a, bool initial) {
    if(a->isPtr) {
        ref = a->ref();
    } else {
        ref = lookup(a->n, initial);
    }
    if(ref == nullptr) {
        if(a->name.size() > 0) {
            set_error(std::string("Unbound variable, ") + a->name);
        } else {
            set_error("Unbound variable.");
        }
        return false;
    }
    return true;
}

Stack::Stack(Stack *_p, AstP a, bool isT, Stack *_next)
        : parent(_p), ctxt(nullptr), app(nullptr), next(_next) {
    if(isT) {
        windType(a);
    } else {
        wind(a);
    }
}

struct windStack {
    bool err = false;
    Stack *s;
    windStack(Stack *_s) : s(_s) {}

    AstP val(AstP a) {
        s->t = a->t;
        switch(a->t) {
        case Type::var:
            if(!s->deref(a, true)) {
                return nullptr;
            }
            if(a->t == Type::var && bindType(s->ref->t)) {
                s->set_error("var refers to a binding for types.");
            }
            s->ref->nref++;
            break;
        case Type::Var:
            s->set_error("Encountered a type variable, but expected a term.");
            break;
        case Type::Top:    // largest type
        case Type::top:     // member of Top
        case Type::Group:  // grouping, {A}
            break;
        case Type::group:         // grouping, {a}
            s->set_error("Group not handled.");
            break;
        case Type::error:
            s->set_error(a->name);
            break;
        default:
            fprintf(stderr, "Encountered invalid value in wind (%d).\n", a->t);
            break;
        }
        //if(s->t != Type::error && isType(s->t)) {
        if(isType(s->t)) {
            s->set_error("Expected term, but found a type.");
        }
        return nullptr;
    }
    AstP bind(AstP a) {
        Stack *rhs = s->app;
        if(s->app) {
            s->app = rhs->next;
        }
        s->ctxt = new Bind(s->ctxt, a->t, a->name,
                    new Stack(s, a->child[0], true), rhs);
        return a->child[1];
    }
    AstP apply(AstP a) {
        bool isT = a->t == Type::appT; // Is rhs a type?
        s->app = new Stack(s, a->child[1], isT, s->app);
        return a->child[0];
    }
};

struct windStackType {
    bool err = false;
    Stack *s;
    Stack *args;
    windStackType(Stack *_s, Stack *app) : s(_s), args(app) {}

    AstP val(AstP a) {
        switch(a->t) {
        case Type::Var:    // type variables, X
            if(!s->deref(a, true)) {
                return nullptr;
            }
            if(!bindType(s->ref->t)) {
                s->set_error("Var refers to a binding for values.");
                return nullptr;
            }
            if(s->ref->rhs == nullptr) {
                ++s->ref->nref; // no known substitition for this type var
            } else {
                return get_ast(s->ref->rhs); // locally nameless Ast
            }
            break;
        case Type::var:    // variables, x
            s->set_error("Encountered a term variable, but expected a type.");
            return nullptr;
        case Type::group:  // grouping, {a}
        case Type::Top:    // largest type
        case Type::top:     // member of Top
            break;
        case Type::Group:  // grouping, {A}
            s->set_error("Group not handled.");
            return nullptr;
        case Type::error:
            s->set_error(a->name);
            return nullptr;
        default:
            fprintf(stderr, "Encountered invalid value in wind (%d).\n", a->t);
            return nullptr;
        }
        // type = type of bottom term in Ast.
        s->t = a->t;
        if(s->t != Type::error && !isType(s->t)) {
            s->set_error("Expected type, but found a term.");
        }
        return nullptr;
    }
    AstP bind(AstP a) {
        const char err1[] = "Invalid application of type (A->B).";
        const char err2[] = "Invalid application of type All(X:<A) B.";
        Stack *rhs = s->app;
        if(rhs) {
            s->set_error(a->t == Type::Fn ? err1 : err2);
            return nullptr;
        }

        switch(a->t) {
        case Type::Fn:      // function spaces, A->B
            if(args) {
                // Verify function type, do not add this binder to the stack.
                AstP rht = get_type(args);
                TracebackP tb = subType(rht, a->child[0]);
                if(tb) {
                    s->set_error("Invalid function argument type: " + tb->name);
                    return nullptr;
                }
                args = args->next;
            } else {
                s->ctxt = new Bind(s->ctxt, a->t,
                                new Stack(s, a->child[0], true), rhs);
            }
            break;
        case Type::ForAll:  // bounded quantification, All(X<:A) B
            if(args) {
                // TODO: store app vs. appT in an "application cell"
                // It's still safe to assume isType is accurate though,
                // since its checked on Stack::wind{Type}.
                //
                // args being a type also means args is fully evaluated
                // at this point.
                if(!isType(args->t)) {
                    s->set_error("fnT applied to non-type");
                    return nullptr;
                }
                AstP rht = get_ast(args);
                // need to evaluate a->child[0] in order
                // to resolve bindings added during this windType traversal.
                Stack *rht_ts = new Stack(s, a->child[0], true);
                AstP rht_t = get_ast(rht_ts);
                TracebackP tb = subType(rht, rht_t);
                if(tb) {
                    stack_dtor(rht_ts);
                    s->set_error("Argument of fnT does not match expected: "
                                 + tb->name);
                    return nullptr;
                }
                // Note, this effectively copies the stack.
                // We *might* be able to move args in-place if
                // we swapped out a place-holder like Top.
                // However, further get_ast-s would be incorrect.
                Stack *rhts = new Stack(s, rht, true);
                // rhs is known to be a type, bind it as fnT
                s->ctxt = new Bind(s->ctxt, Type::fnT, a->name,
                                   rht_ts, nullptr);
                // prevent unification again
                s->ctxt->rhs = rhts;
                args = args->next;
            } else {
                s->ctxt = new Bind(s->ctxt, a->t, a->name,
                                new Stack(s, a->child[0], true), rhs);
            }
            break;
        case Type::fn:
        case Type::fnT:
            s->set_error("Invalid fn/fnT inside a type.");
            break;
        default:
            fprintf(stderr, "Encountered invalid bind in wind (%d).\n", a->t);
            return nullptr;
        }
        return a->child[1];
    }
    AstP apply(AstP a) {
        s->set_error("Invalid application of a type.");
        return nullptr;
    }
};

// Is this stack a trivial value?
// note: the ctxt check is technically not needed, since
// any ctxt passing isValue&&app==null is unreachable code.
bool Stack::isTrivial() const {
    return isValue(t) && app == nullptr && ctxt == nullptr;
}

/** Wind the value `a` onto the stack.
 *  Only types are evaluated.
 */
bool Stack::wind(AstP a) {
    windStack W(this);
    ::wind(&W, a);
    return W.err;
}

/** Wind the type `a` onto the stack, evaluating completely
 *  to a term.
 *
 *  If app is non-null, it is treated as a series of
 *  known function arguments.  In this case,
 *  the right-hand sides will be typed.
 *  The stack returned will represent
 *  the return type of the function application.
 *
 *  Returns true on success or false if an error
 *  is encountered.
 */
bool Stack::windType(AstP a, Stack *app) {
    windStackType W(this, app);
    ::wind(&W, a);
    if(W.args) { // args remain after
        set_error("Application of a function to too many args.");
        W.err = true;
    }
    return W.err;
}

bool Bind::check_rhs() {
    if(rhs) {
        AstP A = get_type(rhs);
        AstP B = get_ast(rht);
        TracebackP err = subType(A, B);
        if(err) {
            rhs->set_error(err->name);
            return false;
        }
    }
    return true;
}
