#include "ast.hpp"
#include "stack.hpp"

/** Look up the variable binding in this stack.
 *  If incr is true, increments the binding's reference
 *  count.
 *
 *  Note: Use incr if you intend to add this reference
 *  somewhere in the tree rooted at `this`.
 */
Bind *Stack::lookup(const std::string &name, bool incr) {
    for(Stack *s = this; s != nullptr; s = s->parent) {
        for(Bind *x = s->ctxt; x != nullptr; x=x->next) {
            if(x->name == name) {
                if(incr) {
                    ++x->nref;
                }
                return x;
            }
        }
    }
    return nullptr;
}

Stack::Stack(Stack *_p, AstP a, bool isT, Stack *_next)
        : parent(_p), ctxt(nullptr),
          app(nullptr), next(_next) {
    wind(a, isT);
}

bool Stack::wind(AstP a, bool isT) {
    while(true) {
        switch(a->t) {
        case Type::Var:    // type variables, X
            ref = lookup(a->name);
            if(ref == nullptr) {
                set_error(std::string("Unbound variable, ") + a->name);
                return true;
            }
            break;
        case Type::Top:    // largest type
            break;
        case Type::Fn: {   // function spaces, A->B
            Stack *rhs = app;
            if(rhs != nullptr) {
                set_error("Invalid application of type (A->B).");
                return true;
            }
            ctxt = new Bind(ctxt, Type::Fn,
                        new Stack(this, a->child[0], true), rhs);
            a = a->child[1];
            continue;
            }
        case Type::ForAll: {  // bounded quantification, All(X<:A) B
            Stack *rhs = app;
            if(rhs != nullptr) {
                set_error("Invalid application of type (A->B).");
                return true;
            }
            ctxt = new Bind(ctxt, Type::ForAll, a->name,
                        new Stack(this, a->child[0], true), rhs);
            a = a->child[1];
            continue;
            }
        case Type::Group:  // grouping, {A}
            set_error("Group not handled.");
            break;
        case Type::var:     // variables, x
            ref = lookup(a->name);
            if(ref == nullptr) {
                set_error(std::string("Unbound variable, ") + a->name);
                return true;
            }
            if(isType(ref->t)) {
                set_error("RHS of var is a type.");
                return true;
            }
            break;
        case Type::top:     // member of Top
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
        case Type::app:     // application, b(a)
            app = new Stack(this, a->child[1], false, app);
            a = a->child[0];
            continue;
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
        case Type::appT:          // type application, b(:A)
            app = new Stack(this, a->child[1], true, app);
            a = a->child[0];
            continue;
        case Type::group:         // grouping, {a}
            set_error("Group not handled.");
            return true;
        case Type::error:
            set_error(a->name);
            break;
        }
        break;
    }
    // type = type of bottom term in Ast.
    t = a->t;
    if(isT ^ isType(t)) {
        if(isT) {
            set_error("Expected type, but found a term.");
        } else {
            set_error("Expected term, but found a type.");
        }
        return true;
    }
    // Is this stack a value? return true (no further processing)
    // note: the ctxt check is technically not needed, since
    // any ctxt passing isValue&&app==null is unreachable code.
    return isValue(t) && app == nullptr && ctxt == nullptr;
}
