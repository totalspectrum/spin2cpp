#ifndef EXPR_H
#define EXPR_H

typedef enum {
    INT_EXPR,
    FLOAT_EXPR
} ExprDefType;

typedef struct exprval {
    ExprDefType type;
    int32_t  val;
} ExprVal;

/* evaluate a constant expression */
int32_t EvalConstExpr(AST *expr);

/* similar but for PASM */
int32_t EvalPasmExpr(AST *expr);

/* determine whether an expression is constant */
int IsConstExpr(AST *expr);
/* determine whether an expression is a float constant */
int IsFloatConst(AST *expr);
/* evaluate to an integer if an expression is constant */
AST *FoldIfConst(AST *expr);

/* printing functions */
void PrintExpr(FILE *f, AST *expr);
void PrintBoolExpr(FILE *f, AST *expr);
void PrintAsAddr(FILE *f, AST *expr);
void PrintExprList(FILE *f, AST *list);
void PrintType(FILE *f, AST *type);
void PrintPostfix(FILE *f, AST *val, int toplevel);
void PrintInteger(FILE *f, int32_t v);
int  PrintLookupArray(FILE *f, AST *arr);

/* expression utility functions */
union float_or_int {
    int32_t i;
    float   f;
};

ExprVal intExpr(int32_t x);
ExprVal floatExpr(float f);
int32_t  floatAsInt(float f);
float    intAsFloat(int32_t i);

int IsArray(AST *expr);
int IsArrayType(AST *typ);
int IsArraySymbol(Symbol *);

#endif
