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
/* note that strings, as pointers to memory, are not "constant"
   in that their value is not known until link time */
int IsConstExpr(AST *expr);
/* determine whether an expression is a float constant */
int IsFloatConst(AST *expr);
/* determine whether an expression is a string constant
   the value will not be known initially at compile time!
*/
bool IsStringConst(AST *expr);

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
int ArrayTypeSize(AST *typ);
int TypeAlignment(AST *typ);
int PointerTypeIncrement(AST *typ);

int IsFloatType(AST *typ);
int IsIntType(AST *typ);
int IsGenericType(AST *typ);
int IsPointerType(AST *typ);
#define IsIntOrGenericType(t) (IsGenericType(t) || IsIntType(t))

int CompatibleTypes(AST *A, AST *B);

Symbol *LookupSymbol(const char *name);
Symbol *LookupAstSymbol(AST *ast, const char *msg);
Symbol *LookupObjSymbol(AST *expr, Symbol *obj, const char *name);

AST *ExprType(AST *ast);

AST *TransformRangeAssign(AST *dst, AST *src, int toplevel);
AST *TransformRangeUse(AST *src);
AST *TransformCaseExprList(AST *var, AST *list);

// optimize things like ((a+N)-N) -> a
AST *SimpleOptimizeExpr(AST *);

// return 1 if an expression can have side effects
int ExprHasSideEffects(AST *);

#endif
