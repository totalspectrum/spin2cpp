#ifndef BACKEND_COMMON_H
#define BACKEND_COMMON_H

#include "ast.h"
#include "symbol.h"
#include "expr.h"

AST *BuildMethodPointer(AST *ast, bool is_abs);
void OutputAlignLong(Flexbuf *fb);
void OutputDataBlob(Flexbuf *fb, Flexbuf *databuf, Flexbuf *relocbuf, const char *startLabel, bool emitLabel);

void NormalizeVarOffsets(Function *f);

void CompileAsmToBinary(const char *binname, const char *asmname); // in cmdline.c

// evaluate any constant expressions inside a string
// and return an AST representing the whole string
AST *EvalStringConst(AST *expr);

// Similar to EvalStringConst, but either zero terminates (if lenVal is 0) or
// else prepends a length count of lenVal bytes
AST *EvalTerminatedStringConst(AST *expr, int lenVal);

// turn an AST stringptr into a flexbuf buffer, with a length prefix
// that is "lenPrefix" bytes long (or make it zero terminated if
// "lenPrefix" is 0)
void StringBuildBuffer(Flexbuf *fb, AST *expr, int lenPrefix);

// print into a freshly allocated string
char *auto_printf(size_t max,const char *format,...) __attribute__((format(printf,2,3)));

// output for DAT blocks
/* functions for printing data into a flexbuf */
typedef struct DataBlockOutFuncs {
    void (*startAst)(Flexbuf *f, AST *ast);
    void (*putByte)(Flexbuf *f, int c);
    void (*endAst)(Flexbuf *f, AST *ast);
} DataBlockOutFuncs;

/*
 * information about relocations:
 * data blocks are normally just binary blobs; but if an absolute address
 * (like @@@foo) is requested, we need a way to specify that address
 * Note that a normal @foo in a dat section is a relative address;
 * @@@foo requires that we add the base of the dat to @foo.
 * For now, the relocation system works only on longs, and only in some
 * modes. For each long that needs the base of dat added to it we emit
 * a relocation r, which contains (a) the offset of the relocatable long
 * in bytes from the start of the dat, and (b) the symbol to add to the base
 * of the dat section at that long.
 * The relocs should be sorted in order of increasing offset, so we can
 * easily process them in order along with the output.
 *
 * We also re-use the Reloc struct to hold debug information as well, so
 * that we can provide source listings for the DAT section contents.
 * for that purpose we emit DebugEntry
 */
typedef struct Reloc {
    int32_t  kind;    // reloc or debug
    int32_t  addr;    // the address the entry affects (offset from dat base)
    Symbol   *sym;    // the symbol this relocation is relative to (NULL for dat base)
    int32_t  symoff;  // offset relative to sym
} Reloc;

enum Reloc_Kind {
    RELOC_KIND_NONE = 0,  // no relocation, should not produce an entry
    RELOC_KIND_DEBUG = 1,  // not a real relocation, a debug entry
    RELOC_KIND_I32   = 2,  // add a symbol to a 32 bit value
    RELOC_KIND_AUGS  = 3,  // relocation for AUGS
    RELOC_KIND_AUGD  = 4,  // relocation for AUGD
    RELOC_KIND_FPTR16 = 5, // relocation for function pointer (16 bits)
    RELOC_KIND_FPTR12 = 6, // relocation for function pointer (12 bits)
};

/* find the backend name for a symbol */
const char *BackendNameForSymbol(Symbol *sym);
/* utility for PASM names */
const char *IdentifierModuleName(Module *P, const char *basename);
/* utility for Nucode names */
const char *NuCodeSymbolName(Symbol *sym);

void PrintDataBlock(Flexbuf *f, AST *list, DataBlockOutFuncs *funcs, Flexbuf *relocs);
void PrintDataBlockForGas(Flexbuf *f, Module *P, int inlineAsm);

/* flags for PrintExpr and friends */
#define PRINTEXPR_DEFAULT    0x0000
#define PRINTEXPR_GAS        0x0001  /* printing in a GAS context */
#define PRINTEXPR_ASSIGNMENT 0x0002  /* printing from an assignment operator */
#define PRINTEXPR_ISREF      0x0004  /* expression used as a reference */
#define PRINTEXPR_GASIMM     0x0008  /* GAS expression is an immediate value (so divide labels by 4) */
#define PRINTEXPR_GASOP      0x0010  /* GAS expression used in an operand */
#define PRINTEXPR_GASABS     0x0020  /* absolute address, not relative */
#define PRINTEXPR_USECONST   0x0040  /* print constant names, not values */
#define PRINTEXPR_TOPLEVEL   0x0080  /* leave out parens around operators */
#define PRINTEXPR_USEFLOATS  0x0100  /* print  expression as floats if appropriate */
#define PRINTEXPR_INLINESYM  0x0200  /* printing symbols in inline assembly */
#define PRINTEXPR_FORCE_UNS  0x0400  /* force arguments to be unsigned */
#define PRINTEXPR_DEBUG      0x0800  /* print symbols for DEBUG */

/* printing functions */
void PrintTypedExpr(Flexbuf *f, AST *casttype, AST *expr, int flags);
void PrintExpr(Flexbuf *f, AST *expr, int flags);
void PrintLHS(Flexbuf *f, AST *expr, int flags);
void PrintBoolExpr(Flexbuf *f, AST *expr, int flags);
void PrintAsAddr(Flexbuf *f, AST *expr, int flags);
void PrintExprList(Flexbuf *f, AST *list, int flags, Function *func);
void PrintType(Flexbuf *f, AST *type, int flags);
void PrintCastType(Flexbuf *f, AST *type);
void PrintPostfix(Flexbuf *f, AST *val, int toplevel, int flags);
void PrintInteger(Flexbuf *f, int32_t v, int flags);
void PrintFloat(Flexbuf *f, int32_t v, int flags);
int  PrintLookupArray(Flexbuf *f, AST *arr, int flags);
void PrintGasExpr(Flexbuf *f, AST *expr, bool useFloat);
void PrintSymbol(Flexbuf *f, Symbol *sym, int flags);
void PrintObjConstName(Flexbuf *f, Module *P, const char* name);
void PrintStatementList(Flexbuf *f, AST *ast, int indent);

/* for PASM debug */

/* function to evaluate an expression and put its address into "addr"
 * returns PASM_EVAL_ISCONST for a constant, PASM_EVAL_ISREG if
 * addr points to a register; for multiple registers it returns
 * PASM_EVAL_ISREG_n. Otherwise we can't handle it (yet)
 * but eventually could save result on stack or in hub
 * regNum counts how many register arguments have been seen so far
 * in this DEBUG
 */
typedef int (*BackendDebugEval)(AST *ast, int regNum, int *addr, void *beArg);
#define PASM_EVAL_ISCONST 0
#define PASM_EVAL_ISREG   1
#define PASM_EVAL_ISREG_2 2
#define PASM_EVAL_ISREG_3 3
#define PASM_EVAL_ISREG_4 4
#define PASM_EVAL_ISREG_MAX PASM_EVAL_ISREG_4

#define MAX_BRK 256
extern unsigned brkAssigned; // Currently assigned BRK codes

int Pasm_DebugEval(AST *ast, int regNum, int *addr, void *arg);
int AsmDebug_CodeGen(AST *ast, BackendDebugEval evalFunc, void *evalArg);
Flexbuf CompileBrkDebugger(size_t appsize);

/* utility for visiting modules */
typedef int (*VisitorFunc)(void *irl, Module *P);
int VisitRecursive(void *irl, Module *P, VisitorFunc func, unsigned visitval);

// the bottom 8 bits are reserved for IterateOverModules
// the next 8 are reserved for other potential uses elsewhere in code
#define VISITFLAG_RESERVED      0x0000FFFF
#define VISITFLAG_COMPILEIR_COG 0x00010000
#define VISITFLAG_COMPILEIR_HUB 0x00020000
#define VISITFLAG_COMPILEIR_LUT 0x00040000
#define VISITFLAG_FUNCNAMES     0x00080000
#define VISITFLAG_COMPILEFUNCS  0x00100000
#define VISITFLAG_EXPANDINLINE  0x00200000
#define VISITFLAG_EMITDAT       0x00400000
#define VISITFLAG_BC_OPTIMIZE   0x00800000

// interpreter ability functions
bool interp_can_unsigned();
bool interp_can_multireturn();

// some common optimization functions
bool CanUseEitherSignedOrUnsigned(AST *node);

bool isBoolOperator(int optoken);
bool OptimizeOperator(int *optoken, AST **left,AST **right);

static inline bool isPowerOf2(uint32_t x)
{
    return (x & (x-1)) == 0;
}

// Decompose val into a sequence of a shift, add/sub
// returns 0 if failure, 1 if success
// sets shifts[0] to final shift
// shifts[1] to +1 for add, -1 for sub, 0 if done
// shifts[2] to initial shift
int DecomposeBits(unsigned val, int *shifts);

// allocate a FunctionList entry
FunctionList *NewFunctionList(Function *f);

#endif /* BACKEND_COMMON_H */
