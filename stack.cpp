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
    // TODO: replace with std::runtime_error
    fprintf(stderr, "Stack %p not found in parent %p\n", s, p);
    return nullptr;
}

/**
 * Step through the binding stack until
 * a non-null c is found.  If c == nullptr
 * on return, there are no more bindings.
 *
 * Returns true if `c` is defined (non-null).
 *
 * Note: to count binders from a head term,
 * initialize c = s->ctxt before calling this function.
 */
bool cur_bind(Stack const *&s, Bind const *&c) {
    while(c == nullptr && s != nullptr) {
        c = s->outer_ctxt();
        s = s->parent;
    }
    return c != nullptr;
}
bool cur_bind(Stack *&s, Bind *&c, bool initial=false) {
    while(c == nullptr && s != nullptr) {
        if(initial) {
            if(s->parent == nullptr) {
                c = nullptr;
                break;
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
bool Stack::number_var(intptr_t *nv, Bind *ref, Stack *parent) const {
    const Stack *s = this;
    const Bind *c = s->ctxt;
    int n = 0;
    for(; cur_bind(s, c); ++n, c = c->next) {
        if(s == parent) { // c is now in the parent stack's scope.
                          // Pass through pointer directly
            *nv = (intptr_t)ref; // as a "global" named variable.
            return true;
        }
        if(c == ref) {
            *nv = n;
            return false;
        }
    }
    if(ref == nullptr) {
        *nv = n;
        return false;
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

    while(cur_bind(s, c, initial) && --n >= 0) {
        c = c->next;
    }
    return c;
}

Stack::Stack(Stack *_p, AstP a, bool isT, Stack *_next)
        : parent(_p), ctxt(nullptr),
          app(nullptr), next(_next) {
    wind(a, isT);
}

/** TODO: Take another Stack *t arg to fill in
 *  the term's type.
 */
bool Stack::wind(AstP a, bool isT) {
    if(isT) {
        bool ok = windType(a);
        //if(!ok) { // Discovered an error in stack.
        //}
        return true;
    }
    while(true) {
        switch(a->t) {
        case Type::var:     // variables, x
            if(a->isPtr) {
                ref = a->ref();
            } else {
                ref = lookup(a->n, true);
            }
            if(ref == nullptr) {
                if(a->name.size() > 0) {
                    set_error(std::string("Unbound variable, ") + a->name);
                } else {
                    set_error("Unbound variable.");
                }
                return true;
            } else {
                ++ref->nref;
            }
            if(a->t == Type::var && bindType(ref->t)) {
                set_error("RHS of var is a type.");
                return true;
            }
            break;
        case Type::Var:    // type variables, X
            set_error("Encountered a type variable, but expected a term.");
            return true;
        case Type::Top:    // largest type
            break;
        case Type::top:     // member of Top
            break;
        case Type::Fn:
        case Type::ForAll:
            break;
        case Type::fn: {    // functions, fn(x:A) b
            Stack *rhs = app;
            if(app != nullptr) {
                app = rhs->next;
            }
            ctxt = new Bind(ctxt, Type::fn, a->name,
                        new Stack(this, a->child[0], true), rhs);
            a = a->child[1];
            continue;
            }
        case Type::fnT: {   // polymorphic function, fn(X<:A)b
            Stack *rhs = app;
            if(app != nullptr) {
                app = rhs->next;
            }
            ctxt = new Bind(ctxt, Type::fnT, a->name,
                        new Stack(this, a->child[0], true), rhs);
            a = a->child[1];
            continue;
            }
        case Type::app:     // application, b(a)
            app = new Stack(this, a->child[1], false, app);
            a = a->child[0];
            continue;
        case Type::appT:          // type application, b(:A)
            app = new Stack(this, a->child[1], true, app);
            a = a->child[0];
            continue;
        case Type::Group:  // grouping, {A}
            break;
        case Type::group:         // grouping, {a}
            set_error("Group not handled.");
            return true;
        case Type::error:
            set_error(a->name);
            return true;
        }
        break;
    }
    // type = type of bottom term in Ast.
    t = a->t;
    if(isType(t)) {
        set_error("Expected term, but found a type.");
        return true;
    }
    // Is this stack a value? return true (no further processing)
    // note: the ctxt check is technically not needed, since
    // any ctxt passing isValue&&app==null is unreachable code.
    return isValue(t) && app == nullptr && ctxt == nullptr;
}

/** Wind the type `a` onto the stack, evaluating completely
 *  to a term.
 *
 *  Returns true on success or false if an error
 *  is encountered.
 */
bool Stack::windType(AstP a) {
    while(true) {
        switch(a->t) {
        case Type::Var:    // type variables, X
            if(a->isPtr) {
                ref = a->ref();
            } else {
                ref = lookup(a->n, true);
            }
            if(ref == nullptr) {
                if(a->name.size() > 0) {
                    set_error(std::string("Unbound variable, ") + a->name);
                } else {
                    set_error("Unbound type variable.");
                }
                return false;
            }
            if(!bindType(ref->t)) {
                set_error("Var refers to a binding construct that does not accept types.");
                return false;
            }
            if(ref->rhs == nullptr) {
                ++ref->nref; // no known substitition for this type var
            } else {
                a = get_ast(ref->rhs); // locally nameless Ast
                continue;
            }
            break;
        case Type::var:    // variables, x
            set_error("Encountered a term variable, but expected a type.");
            return false;
        case Type::Top:    // largest type
            break;
        case Type::top:     // member of Top
            break;
        case Type::Fn: {   // function spaces, A->B
            Stack *rhs = app;
            if(rhs != nullptr) {
                set_error("Invalid application of type (A->B).");
                return false;
            }
            ctxt = new Bind(ctxt, Type::Fn,
                        new Stack(this, a->child[0], true), rhs);
            a = a->child[1];
            continue;
            }
        case Type::ForAll: {  // bounded quantification, All(X<:A) B
            Stack *rhs = app;
            if(rhs != nullptr) {
                set_error("Invalid application of type All(X:<A) B.");
                return false;
            }
            ctxt = new Bind(ctxt, Type::ForAll, a->name,
                        new Stack(this, a->child[0], true), rhs);
            a = a->child[1];
            continue;
            }
        case Type::fn:
        case Type::fnT:
        case Type::app:    // application, b(a)
        case Type::appT:   // type application, b(:A)
            break;
        case Type::Group:  // grouping, {A}
            set_error("Group not handled.");
            return false;
        case Type::group:  // grouping, {a}
            break;
        case Type::error:
            set_error(a->name);
            return false;
        }
        break;
    }
    // type = type of bottom term in Ast.
    t = a->t;
    if(!isType(t)) {
        set_error("Expected type, but found a term.");
        return false;
    }
    return true;
}
