#include <utility>

#include "wind.hpp"

// SFold
struct GetAst {
    AstP ast;
    Stack *parent;
    GetAst(Stack *_parent) : parent(_parent) { }

    bool val(Stack *s) {
        ast = std::make_shared<Ast>(s->t);
        switch(s->t) {
        case Type::Var:
        case Type::var: // re-number variable ref-s
            ast->isPtr = s->number_var(&ast->n, s->ref, parent);
            break;
        case Type::error:
            ast->name = s->err;
            break;
        case Type::top:
        case Type::Top:
            break;
        default:
            printf("Invalid stack type: %d\n", (int)s->t);
            break;
        }
        return true;
    }
    void bind(Bind *c) {
        ast = std::make_shared<Ast>(
                    c->t, c->name,
                         get_ast_sub(c->rht), ast);
        if(c->rhs != nullptr) {
            apply(c->rhs);
        }
    }
    void apply(Stack *b) {
        // assume b's type-ness marker is correct
        if(isType(b->t)) {
            ast = appT(ast, get_ast_sub(b));
        } else {
            ast = app(ast, get_ast_sub(b));
        }
    }

    /** Turn a sub-tree into an Ast.  This retains
     *  the same parent stack as before, so the sub-tree
     *  remains locally nameless with no change in scoping.
     */
    AstP get_ast_sub(Stack *s) {
        return get_ast(s, parent);
    }
};

/** Create a "locally nameless" Ast for the given stack.
 *  Varibles defined within the stack are replaced by de-Bruijn
 *  indices.  Variables external to the stack are left as
 *  pointers to Bind-s.
 */
AstP get_ast(Stack *s) {
    return get_ast(s, s->parent);
}

/** Create an ast that is nameless for bindings in
 *  "parent", but uses de-Bruijn indices otherwise.
 */
AstP get_ast(Stack *s, Stack *parent) {
    struct GetAst h(parent);
    unwind(&h, s);
    return h.ast;
}

/* Garbage collection fold. */
struct StackDtor {
    Stack *spine;
    StackDtor(Stack *_spine) : spine(_spine) {}

    // Mark references as deleted.
    bool val(Stack *s) {
        switch(s->t) {
        case Type::Var:
        case Type::var:
            --s->ref->nref;
            break;
        default:
            break;
        }
        return true;
    }

    void bind(Bind *c) {
        if(c->rht != nullptr)
            stack_dtor(c->rht);
        if(c->rhs != nullptr)
            stack_dtor(c->rhs);
        spine->ctxt = c->next; // remove binder from context
        delete c;
    }

    void apply(Stack *rhs) {
        spine->app = spine->app->next; // remove application from @-stack.
        stack_dtor(rhs);
    }
};

void stack_dtor(Stack *s) {
    struct StackDtor dtor(s);
    unwind(&dtor, s);
    delete s;
}
