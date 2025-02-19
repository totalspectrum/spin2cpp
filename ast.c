/*
 * functions for dealing with Abstract Syntax Trees
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "spinc.h"

static LexStream *s_reportas_lexdata;
static int s_reportas_lineidx;

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
    ast->d.ival = 0;
    if (s_reportas_lexdata) {
        ast->lexdata = s_reportas_lexdata;
        ast->lineidx = s_reportas_lineidx;
    } else if (current && current->Lptr) {
        ast->lexdata = current->Lptr;
        ast->lineidx = getLineInfoIndex(ast->lexdata);
    } else {
        ast->lexdata = NULL;
        ast->lineidx = 0;
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
 * append two lists
 */
AST *
AppendList(AST *first, AST *last)
{
    AST *orig = first;
    if (!first) {
        return last;
    }
    while (first->right) first = first->right;
    first->right = last;
    return orig;
}

/*
 * add an AST to a "left" list
 * this is one where everything is on the left pointers,
 * e.g. a list of type modifiers
 */
AST *
AddToLeftList(AST *list, AST *newelement)
{
    AST *p;
    if (!list)
        return newelement;
    if (!newelement)
        return list;
    for (p = list; p->left != NULL; p = p->left)
        ;
    p->left = newelement;
    return list;
}
/* accelerator for AddToList; keeps track of tail */
AST *AddToListEx(AST *head, AST *newelem, AST **tailptr)
{
    AST *tail;

    if (!head)
        return newelem;
    if (!newelem)
        return head;
    tail = *tailptr;
    if (tail) {
        AddToList(tail, newelem);
    } else {
        AddToList(head, newelem);
    }
    *tailptr = newelem;
    return head;
}

/*
 * insert "newelem" in a list, before "member"
 */
AST *
ListInsertBefore(AST *list, AST *member, AST *newelem)
{
    AST *orig = list;
    if (!list || list == member) {
        newelem->right = list;
        return newelem;
    }
    while (list && list->right != member) {
        list = list->right;
    }
    if (!list) {
        ERROR(member, "Internal error, unable to find item in list");
        return orig;
    }
    newelem->right = list->right;
    list->right = newelem;
    return orig;
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
 * duplicate an AST, preserving BASIC string type
 */
AST *
DupASTTypeSafe(AST *orig)
{
    AST *dup;

    if (!orig)
        return NULL;
    if (orig == ast_type_string)
        return orig;
    dup = NewAST(orig->kind, NULL, NULL);
    memcpy(dup, orig, sizeof(*dup));
    dup->left = DupASTTypeSafe(orig->left);
    dup->right = DupASTTypeSafe(orig->right);
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

static AST *
skipCommentsAndDeclares(AST *a)
{
    while (a && a->kind == AST_STMTLIST) {
        if (!a->left) {
            break;
        }
        if (a->left->kind == AST_COMMENT) {
            /* just skip */
        } else if (a->left->kind == AST_DECLARE_VAR) {
            /* just skip */
        } else {
            break;
        }
        a = a->right;
    }
    return a;
}


/* see if two trees match */
/* if "ignoreStatic" is true, ignore local identifier differences */

static int
doAstMatch(AST *a, AST *b, int ignoreStatic)
{
    if (ignoreStatic) {
        a = skipCommentsAndDeclares(a);
        b = skipCommentsAndDeclares(b);
    }
    if (a == NULL && b == NULL) {
        return 1;
    }
    if (a == NULL || b == NULL) {
        return 0;
    }
    if (a == b) {
        return 1;
    }

    if (a->kind != b->kind) {
        if (!ignoreStatic) return 0;
        if (a->kind == AST_SEQUENCE && !a->right) {
            return doAstMatch(a->left, b, 1);
        }
        if (b->kind == AST_SEQUENCE && !b->right) {
            return doAstMatch(a, b->left, 1);
        }
        return 0;
    }
    switch (a->kind) {
    case AST_HWREG:
        return a->d.ptr == b->d.ptr;
    case AST_INTEGER:
    case AST_FLOAT:
        return a->d.ival == b->d.ival;
    case AST_STRING:
        return strcmp(a->d.string, b->d.string) == 0;
    case AST_IDENTIFIER:
        if ( (curfunc && LangCaseSensitive(curfunc->language))
                || (current && LangCaseSensitive(current->curLanguage)) )
        {
            return strcmp(a->d.string, b->d.string) == 0;
        }
        return strcasecmp(a->d.string, b->d.string) == 0;
    case AST_LOCAL_IDENTIFIER:
        if (ignoreStatic) {
            return doAstMatch(a->right, b->right, ignoreStatic);
        }
        break;
    case AST_OPERATOR:
    case AST_ASSIGN:
        if (a->d.ival != b->d.ival)
            return 0;
        break;
    default:
        break;
    }
    return doAstMatch(a->left, b->left, ignoreStatic) && doAstMatch(a->right, b->right, ignoreStatic);
}

int
AstMatch(AST *a, AST *b)
{
    return doAstMatch(a, b, 0);
}

int
AstBodyMatch(AST *a, AST *b)
{
    return doAstMatch(a, b, 1);
}
/* see if b is a subtree of a */
int
AstUses(AST *a, AST *b)
{
    if (!b) return 1;
    if (!a) return 0;
    if (AstMatch(a, b)) return 1;
    return AstUses(a->left, b) || AstUses(a->right, b);
}

/* helper; see if a LHS assignment modifies id */
static int
AstModifiesIdentifierLHS(AST *body, AST *id)
{
    if (!body) return 0;
    if (body->kind == AST_IDENTIFIER) {
        if (!strcasecmp(id->d.string, body->d.string)) {
            return 1;
        }
    } else if (body->kind == AST_ARRAYREF) {
        return AstModifiesIdentifierLHS(body->left, id)
               || AstModifiesIdentifier(body->right, id);
    }
    if (AstModifiesIdentifierLHS(body->left, id) || AstModifiesIdentifierLHS(body->right, id)) {
        return 1;
    }
    // FIXME should we handle memory references here?
    return 0;
}

/* see if a changes the identifier b */
/* be conservative here */
int
AstModifiesIdentifier(AST *body, AST *id)
{
    while (body) {
        if (body->kind == AST_ASSIGN) {
            if (AstModifiesIdentifierLHS(body->left, id))
                return 1;
            /* FIXME: we should handle memory references here */
        } else if (body->kind == AST_OPERATOR) {
            int op = body->d.ival;
            if (op == K_INCREMENT || op == K_DECREMENT) {
                return AstUses(body->left, id) || AstUses(body->right, id);
            }
        } else if (body->kind == AST_ADDROF) {
            /* if we take the address of a variable, assume it will be modified */
            if (AstModifiesIdentifierLHS(body->left, id))
                return 1;
        }
        if (AstModifiesIdentifier(body->left, id))
            return 1;
        body = body->right;
    }
    return 0;
}

/* create an integer */
AST *
AstInteger(int64_t ival)
{
    AST *ast = NewAST(AST_INTEGER, NULL, NULL);
    ast->d.ival = ival;
    return ast;
}

/* create an untyped bitvalue */
AST *
AstBitValue(int64_t ival)
{
    AST *ast = NewAST(AST_BITVALUE, NULL, NULL);
    ast->d.ival = ival;
    return ast;
}

/* create a floating point value */
AST *
AstFloat(float f)
{
    AST *ast = NewAST(AST_FLOAT, NULL, NULL);
    ast->d.ival = floatAsInt(f);
    return ast;
}

/* create an AST_SYMBOL */
AST *
AstSymbol(void *sym)
{
    AST *ast = NewAST(AST_SYMBOL, NULL, NULL);
    ast->d.ptr = sym;
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
/* create a string literal */
AST *
AstStringPtr(const char *name)
{
    AST *ast = NewAST(AST_STRING, NULL, NULL);
    ast->d.string = name;
    return NewAST(AST_STRINGPTR, ast, NULL);
}

/* a literal string without STRINGPTR wrapper */
AST *
AstPlainString(const char *name)
{
    AST *ast = NewAST(AST_STRING, NULL, NULL);
    ast->d.string = name;
    return ast;
}

/* get a string from an AST */
const char *
GetStringFromAst(AST *ast)
{
    if (ast && ast->kind == AST_STRINGPTR)
        ast = ast->left;
    if (!ast || ast->kind != AST_STRING)
        return "unknown";
    return ast->d.string;
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
AstOpAssign(int32_t ival, AST *left, AST *right)
{
    AST *ast;

    ast = NewAST(AST_ASSIGN, left, right);
    ast->d.ival = ival;
    return ast;
}

AST *
AstAssign(AST *left, AST *right)
{
    return AstOpAssign(K_ASSIGN, left, right);
}

AST *
AstDeclareLocal(AST *left, AST *right)
{
    AST *ast = NewAST(AST_DECLARE_VAR, left, right);
    return ast;
}

AST *
AstTempIdentifier(const char *prefix)
{
    char *name;

    name = NewTemporaryVariable(prefix, NULL);
    return AstIdentifier(name);
}

AST *
AstTempVariable(const char *prefix)
{
    AST *ident = AstTempIdentifier(prefix);
    const char *name = ident->d.string;
    AddSymbol(&current->objsyms, name, SYM_TEMPVAR, (void *)ast_type_long, name);
    return ident;
}

int
IsAstTempVariable(AST *ident)
{
    if (IsIdentifier(ident)) {
        Symbol *sym = LookupSymbol(GetIdentifierName(ident));
        if (sym && sym->kind == SYM_TEMPVAR) {
            return 1;
        }
    }
    return 0;
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

/* return length of a Spin expression like STRING("hello", 1, 2) */
int
AstStringLen(AST *list)
{
    int len = 1;
    if (!list) return 0;
    if (list->kind == AST_STRINGPTR) {
        list = list->left;
    }
    while (list && list->kind == AST_EXPRLIST) {
        if (!list->left) continue;
        if (list->left->kind == AST_STRING) {
            len += strlen(list->left->d.string);
        } else {
            len += 1;
        }
        list = list->right;
    }
    if (list && list->kind == AST_STRING) {
        len += strlen(list->d.string);
    }
    return len;
}

static char *
CopyString(char *p, AST *list)
{
    list = list->left;
    while (list && list->kind == AST_EXPRLIST) {
        if (!list->left) continue;
        if (list->left->kind == AST_STRING) {
            strcpy(p, list->left->d.string);
            p += strlen(list->left->d.string);
        } else {
            *p++ = list->left->d.ival;
        }
        list = list->right;
    }
    if (list && list->kind == AST_STRING) {
        strcpy(p, list->d.string);
        p += strlen(list->d.string);
    }
    return p;
}

/* merge two AST string pointers */
AST *
AstMergeStrings(AST *left, AST *right)
{
    int newLen = 1;
    if (left) {
        if (left->kind != AST_STRINGPTR) {
            ERROR(left, "Internal errors, expected string");
            return left;
        }
        if (left->d.ival != 0) {
            ERROR(left, "Internal error, expected zstring not lstring");
            return left;
        }
        newLen += AstStringLen(left);
    }
    if (right) {
        if (right->kind != AST_STRINGPTR) {
            ERROR(right, "Internal errors, expected string");
            return left;
        }
        if (right->d.ival != 0) {
            ERROR(right, "Internal error, expected zstring not lstring");
            return right;
        }
        newLen += AstStringLen(right);
    }
    char *newBuf = (char *)malloc(newLen);
    char *p = newBuf;
    if (left) {
        p = CopyString(p, left);
    }
    if (right) {
        p = CopyString(p, right);
    }
    return AstStringPtr(newBuf);
}

/* return length of an AST list; data is on left, ptr to next on right */
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

void AstReportAs(AST *old, ASTReportInfo *save)
{
    if (save) {
        save->lexdata = s_reportas_lexdata;
        save->lineidx = s_reportas_lineidx;
    }
    if (old) {
        s_reportas_lexdata = old->lexdata;
        s_reportas_lineidx = old->lineidx;
    } else {
        s_reportas_lexdata = NULL;
    }
}

void AstReportDone(ASTReportInfo *save)
{
    if (save) {
        s_reportas_lexdata = save->lexdata;
        s_reportas_lineidx = save->lineidx;
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
    "quitloop",
    "continue",

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

    "foratleastonce",
    "isbetween",
    "inlineasm",
    "operand",

    "orgh",
    "maskmove",
    "const_modifier",
    "volatile_modifier",

    "immholder",
    "bigimmholder",
    "align",
    "orgf",

    "tupletype",
    "srccomment",
    "declare_var",
    "declare_var_weak",

    "label",
    "goto",
    "print",
    "char",

    "regpair",
    "functype",
    "self",
    "bitvalue",

    "new",
    "delete",
    "using",
    "globalvars",

    "lambda",
    "setjmp",
    "tryenv",
    "catchresult",

    "sizeof",
    "cast",
    "varargs",
    "alloca",

    "scope",
    "extern",
    "static",
    "typedef",

    "symbol",
    "va_start",
    "va_arg",
    "declare_alias",

    "struct",
    "union",
    "simplefuncptr",
    "read",

    "gosub",
    "super",
    "local_identifier",
    "compress",

    "bitfield",
    "case_table",
    "jmptable",
    "func_template",

    "class_template",
    "endcase",
    "reftype",
    "copyreftype",

    "empty",
    "sendargs",
    "fvar",
    "fvars",

    "initializer_modifier",
    "declare_bitfield",
    "getlow",
    "gethigh",

    "func_name",
    "caseexpr_variable",
    "bytecode",
    "_sametypes",

    "_hasmethod",

    "brk_debug",
    "bytefit",
    "wordfit",
    "register",

    "global_registers",
    "typeof",
    "assign_init",
    "here_imm",

    "^@",
    "field",
    "asm_if",
    "asm_elseif",

    "asm_endif",
    "expect",
    "print_debug",
    "error_holder",

    "signed_booltype",
    "unsigned_booltype",
    "byteptr",
    "static_assert",

    "taskinit",
    "ditto_start",
    "ditto_end",
    "ditto_count",
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

    if (!ast) {
        printf("%*c<>\n", indent, ' ');
        return;
    }
    switch (ast->kind) {
    case AST_LISTHOLDER:
        printf("%*c<listholder>\n", indent, ' ');
        while (ast && ast->kind == AST_LISTHOLDER) {
            doASTDump(ast->left, indent+2);
            ast = ast->right;
        }
        if (ast) {
            printf("%*c<MALFORMED>\n", indent, ' ');
            doASTDump(ast, indent+2);
            printf("%*c</MALFORMED>\n", indent, ' ');
        }
        printf("%*c</listholder>\n", indent, ' ');
        return;
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
        sprintf(buf, "<integer %" PRId64 "/>", ast->d.ival);
        leaf = 1;
        break;
    case AST_HERE_IMM:
        sprintf(buf, "<here %" PRId64 "/>", ast->d.ival);
        leaf = 1;
        break;
    case AST_INLINEASM:
        sprintf(buf, "<inlineasm%s>", ast->d.ival ? " const" : " optimized");
        break;
    case AST_BITVALUE:
        sprintf(buf, "<bitvalue 0x%" PRIx64 "/>", ast->d.ival);
        leaf = 1;
        break;
    case AST_FLOAT:
        sprintf(buf, "<float %" PRIx64 "/>", ast->d.ival);
        leaf = 1;
        break;
    case AST_STRING:
        sprintf(buf, "<string %s/>", ast->d.string);
        leaf = 1;
        break;
    case AST_STRINGPTR:
        if (ast->d.ival == 0) {
            sprintf(buf, "<zstringptr>");
        } else {
            sprintf(buf, "<lstringptr %ld>", (long)ast->d.ival);
        }
        break;
    case AST_BYTEPTR:
        sprintf(buf, "<byteptr %ld>", (long)ast->d.ival);
        break;
    case AST_BYTECODE:
        sprintf(buf, "<bytecode %s/>", ast->d.string);
        leaf = 1;
        break;
    case AST_HWREG:
        sprintf(buf, "<hwreg/>");
        leaf = 1;
        break;
    case AST_COMMENT:
        sprintf(buf, "<comment/>"); // could print ast->d.string);
        leaf = 1;
        break;
    case AST_ARRAYDECL:
    {
        int arraybase = 0;
        if (ast->d.ptr) {
            arraybase = EvalConstExpr((AST *)ast->d.ptr);
        }
        sprintf(buf, "<arraydecl %d>", arraybase);
        break;
    }
    case AST_ARRAYTYPE:
    {
        int arraybase = 0;
        if (ast->d.ptr) {
            arraybase = EvalConstExpr((AST *)ast->d.ptr);
        }
        sprintf(buf, "<arraytype %d>", arraybase);
        break;
    }
    case AST_OPERATOR:
        astname = "operator";
        if (ast->d.ival >= 32 && ast->d.ival <= 126) {
            sprintf(buf, "<operator '%c'>", (int)ast->d.ival);
        } else {
            const char *opString = NULL;
            switch(ast->d.ival) {
            case K_BOOL_OR:
                opString = "||";
                break;
            case K_BOOL_AND:
                opString = "&&";
                break;
            case K_GE:
                opString = ">=";
                break;
            case K_LE:
                opString = "<=";
                break;
            case K_NE:
                opString = "<>";
                break;
            case K_EQ:
                opString = "==";
                break;
            case K_INCREMENT:
                opString = "++";
                break;
            case K_DECREMENT:
                opString = "--";
                break;
            case K_SHL:
                opString = "<<";
                break;
            case K_SAR:
                opString = ">>";
                break;
            case K_SHR:
                opString = "+>>";
                break;
            case K_ROTL:
                opString = "ROTL";
                break;
            case K_ROTR:
                opString = "ROTR";
                break;
            case K_NEGATE:
                opString = "~-";
                break;
            case K_BIT_NOT:
                opString = "~";
                break;
            case K_BOOL_NOT:
                opString = "!";
                break;
            case K_LTU:
                opString = "+<";
                break;
            case K_LEU:
                opString = "+<";
                break;
            case K_GTU:
                opString = "+>";
                break;
            case K_GEU:
                opString = "+=>";
                break;
            case K_SIGNEXTEND:
                opString = "signx";
                break;
            case K_ZEROEXTEND:
                opString = "zerox";
                break;
            case K_ASC:
                opString = "asc";
                break;
            case K_STRLEN:
                opString = "strlen";
                break;
            case K_POWER:
                opString = "power";
                break;
            default:
                sprintf(buf, "<operator #0x%" PRIx64 ">", ast->d.ival);
                break;
            }
            if (opString) {
                sprintf(buf, "<operator '%s'>", opString);
                break;
            }
        }
        break;
    case AST_ANNOTATION:
        sprintf(buf, "<annotation %s>", ast->d.string);
        break;
    case AST_FUNC_NAME:
        sprintf(buf, "<func_name/>");
        leaf = 1;
        break;
    case AST_THROW:
        sprintf(buf, "<throw %" PRId64 ">", ast->d.ival);
        break;
    case AST_LINEBREAK:
        if (ast->left) {
            sprintf(buf, "<linebreak>");
        } else {
            sprintf(buf, "<linebreak/>");
            leaf = 1;
        }
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

AST *DummyLineAst(int lineNum)
{
    AST *dummy = NewAST(AST_ERRHOLDER, NULL, NULL);
    dummy->d.ival = lineNum;  // the exact line number we wish to report
    return dummy;
}

LineInfo *GetLineInfo(AST *ast)
{
    LexStream *L;
    LineInfo *I;
    int size;
    int i;

    if (!ast || !ast->lexdata) return NULL;
    L = ast->lexdata;
    I = (LineInfo *)flexbuf_peek(&L->lineInfo);
    if (I) {
        i = ast->lineidx;
        size = (flexbuf_curlen(&L->lineInfo) / sizeof(*I));
        if (i >= size || ast->kind == AST_ERRHOLDER) {
            // AST_ERRHOLDER always refers to the current file
            i = size - 1;
        }
        return I + i;
    }
    return NULL;
}

//
// some useful utilities
//
/* create an AST in a comment holder */
AST *
NewCommentedAST(enum astkind kind, AST *left, AST *right, AST *comment)
{
    AST *ast;

    ast = NewAST(kind, left, right);
    if (comment) {
        ast = NewAST(AST_COMMENTEDNODE, ast, comment);
    }
    return ast;
}

AST *
NewStatement(AST *stmt)
{
    AST *ast;

    if (!stmt) return NULL;
    ast = NewAST(AST_STMTLIST, stmt, NULL);
    return ast;
}

AST *
NewCommentedStatement(AST *stmt)
{
    AST *ast;
    AST *comment;

    if (!stmt) return NULL;
    comment = GetComments();
    if (comment) {
        stmt = NewAST(AST_COMMENTEDNODE, stmt, comment);
    }
    ast = NewAST(AST_STMTLIST, stmt, NULL);
    return ast;
}

/* utility functions */
AST *
AstYield(void)
{
    return NewStatement(NewAST(AST_YIELD, NULL, NULL));
}

AST *
AstReturn(AST *expr, AST *comment)
{
    if (expr && expr->kind == AST_EXPRLIST && expr->right == NULL) {
        return NewCommentedAST(AST_RETURN, expr->left, NULL, comment);
    }
    return NewCommentedAST(AST_RETURN, expr, NULL, comment);
}

AST *
AstAssignList(AST *dest, AST *expr, AST *comment)
{
    AST *ast;
    if (expr && expr->kind == AST_EXPRLIST && expr->right == NULL) {
        ast = AstAssign(dest, expr->left);
    } else {
        ast = AstAssign(dest, expr);
    }
    if (comment) {
        ast = NewAST(AST_COMMENTEDNODE, ast, comment);
    }
    return ast;
}

AST *
AstAbort(AST *expr, AST *comment)
{
    return NewCommentedAST(AST_THROW, expr, NULL, comment);
}

AST *
AstCatch(AST *expr)
{
    return NewAST(AST_CATCH, expr, NULL);
}

AST *
AstSprRef(AST *index, int ormask)
{
    if (ormask) {
        index = AstOperator('|',index,AstInteger(ormask));
    }
    return NewAST(AST_SPRREF, index, NULL);
}

//
// turn an AST entry into a no-op
// an empty comment should do for this
//
void
AstNullify(AST *ast)
{
    memset(ast, 0, sizeof(*ast));
    ast->kind = AST_COMMENT;
}

//
// replace occurances of "old" within body by "new"
//
void
ReplaceAst(AST *body, AST *old, AST *newast)
{
    if (!body) return;
    if (body->left) {
        if (body->left->kind == old->kind && AstMatch(body->left, old)) {
            body->left = newast;
        } else {
            ReplaceAst(body->left, old, newast);
        }
    }
    if (body->right) {
        if (body->right->kind == old->kind && AstMatch(body->right, old)) {
            body->right = newast;
        } else {
            ReplaceAst(body->right, old, newast);
        }
    }
}

// check for a list with just one element; returns that element
AST *
ExpectOneListElem(AST *list)
{
    if (list->kind != AST_EXPRLIST) {
        ERROR(list, "Expected a list");
        return list;
    }
    if (list->right != NULL) {
        ERROR(list, "Expected a single element list, found a longer one");
    }
    return list->left;
}
