#include <stdio.h>
#include <iostream>
#include <map>

#include "ast.hpp"
#include "unwind.hpp"

TracebackP subType1(AstP A, AstP B);

void replaceVars(AstP a, const std::map<intptr_t,int> &map, int ndown);
// Replaces pointers with numbers.
// the map is from [Bind *] to [depth to the term's root]
struct windReplaceVars {
    const std::map<intptr_t,int> &map;
    int ndown;
    windReplaceVars(const std::map<intptr_t,int> &_map, int _ndown)
        : map(_map), ndown(_ndown) {}

    AstP val(AstP a) {
        if(a->isPtr) {
            auto it = map.find(a->n);
            if(it != map.end()) {
                a->isPtr = false;
                a->n = ndown + it->second;
            }
        }
        return nullptr;
    }
    AstP bind(AstP a) {
        replaceVars(a->child[0], map, ndown);
        ++ndown;
        return a->child[1];
    }
    AstP apply(AstP a) {
        replaceVars(a->child[1], map, ndown);
        return a->child[0];
    }
};
// Replace local variables with de-Bruijn indices
// Note: this breaks convention and replaces vars in-place.
void replaceVars(AstP a, const std::map<intptr_t,int> &map, int ndown) {
    windReplaceVars W(map, ndown);
    wind(&W, a);
}

/** check that A is a subtype of B
 *
 *  Returns a (unique pointer to) Traceback on error,
 *  or else a nullptr.
 *
 *  Note that both A and B must be completely evaluated
 *  before entry to this function.
 */
TracebackP subType(AstP A, AstP B) {
    std::cout << "Checking: "; print_ast(A, 7);
    std::cout << "\n  <: "; print_ast(B, 7);
    std::cout << "\n";
    TracebackP err = subType1(A, B);
    if(err) {
        std::cout << "Error: " << err->name << std::endl;
    }
    return err;
}

TracebackP subType1(AstP A, AstP B) {
    TracebackP err;
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
            err = subType1(B->child[0], A->child[0]);
            if(err) {
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
// TODO: create a new stack (representing the type),
// create an application AstP to represent the right-hand sides,
// then windType() onto it to evaluate the type.
struct GetType {
    AstP ast;
    int nbind = 0;
    bool err;
    std::map<intptr_t,int> map;

    bool val(Stack *s) {
        switch(s->t) {
        case Type::Var:
        case Type::var:
            // Here, we rely on Ast-ptrs to number vars in s->ctxt.
            // We'll need to number these later.
            ast = get_ast(s->ref->rht);
            if(s->app != nullptr) {
                // Pending applications -- need to check that
                // ast represents the correct function type
                // and replace ast with the function result
                // while resolving type variable references
                // added by application right-hand sides.
                Stack *ret = new Stack(s);
                err = ret->windType(ast, s->app);
                // remove intermediate let-bindings.
                // alternately, walk ret->ctxt
                eval_need(ret);
                ast = get_ast(ret);
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
            // Note: These rhs type annotations still rely
            // on ptrs to number vars within s->ctxt.
            ast = std::make_shared<Ast>(
                        tt, c->name, get_ast(c->rht), ast);
            map[(intptr_t)c] = nbind++;
        }
    }
    void apply(Stack *b) {
        // ignore (already dealt with)
    }

    /** Change map from counting binder height above head
     *  term mapping depth to term's root (current nbind).
     *
     *  Then run replaceVars on the current ast.
     *  
     *  example final state of the above unwind op:
     *  nbind = 3
     *  map = BindC |-> 2
     *        BindB |-> 1
     *              [ hypothetical renumber of BindC ]
     *        BindA |-> 0
     *
     *  For the hypothetical renumbering, the
     *  de-Bruijn index is computed, given
     *    ndown = current number of binders
     *    ndown@C = nbind - map[BindC] == 1
     *    ------------------------, as
     *    answer = ndown - ndown@C
     *           = map[BindC] + (ndown-nbind)
     */
    void replace() {
        replaceVars(ast, map, -nbind);
    }
};

/** Create a "locally nameless" Ast for the type of the given stack.
 *  Varibles defined within the stack are replaced by de-Bruijn
 *  indices.  Variables external to the stack are left as
 *  pointers to Bind-s.
 */
AstP get_type(Stack *s) {
    struct GetType h;
    unwind(&h, s);
    h.replace();
    return h.ast;
}
