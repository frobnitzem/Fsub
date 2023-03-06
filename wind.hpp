#pragma once

#include "ast.hpp"
#include "stack.hpp"

void stack_dtor(Stack *s);
void eval_need(Stack *s);
AstP get_ast(Stack *s);
