#ifndef EXPR_H
#define EXPR_H

typedef struct exprval {
    AST *type;
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

/* look up an object constant reference */
/* sets *objsym to the object and *sym to the symbol */
/* returns 0/1 for fail/success */
int GetObjConstant(AST *expr, Symbol **objsym_ptr, Symbol **sym_ptr);

/* look up the class name of an object */
const char *ObjClassName(Symbol *obj);

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

int IsFloatType(AST *typ);
int IsIntType(AST *typ);
int IsGenericType(AST *typ);
#define IsIntOrGenericType(t) (IsGenericType(t) || IsIntType(t))

Symbol *LookupSymbol(const char *name);
Symbol *LookupAstSymbol(AST *ast, const char *msg);
Symbol *LookupObjSymbol(AST *expr, Symbol *obj, const char *name);

AST *ExprType(AST *ast);

AST *TransformRangeAssign(AST *dst, AST *src);
AST *TransformRangeUse(AST *src);

#endif
