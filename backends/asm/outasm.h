/*
 * Spin to Pasm converter
 * Copyright 2016-2020 Total Spectrum Software Inc.
 * PASM output routines
 */

#ifndef OUTASM_H
#define OUTASM_H

#include "instr.h"

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

// optimization functions
void OptimizeIRLocal(IRList *irl, Function *f);
void OptimizeIRGlobal(IRList *irl);
void OptimizeFcache(IRList *irl);
bool ShouldBeInlined(Function *f);
bool RemoveIfInlined(Function *f);
int  ExpandInlines(IRList *irl);

void ReplaceOpcode(IR *ir, IROpcode op);

bool IsDummy(IR *ir);
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

    /* trampoline code for calling to another COG */
    Operand *asmtrampoline;

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
    
    /* flag for whether we should inline the function */
    bool isInline;
    
    /* type of calling convention */
    CallConvention convention;

    /* actual call instructions emitted for this function */
    int actual_callsites;

    /* firl emitted already */
    bool firl_done;
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

// compile inline assembly
#define INLINE_ASM_FLAG_CONST  0x01
#define INLINE_ASM_FLAG_FCACHE 0x02

void CompileInlineAsm(IRList *irl, AST *ast, unsigned asmFlags);
Operand *CompileIdentifier(IRList *irl, AST *expr);

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

#endif
