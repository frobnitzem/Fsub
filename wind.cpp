#include <utility>

#include "wind.hpp"

// SFold
struct GetAst {
    AstP ast;

    // Type annotations are ignored.
    bool val(Stack *s) {
        ast = std::make_shared<Ast>(s->t);
        switch(s->t) {
        case Type::Var:
        case Type::var: // re-number variable ref-s
            //if(number_var(s, s->ref)) {
            //    return true;
            //}
            //TODO: renumber using a map
            ast->name = s->ref->name;
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

    AstP get_ast_sub(Stack *s) {
        AstP a = ast;
        unwind(this, s);
        std::swap(a, ast);
        return a;
    }
};

AstP get_ast(Stack *s) {
    struct GetAst h;
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
