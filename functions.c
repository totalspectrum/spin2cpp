/*
 * code for handling functions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

Function *
NewFunction(void)
{
    Function *f;
    Function *pf;

    f = calloc(1, sizeof(*f));
    if (!f) {
        fprintf(stderr, "FATAL ERROR: Out of memory!\n");
        exit(1);
    }
    /* now link it into the current object */
    if (current->functions == NULL) {
        current->functions = f;
    } else {
        pf = current->functions;
        while (pf->next != NULL)
            pf = pf->next;
        pf->next = f;
    }
    return f;
}

void
DeclareFunction(int is_public, AST *funcdef, AST *body)
{
    const char *name;
    Function *fdef;
    AST *vars;

    if (funcdef->kind != AST_FUNCDEF || funcdef->left->kind != AST_FUNCDECL) {
        ERROR("Internal error: bad function definition");
        return;
    }
    if (funcdef->left->left->kind != AST_IDENTIFIER) {
        ERROR("Internal error: no function name");
        return;
    }
    name = funcdef->left->left->d.string;

    fdef = NewFunction();
    fdef->is_public = is_public;
    fdef->type = ast_type_long;
    fdef->name = name;

    vars = funcdef->right;
    if (vars->kind != AST_FUNCVARS) {
        ERROR("Internal error: bad variable declaration");
    }
}
