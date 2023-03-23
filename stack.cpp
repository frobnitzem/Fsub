#include <stdio.h>

#include "ast.hpp"
#include "stack.hpp"
#include "unwind.hpp"

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
void numberAst(ErrorList &err, AstP *x, Bind *assoc) {
    Bind *const first = assoc;
    while(true) {
        AstP y = std::make_shared<Ast>((*x)->t);
        if((*x)->t == Type::Group || (*x)->t == Type::group) {
            y->name = (*x)->name;
        }
        if((*x)->t == Type::Var || (*x)->t == Type::var) {
            y->n = lookup1((*x)->name, assoc);
            if(y->n < 0) {
                err.append( y->set_err("Undefined variable.") );
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
            numberAst(err, &y->child[i], assoc);
        }
        // The last child of a binding contains the binding.
        if(isBind((*x)->t)) {
            assoc = new Bind(assoc, (*x)->t, (*x)->name);
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
 *  Returns true if *nv is a pointer.
 *  or false if *nv is a de-Bruijn index.
 *
 *  Note: s->number_var(_, s->outer_ctxt(), nullptr)
 *        is the de-Bruijn index of the first variable
 *        outside this stack's local context.
 *
 *        s->number_var(_, nullptr, nullptr)
 *        is -1 (undefined variable).
 */
bool Stack::number_var(intptr_t *nv, Bind *ref, const Stack *parent) const {
    const Stack *s = this;
    const Bind *c = s->ctxt;
    if(!ref) {
        *nv = -1;
        return false;
    }
    int n = 0;
    for(; cur_bind(s, c, parent); ++n, c = c->next) {
        if(c == ref) {
            *nv = n;
            return false;
        }
    }
    // number of binders to root of sub-term
    /*if(ref == nullptr) {
        *nv = n;
        return false;
    }*/
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
        return false;
    }
    return true;
}

Stack::Stack(ErrorList &err, Stack *_p, AstP a, bool isT, Stack *_next)
        : parent(_p), ctxt(nullptr), app(nullptr), next(_next) {
    if(isT) {
        windType(err, a);
    } else {
        wind(err, a);
    }
}

struct windStack {
    ErrorList &err;
    Stack *s;
    windStack(ErrorList &_err, Stack *_s) : err(_err), s(_s) {}

    AstP val(AstP a) {
        s->t = a->t;
        switch(a->t) {
        case Type::var:
            if(!s->deref(a, true)) {
                err.append(s->set_error("Unbound variable."));
                break;
            }
            if(a->t == Type::var && bindType(s->ref->t)) {
                err.append(s->set_error("var refers to a binding for types."));
            }
            s->ref->nref++;
            break;
        case Type::Var:
            err.append(s->set_error(
                         "Encountered a type variable, but expected a term."));
            break;
        case Type::Top:    // largest type
        case Type::top:     // member of Top
        case Type::Group:  // grouping, {A}
            break;
        case Type::group:         // grouping, {a}
            err.append(s->set_error(
                        "Encountered a type variable, but expected a term."));
            break;
        default:
            throw std::runtime_error("Encountered invalid value in wind");
            break;
        }
        if(!s->err && isType(s->t)) {
            err.append(s->set_error("Expected term, but found a type."));
        }
        return nullptr;
    }
    AstP bind(AstP a) {
        Stack *rhs = s->app;
        if(a->t != Type::fn && a->t != Type::fnT) {
            err.append(s->set_error("Unexpected type function in term."));
        }
        if(rhs) {
            if(a->t == Type::fn && isType(rhs->t)) {
                err.append(s->set_error("Function applied to type."));
            } else if(a->t == Type::fnT && !isType(rhs->t)) {
                err.append(s->set_error("fnT applied to term."));
            }
            s->app = rhs->next;
        }
        s->ctxt = new Bind(err, s->ctxt, a->t, a->name,
                    new Stack(err, s, a->child[0], true), rhs);
        return a->child[1];
    }
    AstP apply(AstP a) {
        bool isT = a->t == Type::appT; // Is rhs a type?
        s->app = new Stack(err, s, a->child[1], isT, s->app);
        return a->child[0];
    }
};

struct windStackType {
    ErrorList &err;
    Stack *s;
    Stack *args;
    windStackType(ErrorList &_err, Stack *_s, Stack *app)
        : err(_err), s(_s), args(app) {}

    AstP val(AstP a) {
        s->t = a->t;
        switch(a->t) {
        case Type::Var:    // type variables, X
            if(!s->deref(a, true)) {
                err.append(s->set_error("Unbound variable."));
                return nullptr;
            }
            if(!bindType(s->ref->t)) {
                err.append(s->set_error("Var refers to a binding for values."));
                ++s->ref->nref;
                return nullptr;
            }
            if(s->ref->rhs == nullptr) {
                ++s->ref->nref; // no known substitition for this type var
            } else {
                return get_ast(s->ref->rhs); // locally nameless Ast
            }
            break;
        case Type::var:    // variables, x
            err.append(s->set_error(
                        "Encountered a term variable, but expected a type."));
            return nullptr;
        case Type::group:  // grouping, {a}
        case Type::Top:    // largest type
        case Type::top:     // member of Top
            break;
        case Type::Group:  // grouping, {A}
            err.append(s->set_error("Group not handled."));
            return nullptr;
        default:
            throw std::runtime_error("Encountered invalid value in wind.");
            //fprintf(stderr, "Encountered invalid value in wind (%d).\n", a->t);
            return nullptr;
        }
        // type = type of bottom term in Ast.
        if(!s->err && !isType(s->t)) {
            err.append(s->set_error("Expected type, but found a term."));
        }
        return nullptr;
    }
    AstP bind(AstP a) {
        const char err1[] = "Invalid application of type (A->B).";
        const char err2[] = "Invalid application of type All(X:<A) B.";
        Stack *rhs = s->app;
        if(rhs) {
            err.append(s->set_error(a->t == Type::Fn ? err1 : err2));
            return nullptr;
        }

        if(args) {
            // TODO: store app vs. appT in an "application cell"
            // It's still safe to assume isType is accurate though,
            // since its checked on Stack::wind{Type}.
            //
            // Since we are creating binders, args and the current
            // ast are no longer fully evaluated at this point.
            //
            AstP rht;
            if(a->t == Type::ForAll) {
                if(!isType(args->t)) {
                    err.append(s->set_error("fnT applied to non-type"));
                    args = nullptr;
                    return nullptr;
                }
                rht = get_ast(args);
            } else if(a->t == Type::Fn) {
                if(isType(args->t)) {
                    err.append(s->set_error("fn applied to type"));
                    args = nullptr;
                    return nullptr;
                }
                rht = get_type(err, args);
            } else {
                err.append(s->set_error(
                            "Unexpected regular function in type."));
                args = nullptr;
                return nullptr;
            }
            // Need to evaluate a->child[0] in order
            // to resolve bindings added during this windType traversal.
            // TODO: use fewer wind/unwind steps.
            Stack *rht_ts = new Stack(err, s, a->child[0], true);
            AstP rht_t = get_ast(rht_ts);
            TracebackP tb = subType(rht, rht_t);
            if(tb) {
                err.append(s->traceback([=](std::ostream &os){
                               os << "Invalid function application.\n";
                           }, std::move(tb)));
                // we can proceed to check more args anyway
                //stack_dtor(rht_ts);
                //return nullptr;
            }
            // Note, this effectively copies the stack.
            // We *might* be able to move args in-place if
            // we swapped out a place-holder like Top.
            // However, further get_ast-s would be incorrect.
            Stack *rhts = new Stack(err, s, rht, true);
            // rhs is known to be a type, bind it as fnT
            s->ctxt = new Bind(err, s->ctxt, Type::fnT, a->name,
                               rht_ts, nullptr);
            // prevent unification again
            s->ctxt->rhs = rhts;
            args = args->next;
            return a->child[1];
        }

        switch(a->t) {
        case Type::Fn:      // function spaces, A->B
            s->ctxt = new Bind(err, s->ctxt, a->t,
                            new Stack(err, s, a->child[0], true), rhs);
            break;
        case Type::ForAll:  // bounded quantification, All(X<:A) B
            s->ctxt = new Bind(err, s->ctxt, a->t, a->name,
                            new Stack(err, s, a->child[0], true), rhs);
            break;
        case Type::fn:
        case Type::fnT:
            err.append(s->set_error("Invalid fn/fnT inside a type."));
            break;
        default:
            throw std::runtime_error("Encountered invalid bind in wind.");
            //fprintf(stderr, "Encountered invalid bind in wind (%d).\n", a->t);
            return nullptr;
        }
        return a->child[1];
    }
    AstP apply(AstP a) {
        err.append(s->set_error("Invalid application of a type."));
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
void Stack::wind(ErrorList &err, AstP a) {
    windStack W(err, this);
    ::wind(&W, a);
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
void Stack::windType(ErrorList &err, AstP a, Stack *app) {
    windStackType W(err, this, app);
    ::wind(&W, a);
    if(W.args) { // args remain after wind
        err.append(
                  set_error("Application of a function to too many args.")
                );
    }
}

void Bind::check_rhs(ErrorList &err) {
    if(rhs) {
        AstP A = get_type(err, rhs);
        AstP B = get_ast(rht);
        err.append(rhs->traceback([=](std::ostream& os) {
                        os << "Invalid argument type.\n";
               }, subType(A, B)));
    }
}
