#ifndef EXPR_H
#define EXPR_H

#if 0
typedef int32_t ExprInt;
typedef uint32_t UExprInt;
#else
typedef int64_t ExprInt;
typedef uint64_t UExprInt;
#endif

typedef float ExprFloat;  // changing this from "float" probably will not work

typedef struct exprval {
    AST *type;
    ExprInt  val;
} ExprVal;

/* evaluate a constant expression */
ExprInt EvalConstExpr(AST *expr);

/* similar but for PASM */
ExprInt EvalPasmExpr(AST *expr);

/* determine whether an expression is defined */
bool IsDefinedExpr(AST *expr);

/* determine whether an expression is constant */
/* note that strings, as pointers to memory, are not "constant"
   in that their value is not known until link time */
int IsConstExpr(AST *expr);

int IsConstInitializer(AST *ident);

/* determine whether an expression is a constant zero */
bool IsConstZero(AST *ast);

/* same but check for any value */
bool IsConstEqual(AST *ast, ExprInt val);

/* determine whether an expression is a float constant */
int IsFloatConst(AST *expr);
/* determine whether an expression is a string constant
   the value will not be known initially at compile time!
*/
bool IsStringConst(AST *expr);

/* evaluate to an integer if an expression is constant */
AST *FoldIfConst(AST *expr);

/* look up a methodref or constref */
Symbol *LookupMethodRef(AST *expr, Module **Ptr, int *valid);

/* look up the class name of an object */
const char *ObjClassName(AST *objtype);

/* expression utility functions */
union float_or_int {
    int32_t i;
    float   f;
};

ExprVal intExpr(ExprInt x);
ExprVal floatExpr(ExprFloat f);
ExprVal fixedExpr(ExprInt f);
ExprInt   floatAsInt(ExprFloat f);
ExprFloat intAsFloat(ExprInt i);

AST *RemoveTypeModifiers(AST *typ);

int TypeSize(AST *typ);
int TypeAlign(AST *typ);
int CheckedTypeSize(AST *typ); // like TypeSize, but declares members if necessary
int PaddedTypeAlign(AST *typ);  // pads to long boundary if necessary

int IsArray(AST *expr);
int IsArrayType(AST *typ);
int IsArraySymbol(Symbol *);
int IsArrayOrPointerSymbol(Symbol *);

int PointerTypeIncrement(AST *typ);
// get base of an array type
AST *GetArrayBase(AST *type);

// get size of array type
AST *GetArraySize(AST *type);

// create an array based on a type and expression or expression list
// the elements of the list can either be simple expressions (giving the array size)
// or AST_RANGE (giving the start and end indices)
AST *MakeArrayType(AST *basetype, AST *exprlist);

int IsFunctionType(AST *typ);
int IsStringType(AST *typ);
int IsFloatType(AST *typ);
int IsIntType(AST *typ);
int IsVoidType(AST *typ);
int IsUnsignedType(AST *typ);
int IsGenericType(AST *typ);
int IsPointerType(AST *typ);
int IsRefType(AST *typ);
int IsBoolType(AST *typ);

int IsInt64Type(AST *typ);
int IsFloat64Type(AST *typ);

AST *ArrayToPointerType(AST *type);
AST *ClassToPointerType(AST *type);

#define IsScalar64Type(t) (IsInt64Type(t) || IsFloat64Type(t))

#define IsIntOrGenericType(t) (!t || IsGenericType(t) || IsIntType(t))
int IsBoolCompatibleType(AST *typ);
int IsClassType(AST *typ);
int IsInterfaceType(AST *typ);

int IsConstType(AST *typ);
int IsConstArrayType(AST *typ);

// number of arguments to a function of this type
int NumArgsForType(AST *functyp);

// note that this function isn't symmetric; for pointer types,
// it allows A to have a stricter type than B (so for example
// it's OK to pass a "char *" to a "const char *" but not vice-versa)
int CompatibleTypes(AST *A, AST *B);

// check for two types the same
int SameTypes(AST *A, AST *B);

// get the internal name from an AST_IDENTIFIER or AST_SYMBOL
const char *GetIdentifierName(AST *ident);
// get the user visible name from an AST_IDENTIFIER or AST_SYMBOL
const char *GetUserIdentifierName(AST *ident);
// get a user-printable string for an expression; similar to GetUserIdentifierName
const char *GetExprString(AST *expr);

// get the name to use for printing error messages about assignments and such
const char *GetVarNameForError(AST *expr);

// various symbol lookup utilities
Symbol *LookupSymbol(const char *name);
Symbol *LookupAstSymbol(AST *ast, const char *msg);
Symbol *LookupMemberSymbol(AST *topexpr, AST *objtype, const char *name, Module **Ptr, int *valid);

// find expression type relative to current function
AST *ExprType(AST *ast);

// find expression type relative to some table
AST *ExprTypeRelative(SymbolTable *table, AST *ast, Module *P);

// the type underlying an array or pointer
AST *BaseType(AST *ast);

// dereference any Typeof()
AST *DerefType(AST *ast);

// others
AST *TransformRangeAssign(AST *dst, AST *src, int optoken, int toplevel);
AST *TransformRangeUse(AST *src);
AST *TransformCaseExprList(AST *var, AST *list);
AST *CheckSimpleArrayref(AST *ast);

// optimize things like ((a+N)-N) -> a
AST *SimpleOptimizeExpr(AST *);

// return 1 if an expression can have side effects
int ExprHasSideEffects(AST *);

// number of results from a function
int FuncNumResults(AST *functype);
// number of longs coming back from a function (which may be more than FuncNumResults if
// some results need multiple longs)
int FuncLongResults(AST *functype);

// number of parameters to a function (negative for varargs, in which
// case the absolute value is the minimum number of parameters)
int FuncNumParams(AST *functype);

// number of longs passed as parameters
int FuncLongParams(AST *functype);

// clean up a type so it has no symbolic references; needed so that e.g. array
// sizes that are symbolic constants can be found outside of the module they're
// declared in
AST *CleanupType(AST *typ);

// build an expression list containing individual object member references
AST *BuildExprlistFromObject(AST *expr, AST *typ);

// fix up an initializer list of type "type"
// handles designators like .x = n, and adds any missing 0's
AST *FixupInitList(AST *typ, AST *initval);

/* get a const value as either a constant or a default value */
int32_t const_or_default(Module *M, const char *name, int32_t defaultval);

/* type name */
const char *TypeName(AST *typ);

/* create a function call with parameters left, right, extra (some may
   be NULL */
AST *MakeOperatorCall(AST *func, AST *left, AST *right, AST *extra);
#if (defined(__GNUC__) || defined(__clang__))
#define clz32(x)    __builtin_clz((uint32_t)(x))
#define ctz32(x)    __builtin_ctz((uint32_t)(x))
#define popcount(x) __builtin_popcount(x)
#define bswap32(x)  __builtin_bswap32((uint32_t)(x))
#define bswap16(x)  __builtin_bswap16((uint16_t)(x)) 
#else
#define NEED_BUILTINS
extern int clz32(uint32_t x);
extern int ctz32(uint32_t x);
extern int popcount(uint64_t x);
extern uint32_t bswap32(uint32_t x);
extern uint16_t bswap16(uint16_t x);
#endif

#ifndef __BYTE_ORDER__
#define __ORDER_LITTLE_ENDIAN__ 0
#define __ORDER_BIG_ENDIAN__    1
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#endif

extern AST *struct_copy;
extern AST *struct_memset;

#endif
