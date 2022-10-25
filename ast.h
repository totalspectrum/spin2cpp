#ifndef SPIN_AST_H
#define SPIN_AST_H

#include <stdint.h>

typedef struct LineInfo {
    const char *fileName;
    int lineno;
    char *linedata;
} LineInfo;

/*
 * types of data which may be contained within an AST node
 */

union ASTdata {
    uint64_t ival;      /* unsigned integer value */
    const char *string; /* string value */
    void *ptr;          /* generic pointer */
};

typedef struct AST AST;

/* AST types */
enum astkind {
    AST_UNKNOWN = 0,
    AST_LISTHOLDER,
    AST_INTEGER,
    AST_STRING,

    AST_IDENTIFIER = 4,
    AST_OPERATOR,
    AST_FLOAT,
    AST_ASSIGN,

    AST_ENUMSET = 8,
    AST_ARRAYDECL,
    AST_BYTELIST,
    AST_WORDLIST,

    AST_LONGLIST = 12,
    AST_INTTYPE,
    AST_UNSIGNEDTYPE,
    AST_ARRAYTYPE,

    AST_FUNCDECL = 16,
    AST_FUNCDEF,
    AST_FUNCVARS,
    AST_STMTLIST,

    AST_INSTR = 20,
    AST_HWREG,
    AST_RETURN,
    AST_IF,

    AST_THENELSE = 24,
    AST_RANGE,
    AST_RANGEREF,
    AST_FUNCCALL,

    AST_EXPRLIST = 28,
    AST_INSTRHOLDER,
    AST_INSTRMODIFIER,
    AST_ORG,

    AST_HERE = 32,
    AST_POSTSET,
    AST_WHILE,
    AST_DOWHILE,

    AST_FOR = 36,
    AST_MEMREF,
    AST_ARRAYREF,
    AST_COUNTREPEAT,

    AST_CASE = 40,
    AST_CASEITEM,
    AST_OTHER,
    AST_RES,

    AST_FROM = 44,
    AST_TO,
    AST_STEP,
    AST_FIT,

    AST_ADDROF = 48,
    AST_LOOKUP,
    AST_LOOKDOWN,
    AST_OBJECT,

    AST_METHODREF = 52,
    AST_CONSTREF,
    AST_OBJDECL,
    AST_STRINGPTR,

    AST_YIELD = 56,
    AST_CONSTANT,
    AST_QUITLOOP,
    AST_CONTINUE,

    AST_RESULT = 60,
    AST_ROUND,
    AST_TRUNC,
    AST_TOFLOAT,

    AST_FILE = 64,
    AST_THROW,
    AST_CATCH,
    AST_LOOKEXPR,

    AST_DATADDROF = 68,
    AST_ANNOTATION,
    AST_TEMPARRAYDECL,
    AST_TEMPARRAYUSE,

    AST_PUBFUNC = 72,
    AST_PRIFUNC,
    AST_FUNCHOLDER,
    AST_ENUMSKIP,

    AST_LINEBREAK = 76,
    AST_COMMENT,
    AST_COMMENTEDNODE,
    AST_COGINIT,

    AST_SPRREF = 80,
    AST_ABSADDROF,
    AST_FLOATTYPE,
    AST_PTRTYPE,

    AST_GENERICTYPE = 84,
    AST_VOIDTYPE,
    AST_SEQUENCE,
    AST_CONDRESULT,

    AST_FORATLEASTONCE = 88,
    AST_ISBETWEEN,  /* left is between two values on right */
    AST_INLINEASM,
    AST_OPERAND,    /* used in ASM backend, ptr is operand */

    AST_ORGH = 92,
    AST_MASKMOVE,
    AST_MODIFIER_CONST,
    AST_MODIFIER_VOLATILE,

    AST_IMMHOLDER = 96,
    AST_BIGIMMHOLDER,
    AST_ALIGN,
    AST_ORGF,

    AST_TUPLE_TYPE = 100,
    AST_SRCCOMMENT,
    AST_DECLARE_VAR,
    AST_DECLARE_VAR_WEAK, // like AST_DECLARE_VAR, but no error if already defined

    AST_LABEL = 104,
    AST_GOTO,
    AST_PRINT,
    AST_CHAR,

    AST_REGPAIR = 108,
    AST_FUNCTYPE,
    AST_SELF,
    AST_BITVALUE, // a generic bit value, e.g. NIL is BITVALUE(0)

    AST_NEW = 112,
    AST_DELETE,
    AST_USING,
    AST_GLOBALVARS,

    AST_LAMBDA = 116,
    AST_SETJMP,
    AST_TRYENV,
    AST_CATCHRESULT,

    AST_SIZEOF = 120,
    AST_CAST,
    AST_VARARGS,
    AST_ALLOCA,

    AST_SCOPE = 124,
    AST_EXTERN,
    AST_STATIC,
    AST_TYPEDEF,

    AST_SYMBOL = 128,
    AST_VA_START,
    AST_VA_ARG,
    AST_DECLARE_ALIAS,

    AST_STRUCT = 132,
    AST_UNION,
    AST_SIMPLEFUNCPTR,
    AST_READ,

    AST_GOSUB = 136,
    AST_SUPER,
    AST_LOCAL_IDENTIFIER,
    AST_COMPRESS_INSTR,

    AST_BITFIELD = 140,
    AST_CASETABLE,
    AST_JUMPTABLE,
    AST_FUNC_TEMPLATE,

    AST_CLASS_TEMPLATE = 144, // reserved for future use
    AST_ENDCASE,
    AST_REFTYPE, // like a ptrtype, but transparent
    AST_COPYREFTYPE, // first the value is copied, then a reference passed; used for passing large structs

    AST_EMPTY = 148,
    AST_MODIFIER_SEND_ARGS = 149,
    AST_FVAR_LIST = 150,
    AST_FVARS_LIST = 151,

    AST_INITMODIFIER = 152,
    AST_DECLARE_BITFIELD = 153,
    AST_GETLOW = 154,
    AST_GETHIGH = 155,

    AST_FUNC_NAME = 156,
    AST_CASEEXPR = 157, // Represents the hidden variable in a bytecode CASE. Very terrible hack.
    AST_BYTECODE = 158, // a raw bytecode opcode, used in sys/
    AST_SAMETYPES = 159, // BASIC compile time type checking _sametypes(T, long)
    AST_HASMETHOD = 160, // BASIC compile time checking, _hasmethod(T, identifier)

    AST_BRKDEBUG = 161, // Opaque debug break.
    AST_BYTEFITLIST = 162,
    AST_WORDFITLIST = 163,
    AST_REGISTER = 164,

    AST_REGISTERVARS = 165,
    AST_TYPEOF = 166,
};

/* forward reference */
typedef struct lexstream LexStream;

struct AST {
    enum astkind kind;        /* type of this node */
    union ASTdata d; /* data in this node */
    AST *left;
    AST *right;

    // debug info
    LexStream *lexdata; /* points to the current lexer */
    int lineidx;        /* index within the lexer LineInfo struct */
};

typedef struct ASTReportInfo {
    LexStream *lexdata;
    int lineidx;
} ASTReportInfo;

/* function declarations */
AST *NewAST(enum astkind kind, AST *left, AST *right);
AST *AddToList(AST *list, AST *newelement);
AST *AddToLeftList(AST *list, AST *newelement);
AST *AddToListEx(AST *list, AST *newelement, AST **tail);
AST *ListInsertBefore(AST *list, AST *member, AST *newelem);
AST *AppendList(AST *list, AST *newlist);
void RemoveFromList(AST **listptr, AST *newelement);
AST *DupAST(AST *ast);
AST *DupASTTypeSafe(AST *ast);
AST *DupASTWithReplace(AST *ast, AST *orig, AST *replace);
AST *AstInteger(int64_t intval);
AST *AstFloat(float f);
AST *AstBitValue(int64_t intval);
AST *AstPlainString(const char *string);
AST *AstStringPtr(const char *string);
AST *AstIdentifier(const char *name);
AST *AstTempIdentifier(const char *name);
AST *AstInstrModifier(int32_t intval);
AST *AstOperator(int32_t intval, AST *left, AST *right);
AST *AstOpAssign(int32_t intval, AST *left, AST *right);
AST *AstAssign(AST *left, AST *right);
AST *AstDeclareLocal(AST *left, AST *right);
AST *AstTempVariable(const char *prefix);
AST *AstTempLocalVariable(const char *prefix, AST *type);
int IsAstTempVariable(AST *var);
AST *AstLookup(enum astkind kind, int index, AST *expr, AST *table);

/* check to see if two trees are identical */
int AstMatch(AST *a, AST *b);

/* see if two function bodies are identical, up to identifier renames */
int AstBodyMatch(AST *a, AST *b);

/* check to see if a tree is a subtree of another */
int AstUses(AST *big, AST *sub);

/* check to see if identifier AST *id is modified inside *body */
int AstModifiesIdentifier(AST *body, AST *id);

/* length of an AST list */
int AstListLen(AST *a);

/* length of an AST stringptr list */
int AstStringLen(AST *a);

/* mark new ASTs to be created to have the same line as AST old */
/* used when we're transforming ASTs */
void AstReportAs(AST *old, ASTReportInfo *save);
void AstReportDone(ASTReportInfo *save);

/* print out an AST */
void DumpAST(AST *);

/* get LineInfo for an AST */
LineInfo *GetLineInfo(AST *);

/* create a dummy AST to report an error on line lineNo of the current file */
AST *DummyLineAst(int lineNum);

/* useful utilities for language parsers */
AST *NewCommentedAST(enum astkind kind, AST *left, AST *right, AST *comment);
AST *NewStatement(AST *stmt);
AST *NewCommentedStatement(AST *stmt);
AST *AstReturn(AST *expr, AST *comment);
AST *AstAssignList(AST *dest, AST *expr, AST *comment);
AST *AstYield(void);
AST *AstAbort(AST *expr, AST *comment);
AST *AstCatch(AST *expr);
AST *AstSprRef(AST *index, int ormask);
AST *CheckYield(AST *loopbody);
void ReplaceAst(AST *body, AST *old, AST *newast);

// checks for a list with just one element, and returns that element
AST *ExpectOneListElem(AST *list);

// turn an AST into a no-op
void AstNullify(AST *ptr);

#define IsIdentifier(ast) ((ast)->kind == AST_IDENTIFIER || ((ast)->kind == AST_LOCAL_IDENTIFIER))

#endif
