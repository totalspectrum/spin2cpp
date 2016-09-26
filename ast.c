/*
 * functions for dealing with Abstract Syntax Trees
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

AST *
NewAST(enum astkind kind, AST *left, AST *right)
{
    AST *ast;

    ast = (AST *)malloc(sizeof(*ast));
    if (!ast) {
        fprintf(stderr, "FATAL ERROR: out of memory\n");
        abort();
    }
    ast->kind = kind;
    ast->left = left;
    ast->right = right;
    if (current) {
        ast->fileName = current->L.fileName;
        ast->line = current->L.lineCounter;
    } else {
        ast->fileName = "<unknown>";
        ast->line = 0;
    }
    return ast;
}

/*
 * add an AST to a list
 * the "right" pointers link the list, the "left" pointers are free
 * to hold other data
 * returns a pointer to the head of the list
 */
AST *
AddToList(AST *list, AST *newelement)
{
    AST *p;
    if (!list)
        return newelement;
    if (!newelement)
        return list;
    for (p = list; p->right != NULL; p = p->right)
        ;
    p->right = newelement;
    return list;
}

/*
 * duplicate an AST
 */
AST *
DupAST(AST *orig)
{
    AST *dup;

    if (!orig)
        return NULL;

    dup = NewAST(orig->kind, NULL, NULL);
    memcpy(dup, orig, sizeof(*dup));
    dup->left = DupAST(orig->left);
    dup->right = DupAST(orig->right);
    return dup;
}

/*
 * duplicate an AST replacing "orig" with "replace"
 */
AST *
DupASTWithReplace(AST *ast, AST *orig, AST *replace)
{
    AST *dup;

    if (!ast)
        return NULL;

    dup = NewAST(ast->kind, NULL, NULL);
    memcpy(dup, ast, sizeof(*dup));
    if (AstMatch(ast->left, orig)) {
        dup->left = DupAST(replace);
    } else {
        dup->left = DupASTWithReplace(ast->left, orig, replace);
    }
    if (AstMatch(ast->right, orig)) {
        dup->right = DupAST(replace);
    } else {
        dup->right = DupASTWithReplace(ast->right, orig, replace);
    }
    return dup;
}

/* see if two trees match */
int
AstMatch(AST *a, AST *b)
{
    if (a == NULL)
        return b == NULL;
    if (b == NULL)
        return 0;
    if (a->kind != b->kind)
        return 0;
    switch (a->kind) {
    case AST_HWREG:
        return a->d.ptr == b->d.ptr;
    case AST_INTEGER:
        return a->d.ival == b->d.ival;
    case AST_STRING:
    case AST_IDENTIFIER:
        return strcasecmp(a->d.string, b->d.string) == 0;
    case AST_OPERATOR:
    case AST_ASSIGN:
        if (a->d.ival != b->d.ival)
            return 0;
        break;
    default:
        break;
    }
    return AstMatch(a->left, b->left) && AstMatch(a->right, b->right);
}

/* see if b is a subtree of a */
int
AstUses(AST *a, AST *b)
{
    if (!b) return 1;
    if (!a) return 0;
    if (AstMatch(a, b)) return 1;
    return AstMatch(a->left, b) || AstMatch(a->right, b);
}

/* create an integer */
AST *
AstInteger(long ival)
{
    AST *ast = NewAST(AST_INTEGER, NULL, NULL);
    ast->d.ival = ival;
    return ast;
}

/* create an identifier */
AST *
AstIdentifier(const char *name)
{
    AST *ast = NewAST(AST_IDENTIFIER, NULL, NULL);
    ast->d.string = name;
    return ast;
}
/*
 * create an instruction modifier with a specific bit pattern
 */
AST *
AstInstrModifier(int32_t ival)
{
    static InstrModifier Imm = {
        "#", IMMEDIATE_INSTR
    };
    static InstrModifier BigImm = {
        "##", BIGIMM_INSTR
    };
    AST *ast = NewAST(AST_INSTRMODIFIER, NULL, NULL);

    if (ival == Imm.modifier) {
        ast->d.ptr = (void *)&Imm;
    } else if (ival == BigImm.modifier) {
        ast->d.ptr = (void *)&BigImm;
    } else {
        ERROR(NULL, "Unknown instruction modifier");
        return NULL;
    }
    return ast;
}

/*
 * create an operator
 */
AST *
AstOperator(int32_t ival, AST *left, AST *right)
{
    AST *ast = NewAST(AST_OPERATOR, left, right);
    ast->d.ival = ival;
    return ast;
}

AST *
AstAssign(int32_t ival, AST *left, AST *right)
{
    AST *ast;
    AST *rhs;

    if (ival == T_ASSIGN)
        rhs = right;
    else
        rhs = AstOperator(ival, left, right);

    ast = NewAST(AST_ASSIGN, left, rhs);
    ast->d.ival = ival;
    return ast;
}

AST *
AstTempVariable(const char *prefix)
{
    char *name;
    AST *ast = NewAST(AST_IDENTIFIER, NULL, NULL);

    name = NewTemporaryVariable(prefix);
    AddSymbol(&current->objsyms, name, SYM_TEMPVAR, (void *)ast_type_long);
    ast->d.string = name;
    return ast;
}


/*
 * create a lookup expression for LOOKUP, LOOKUPZ, etc.
 * "kind" is either AST_LOOKUP or AST_LOOKDOWN, "base"
 * is 0 or 1 (for LOOKUPZ, LOOKUP respectively),
 * expr is the index into the table,
 * table is the array to lookup (lookdown) in
 */

AST *
AstLookup(enum astkind kind, int base, AST *expr, AST *table)
{
    AST *ev;
    ev = NewAST(AST_LOOKEXPR, AstInteger(base), expr);
    return NewAST(kind, ev, table);
}

int
AstListLen(AST *list)
{
    int val = 0;
    while (list) {
        val++;
        list = list->right;
    }
    return val;
}


void
RemoveFromList(AST **listptr, AST *elem)
{
    AST *next;
    AST *cur;

    next = *listptr;
    for(;;) {
        cur = next;
        if (!cur) return;
        if (cur == elem) {
            *listptr = elem->right;
            elem->right = 0;
            return;
        }
        listptr = &cur->right;
        next = cur->right;
    }
}

void AstReportAs(AST *old)
{
    if (current && old) {
        current->L.fileName = old->fileName;
        current->L.lineCounter = old->line;
    }
}

static const char *astnames[] = {
    "unknown",
    "listholder",
    "integer",
    "string",
    
    "identifier",
    "operator",
    "float",
    "assign",
    
    "enumset",
    "arraydecl",
    "bytelist",
    "wordlist",
    
    "longlist",
    "inttype",
    "unsignedtype",
    "arraytype",

    "funcdecl",
    "funcdef",
    "funcvars",
    "stmtlist",

    "instr",
    "hwreg",
    "return",
    "if",
    
    "thenelse",
    "range",
    "rangeref",
    "funccall",

    "exprlist",
    "instrholder",
    "instrmodifier",
    "org",

    "here",
    "posteffect",
    "while",
    "dowhile",

    "for",
    "memref",
    "arrayref",
    "countrepeat",

    "case",
    "caseitem",
    "other",
    "res",

    "from",
    "to",
    "step",
    "fit",

    "addrof",
    "lookup",
    "lookdown",
    "object",

    "methodref",
    "constref",
    "objdecl",
    "stringptr",

    "yield",
    "constant",
    "quit",
    "next",

    "result",
    "round",
    "trunc",
    "tofloat",

    "file",
    "abort",
    "catch",
    "lookexpr",

    "dataddrof",
    "annotation",
    "temparraydecl",
    "temparrayuse",

    "pubfunc",
    "prifunc",
    "funcholder",
    "enumskip",

    "linebreak",
    "comment",
    "commentednode",
    "coginit",

    "sprref",
    "absaddrof",
    "floattype",
    "ptrtype",

    "generictype",
    "voidtype",
    "sequence",
    "condresult",
};

//
// AST debug
//
static void doASTDump(AST *ast, int indent)
{
    char buf[128];
    int leaf = 0;
    unsigned idx;
    const char *astname = "ast";
    
    if (!ast) return;
    switch (ast->kind) {
    case AST_STMTLIST:
        printf("%*c<stmtlist>\n", indent, ' ');
        while (ast && ast->kind == AST_STMTLIST) {
            doASTDump(ast->left, indent+2);
            ast = ast->right;
        }
        if (ast) {
            printf("%*c<MALFORMED>\n", indent, ' ');
            doASTDump(ast, indent+2);
            printf("%*c</MALFORMED>\n", indent, ' ');
        }
        printf("%*c</stmtlist>\n", indent, ' ');
        return;
    case AST_IDENTIFIER:
        sprintf(buf, "<identifier %s/>", ast->d.string);
        leaf = 1;
        break;
    case AST_INTEGER:
        sprintf(buf, "<integer %d/>", ast->d.ival);
        leaf = 1;
        break;
    case AST_OPERATOR:
        if (ast->d.ival >= 32 && ast->d.ival <= 126) {
            sprintf(buf, "<operator '%c'>", ast->d.ival);
        } else {
            sprintf(buf, "<operator #%d>", ast->d.ival);
        }
        astname = "operator";
        break;
    default:
        idx = (unsigned int)ast->kind;
        if (idx < sizeof(astnames) / sizeof(astnames[0])) {
            astname = astnames[idx];
            sprintf(buf, "<%s>", astname);
        } else {
            sprintf(buf, "<ast #%d>", ast->kind);
        }
        break;
    }
    printf("%*c", indent, ' ');
    printf("%s\n", buf);
    if (!leaf) {
        doASTDump(ast->left, indent+2);
        doASTDump(ast->right, indent+2);
        printf("%*c</%s>\n", indent, ' ', astname);
    }
}

void
DumpAST(AST *ast)
{
    doASTDump(ast, 0);
}

