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

IR *NewIR(void)
{
    IR *ir = malloc(sizeof(*ir));
    memset(ir, 0, sizeof(*ir));
    return ir;
}

Register *NewRegister(enum Regkind k, const char *name)
{
    Register *R = malloc(sizeof(*R));
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
    char *name = "stest001_dummy";
    IR *ir = NewIR();
    ir->opc = OPC_LABEL;
    ir->dst = NewRegister(REG_LABEL, name);
    AppendIR(irl, ir);
}

void EmitFunctionEpilog(IRList *irl, Function *f)
{
    char *name = "stest001_dummy_ret";
    IR *ir = NewIR();
    ir->opc = OPC_LABEL;
    ir->dst = NewRegister(REG_LABEL, name);
    AppendIR(irl, ir);

    ir = NewIR();
    ir->opc = OPC_RET;
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

bool
CompileToIR(IRList *irl, ParserState *P)
{
    irl->head = NULL;
    irl->tail = NULL;

    EmitWholeFunction(irl, NULL);
    
    return true;
}
