# Fsub

This project implements F<: by following
Luca Cardelli's "An implementation of F<:"
SRC Research Report, 1993 (revised 1997).
And using the wind/unwind conversion to stack form
idea from Rogers, "Towards a Direct, By-Need Evaluator
for Dependently Typed Languages" (2015),
https://arxiv.org/pdf/1509.07036.pdf.

The central idea is that traversing an Ast
from top to bottom is a "winding" process, which
shifts lambda-s into Bind-s in the environment,
and which shifts apply-s into a "pending application stack".
The winding process naturally occurs as the constructor
of a Stack.  During this process, all variable
references (which were de-Bruijn indices in the Ast)
are resolved to pointers to their corresponding
Bind structures.  Binds are ref-counted, so that
garbage collection is easy.

## Memory allocation tracking

Memory leaks are avoided by strictly adhering
to a convention for pointer ownership.

Ast-s are always stored in `AstP = std::shared_ptr<Ast>`
objects.  This allows refernence counting to manage
de-allocation.  Ast-s may even share sub-trees (forming a DAG).
Ast-s cannot be cyclic.  Instead, they express cyclic
control flow via let-rec bindings.

Stacks follow a different convention.  Every Stack is uniquely
pointed to by only one parent.  Parent to child links
are stored either as `Bind->rhs`, `Bind->rht`,
or `Stack->app` or `Stack->next`.
Note that `Stack->next` is not active unless following
a valid reference chain of the form, `Stack->app[->next]`.

There are also child to parent links `Stack->parent`,
which are used to traverse the context upward.
These are a convenience and could be
removed at a future point (re-calculate with a top-\>down sweep).

## Wind operation

The winding operation takes an Ast and turns it
into a `final` Stack form.

Note there are different tasks for which the
top-down "winding" traversal of Ast-s is useful.
A generic wind example is below.

    struct headTerm {
        Type ans;
        AstP val(AstP a) {
            ans = a->t;
            return nullptr;
        }
        AstP bind(AstP a) {
            return a->child[1];
        }
        AstP apply(AstP a) {
            return a->child[0];
        }
    };

This one just traverses the Ast until it finds
the head term.  Sets that as the stack's type
and returns.

Eager evaluation would perform substitution
inside val(), while lazy evaluation would stop
at val and store a pointer.

`windType` is another kind of winding process that
has all its actions specialized to the case where it's
evaluating an Ast representing a type.  It also
does eager evaluation.  The result of windType is always
a completely evaluated type or a detection of a type
error in the term.

The `windType` operation is re-used for deducing a function
return type.  In this case, windType operates on an
Ast representing the function's type, and keeps track
of pending applications on the side.  It examines them
when it reaches a binding construct in the type.


## Unwind operation

Once a term has been wound onto a Stack, it is simple
to traverse it from bottom to top again.  This
reverse process, called unwind(), can do many different
things:

* free all stacks (GC)
* form an Ast
* eval by-need


# TODO

- [ ] Implement type checking.
- [ ] Implement unification for inferring arguments to fnT.
- [ ] Optimize substitution of values in
      need_var when nref == 1.  In this case, the
      copy can be changed to a move.  The difficulty
      comes from cases where the right-hand side contains
      bindings and/or applications, since these
      have to be added correctly to the parent stack.
- [ ] Implement group-s in stack form.
- [ ] Add an 'Elem' operator to lookup elements from groups.
