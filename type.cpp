#include <stdio.h>
#include <iostream>

#include "ast.hpp"
#include "wind.hpp"

TracebackP subType1(AstP A, AstP B);

/** check that A is a subtype of B
 *
 *  Returns a (unique pointer to) Traceback on error,
 *  or else a nullptr.
 *
 *  Note that both A and B must be completely evaluated
 *  before entry to this function.
 */
TracebackP subType(AstP A, AstP B) {
    std::cout << "Unifying: "; print_ast(A);
    std::cout << " and: "; print_ast(B);
    std::cout << "\n";
    TracebackP err = subType(A, B);
    if(err) {
        std::cout << "Error: " << err->name << std::endl;
    }
    return err;
}

TracebackP subType1(AstP A, AstP B) {
    while(B->t != Type::Top) {
        switch(A->t) {
        case Type::Top:
            // Error: B->t is smaller than A
            // note: due to the while-loop, the following is always false:
            //return B->t == Type::Top;
            return mkError("Top is not a subtype of B");
        case Type::Var:
            if(B->t == Type::Var) {
                if(A->isPtr == B->isPtr && A->n == B->n) {
                    return nullptr;
                }
                return mkError("A refers to a type variable which differs from B.");
            }
            return mkError("A refers to a type variable, but B is a value.");
        case Type::Fn:
        case Type::ForAll:
            if(B->t != A->t) {
                return mkError("A and B bind variables differently (Fn vs. ForAll).");
            }
            if(!subType1(B->child[0], A->child[0])) {
                // A and B have incompatible arguments
                // A's argument must be "wider" than B's
                return mkError("A and B have incompatible arguments (B is too narrow).");
            }
            A = A->child[1];
            B = B->child[1];
            continue;
        default:
            return mkError("A is not a type!");
        }
    }
    return nullptr;
}

// SFold
struct GetType {
    AstP ast;
    Stack *parent;
    GetType(Stack *_parent) : parent(_parent) { }

    // Return the type of the head term
    // Note: Relies on type annotations
    //       to be completely eval-ed already.
    bool val(Stack *s) {
        switch(s->t) {
        case Type::Var:
        case Type::var:
            // Here, we rely on Ast-ptrs to number vars in s->ctxt.
            // We'll need to number these later.
            ast = get_ast(s->ref->rht, s);
            if(s->app != nullptr) {
                // Pending applications -- need to check that
                // ast represents the correct function type
                // and replace ast with the function result
                // while resolving type variable references
                // added by application right-hand sides.
                Stack *ret = new Stack(s);
                bool err = ret->windType(ast, s->app);
                // TODO: error checking.
                ast = get_ast(ret, s);
                // TODO: skip ref-count increment + decrement
                //       during this operation?
                stack_dtor(ret);
            }
            break;
        case Type::error:
            ast = std::make_shared<Ast>(s->t);
            ast->name = s->err;
            break;
        case Type::Top:
        case Type::top:
            ast = Top();
            break;
        default:
            fprintf(stderr, "Invalid stack type: %d\n", (int)s->t);
            break;
        }
        return true;
    }
    void bind(Bind *c) {
        if(c->rhs == nullptr) {
            Type tt = c->t;
            switch(c->t) {
            case Type::fn:
                tt = Type::Fn;
                break;
            case Type::fnT:
                tt = Type::ForAll;
                break;
            case Type::Fn:
            case Type::ForAll:
                ast = Top();
                return;
            default:
                fprintf(stderr, "Invalid bind type (%d).\n", (int)c->t);
                return;
            }
            // Note: still relying on ptrs to number vars in s->ctxt
            ast = std::make_shared<Ast>(
                        tt, c->name, get_ast(c->rht), ast);
        }
    }
    void apply(Stack *b) {
        // ignore (already dealt with)
    }
};

/** Create a "locally nameless" Ast for the type of the given stack.
 *  Varibles defined within the stack are replaced by de-Bruijn
 *  indices.  Variables external to the stack are left as
 *  pointers to Bind-s.
 */
AstP get_type(Stack *s) {
    struct GetType h(s->parent);
    unwind(&h, s);
    return h.ast;
}
