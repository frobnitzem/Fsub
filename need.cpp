#include <stdio.h>

#include "ast.hpp"
#include "stack.hpp"

#include "unwind.hpp"

void eval_need(Stack *s);

void relink_ctxt(Stack *s, Bind *old, Bind *next) {
    for(Bind **x = &s->ctxt; *x != nullptr; x=&(*x)->next) {
        if(*x == old) {
            *x = next;
            return;
        }
    }
    fprintf(stderr, "Error: old binding not found!\n");
}

/* Fold doing full eval and removing all let-binders. */
struct EvalNeed {
    Stack *spine;

    EvalNeed(Stack *_spine) : spine(_spine) {}

    bool need_var(Stack *s) {
        Bind *ref = s->ref;
        if(ref->rhs == nullptr)
            return true;
        eval_need(ref->rhs);

        AstP rhs = get_ast(ref->rhs); // locally nameless Ast
        --ref->nref;
        ErrorList E; // FIXME: these should not throw in a properly typed term
        if(bindType(ref->t)) { // note: this is a non-term
            s->windType(E, rhs);
            return true;
        }
        s->wind(E, rhs);
        return s->isTrivial();
    }

    // Fully evaluate nested data structures.
    // Returns true when done, or false
    // if val needs to be called again.
    bool val(Stack *s) {
        switch(s->t) {
        case Type::Var:
        case Type::var:
            //if(is_open(s) && s->ref->rec) // refuse
            //    return true;
            return need_var(s);
        default: // Top / top / error
            break;
        }
        return true;
    }

    // Remove the binding if unused.
    void bind(Bind *c) {
        if(c->rhs == nullptr || c->nref > 0) { // keep
            /*if(c->rht != nullptr) {
                eval_need(c->rht);
            } else {
                fprintf(stderr, "NULL type found in ctxt!\n");
            }*/
        } else { // variable is unused! -- discard Binding c
            if(c->nref < 0) {
                fprintf(stderr, "%s has %d refs??\n", c->name.c_str(), c->nref);
            }
            if(c->rht != nullptr)
                stack_dtor(c->rht);
            stack_dtor(c->rhs);
            relink_ctxt(spine, c, c->next);
            delete c;
        }
    }

    void apply(Stack *b) {
        eval_need(b);
    }
};

void eval_need(Stack *s) {
    EvalNeed need(s);
    unwind(&need, s);
}
