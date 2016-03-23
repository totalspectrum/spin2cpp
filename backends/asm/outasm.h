/*
 * Spin to Pasm converter
 * Copyright 2016 Total Spectrum Software Inc.
 * PASM output routines
 */

#ifndef OUTASM_H
#define OUTASM_H

#include "instr.h"

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
Operand *NewOperand(enum Operandkind, const char *name, int val);
Operand *NewImmediate(int32_t val);
Operand *NewImmediatePtr(const char *name, Operand *val);
Operand *NewCodeLabel();

void FreeTempRegisters(IRList *irl, int starttempreg);

// utility functions
IRCond InvertCond(IRCond v);

// function to convert an IR list into a text representation of the
// assembly
char *IRAssemble(IRList *list);

// create an IR list from a module definition
bool CompileToIR(IRList *list, Module *P);

// optimization functions
void OptimizeIRLocal(IRList *irl);
void OptimizeIRGlobal(IRList *irl);
bool ShouldBeInlined(Function *f);
int  ExpandInlines(IRList *irl);

bool IsDummy(IR *ir);
bool IsValidDstReg(Operand *reg);
bool SrcOnlyHwReg(Operand *reg);
bool IsLocal(Operand *reg);
bool IsLocalOrArg(Operand *reg);
bool IsHwReg(Operand *reg);

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

    /* label to go to for "return" instruction; this may simply
       be asmretname, but may be something else if there is a need
       for function cleanup
    */
    Operand *asmreturnlabel;
    
    /* function header (mostly to make sure we collect comments
       at the right time */
    IRList irheader;
    
    /* instructions for this function */
    /* leaves off initial label and final ret, so it's
       suitable for inlining */
    IRList irl;

    /* list of registers that need preserving (for
       recursive functions) */
    OperandList *saveregs;
    
    /* flag for whether we should inline the function */
    bool isInline;

    /* type of calling convention */
    CallConvention convention;
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
void CompileInlineAsm(IRList *irl, AST *ast);
Operand *CompileIdentifier(IRList *irl, AST *expr);

/* assign variable space in COG memory */
Operand *GetGlobal(Operandkind kind, const char *name, intptr_t value);

/* assign variable space in HUB memory */
Operand *GetHub(Operandkind kind, const char *name, intptr_t value);

#endif
