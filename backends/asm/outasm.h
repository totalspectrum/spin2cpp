/*
 * Spin to Pasm converter
 * Copyright 2016-2025 Total Spectrum Software Inc.
 * PASM output routines
 */

#ifndef OUTASM_H
#define OUTASM_H

#include "instr.h"
#include "becommon.h"
#include "util/sha256.h"

// LMM jumps +- this amount are turned into add/sub of the pc
// pick a conservative value
// 127 would be the absolute maximum here
#define MAX_REL_JUMP_OFFSET_LMM 100
// On P2 conditional jumps have a range +/- 256
#define MAX_REL_JUMP_OFFSET_P2 200

// functions for producing local identifiers
const char *IdentifierLocalName(Function *func, const char *name);
const char *IdentifierModuleName(Module *P, const char *name);
char *NewTempLabelName();

//
// functions for manipulating IR lists
//
IR *NewIR(IROpcode kind);

// append an IR at the end of a list
void AppendIR(IRList *irl, IR *ir);
// insert an IR after another in a list
void InsertAfterIR(IRList *irl, IR *orig, IR *ir);
void DeleteIR(IRList *irl, IR *ir);
void AppendIRList(IRList *irl, IRList *sub);
void ReplaceIRWithInline(IRList *irl, IR *ir, Function *func);

//
// functions for operand manipulation
//
Operand *NewOperand(enum Operandkind, const char *name, intptr_t val);
Operand *NewImmediate(int32_t val);
Operand *NewImmediatePtr(const char *name, Operand *val);
Operand *NewPcRelative(int32_t val);
Operand *NewCodeLabel();  // use only while compiling a function
Operand *NewHubLabel();
Operand *CogMemRef(Operand *addr, int offset);
Operand *SubRegister(Operand *reg, unsigned long offset);
void FreeTempRegisters(IRList *irl, int starttempreg);

char *OffsetName(const char *basename, unsigned long offset);

// utility functions
IRCond InvertCond(IRCond v);

// function to convert an IR list into a text representation of the
// assembly
char *IRAssemble(IRList *list, Module *P);

// do instruction compression
void IRCompress(IRList *list, IRList *kernel);

// create an IR list from a module definition
bool CompileToIR(IRList *list, Module *P);

IR *EmitLabel(IRList *list, Operand *op);
IR *EmitMove(IRList *irl, Operand *dst, Operand *src, AST *linenum);

// optimization functions
void OptimizeIRLocal(IRList *irl, Function *f);
void OptimizeIRGlobal(IRList *irl);
void OptimizeFcache(IRList *irl);
bool AnalyzeInlineEligibility(Function *f);
bool RemoveIfInlined(Function *f);
int  ExpandInlines(IRList *irl);

void ReplaceOpcode(IR *ir, IROpcode op);

bool IsDummy(IR *ir);
bool IsBranch(IR *ir);
bool IsValidDstReg(Operand *reg);
bool SrcOnlyHwReg(Operand *reg);
bool IsLocal(Operand *reg);
bool IsLocalOrArg(Operand *reg);
bool IsHwReg(Operand *reg);

bool IsHubDest(Operand *dst);
Operand *JumpDest(IR *ir);

typedef enum callconvention {
    FAST_CALL,   // arguments & return in registers, native call
    STACK_CALL,  // arguments & return on stack
} CallConvention;

//
// back end data for functions
//
typedef struct ir_bedata {
    /* temporary register info */
    int curtempreg;
    int maxtempreg;

    /* assembly output name */
    Operand *asmname;
    Operand *asmretname;
    Operand *asmretregister; /* register version of this for push/pop */
    /* label to go to for "return" instruction; this may simply
       be asmretname, but may be something else if there is a need
       for function cleanup
    */
    Operand *asmreturnlabel;

    /* optional label for COGSPIN output, which is the entry point
       the Spin->PASM wrapper code needs to use
    */
    Operand *asmaltname;

    /* optional label for tail-calls */
    Operand *asmentername;
    
    /* function header (mostly to make sure we collect comments
       at the right time */
    IRList irheader;
    
    /* instructions for this function */
    /* leaves off initial label and final ret, so it's
       suitable for inlining */
    IRList irl;

    /* number of local registers that need to be pushed */
    int numsavedregs;
    
    /* flags for whether we should inline the function */
    unsigned inliningFlags;
   #define ASM_INLINE_SMALL_FLAG  0x01
   #define ASM_INLINE_SINGLE_FLAG 0x02
   #define ASM_INLINE_PURE_FLAG   0x04

   /* if this function was actually inlined somewhere (do we really need this?) */
    bool got_inlined;

    /* set after inlining if the function has no external calls left */
    bool effectivelyLeaf;
    /* Highest arg count clobbered */
    /* (beware of off-by-one errors) */
    int maxClobberArg;
    
    /* type of calling convention */
    CallConvention convention;

    /* actual call instructions emitted for this function */
    int actual_callsites;

    /* firl emitted already */
    bool firl_done;

    /* list of duplicate functions that share the same code as this one */
    FunctionList *funcdups;
    
    /* hash of the function's instructions */
    unsigned char firl_hash[SHA256_BLOCK_SIZE];
    
} IRFuncData;

#define FuncData(f) ((IRFuncData *)(f)->bedata)
#define FuncIRL(f)  (&FuncData(f)->irl)

//
// back end data for modules
//
typedef struct asmmoddata {
    Operand *datbase;
    Operand *datlabel;
} AsmModData;

#define ModData(P) ((AsmModData *)(P)->bedata)

// find a PASM instruction description for a generic optimizer instruction
Instruction *FindInstrForOpc(IROpcode kind);

void CompileInlineAsm(IRList *irl, AST *ast, unsigned asmFlags);
Operand *CompileIdentifier(IRList *irl, AST *expr);
Operand *CompileSymbolForFunc(IRList *irl, Symbol *sym, Function *func, AST *ast);

/* assign variable space in COG memory */
Operand *GetSizedGlobal(Operandkind kind, const char *name, intptr_t value, int count);
Operand *GetOneGlobal(Operandkind kind, const char *name, intptr_t value);

/* assign variable space in HUB memory */
Operand *GetSizedHub(Operandkind kind, const char *name, intptr_t value, int count);
Operand *GetOneHub(Operandkind kind, const char *name, intptr_t value);

void InitAsmCode();

Operand *GetLabelOperand(const char *name, bool inFcache);
Operand *GetLabelFromSymbol(AST *where, const char *name, bool inFcache);

// find the variable name in a variable or AST_DECLARE_VAR tree
const char *VarName(AST *ast);

// get operands for argument and result registers
Operand *GetArgReg(int n);
Operand *GetResultReg(int n);

// convert to an effective address
Operand *GetLea(IRList *irl, Operand *src);

// instruction emitting stuff
IR *EmitOp1(IRList *irl, IROpcode code, Operand *op);
IR *EmitOp2(IRList *irl, IROpcode code, Operand *op, Operand *op2);

void EmitNamedCogLabel(IRList *irl, const char *name);

// functions for optimization
bool InstrModifies(IR *ir, Operand *op);

// utility for debug
void PrintOperandAsValue(struct flexbuf *fb, Operand *op);
void DoAssembleIR(struct flexbuf *, IR *, Module *);

int OutAsm_DebugEval(AST *ast, int regNum, int *addr, void *ourarg);

struct ir_lbljumps {
   struct ir_lbljumps *next;
   IR *jump;
};

//
// fetch the real next opcode
//
static inline IR *NextIR(IR *ir) {
    do {
        ir = ir->next;
    } while (ir && ir->opc == OPC_COMMENT);
    return ir;
}

static inline bool
InstrIsVolatile(IR *ir)
{
    return 0 != (ir->flags & FLAG_KEEP_INSTR);
}


// optimization utilities
bool InstrUsesFlags(IR *ir, unsigned flags);
bool InstrSetsFlags(IR *ir, unsigned flags);
bool InstrSetsAnyFlags(IR *ir);

void ReplaceZWithNC(IR *ir);

bool SameIROperand(Operand *, Operand *);
bool IRIsDeadAfter(IR *instr, Operand *op);
bool IRFlagsDeadAfter(IRList *irl, IR *ir, unsigned flags);
int OptimizePeephole2(IRList *irl);

#define SameOperand(a, b) SameIROperand(a, b)
#define IsDeadAfter(ir, op) IRIsDeadAfter(ir, op)

// Hashing functions
void HashFuncIRL(Function *f);

#endif
