//
// Pasm data output for spin2cpp
//
// Copyright 2016 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "spinc.h"
#include "ir.h"

void
OutputAsmCode(const char *fname, ParserState *P)
{
    FILE *f = NULL;
    ParserState *save;
    IRList irl;
    const char *asmcode;
    
    save = current;
    current = P;

    if (!CompileToIR(&irl, P)) {
        return;
    }
    asmcode = IRAssemble(&irl);
    
    current = save;

    f = fopen(fname, "wb");
    if (!f) {
        perror(fname);
        exit(1);
    }
    fwrite(asmcode, 1, strlen(asmcode), f);
    fclose(f);
}

IR *NewIR(enum IROpcode kind)
{
    IR *ir = malloc(sizeof(*ir));
    memset(ir, 0, sizeof(*ir));
    ir->opc = kind;
    return ir;
}

Operand *NewOperand(enum Operandkind k, const char *name)
{
    Operand *R = malloc(sizeof(*R));
    memset(R, 0, sizeof(*R));
    R->kind = k;
    R->name = name;
    return R;
}

/*
 * append some IR to the end of a list
 */
void
AppendIR(IRList *irl, IR *ir)
{
    IR *last = irl->tail;
    if (!ir) return;
    if (!last) {
        irl->head = irl->tail = ir;
        return;
    }
    last->next = ir;
    ir->prev = last;
    while (ir->next) {
        ir = ir->next;
    }
    irl->tail = ir;
}


void EmitFunctionProlog(IRList *irl, Function *f)
{
    IR *ir = NewIR(OPC_LABEL);
    ir->dst = f->asmname;
    AppendIR(irl, ir);
}

void EmitFunctionEpilog(IRList *irl, Function *f)
{
    IR *ir = NewIR(OPC_LABEL);
    ir->dst = f->asmretname;
    AppendIR(irl, ir);

    ir = NewIR(OPC_RET);
    AppendIR(irl, ir);
}

/*
 * compile a function to IR and put it at the end of the IRList
 */

void
EmitWholeFunction(IRList *irl, Function *f)
{
    EmitFunctionProlog(irl, f);
    EmitFunctionEpilog(irl, f);
}

Operand *newlineOp;

bool
CompileToIR(IRList *irl, ParserState *P)
{
    Function *f;

    irl->head = NULL;
    irl->tail = NULL;

    for(f = P->functions; f; f = f->next) {
        char *frname;
	char *fname;
	frname = malloc(strlen(f->name) + strlen(P->basename) + 8);
	sprintf(frname, "%s_%s", P->basename, f->name);
	fname = strdup(frname);
	strcat(frname, "_ret");

	f->asmname = NewOperand(REG_LABEL, fname);
	f->asmretname = NewOperand(REG_LABEL, frname);
	if (newlineOp) {
	    IR *ir = NewIR(OPC_COMMENT);
	    ir->dst = newlineOp;
	    AppendIR(irl, ir);
	}
        EmitWholeFunction(irl, f);
	newlineOp = NewOperand(REG_STRING, "\n");
    }
    return true;
}
