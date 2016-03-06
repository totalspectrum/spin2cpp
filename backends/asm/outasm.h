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

    /* instructions for this function */
    IRList irl;

    /* flag for whether we should inline the function */
    bool isInline;
} IRFuncData;

#define FuncData(f) ((IRFuncData *)(f)->bedata)
#define FuncIRL(f)  (&FuncData(f)->irl)

// find a PASM instruction description for a generic optimizer instruction
Instruction *FindInstrForOpc(IROpcode kind);

// compile inline assembly
void CompileInlineAsm(IRList *irl, AST *ast);

#endif
