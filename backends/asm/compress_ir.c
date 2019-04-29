/*
 * Spin to C/C++ translator
 * Copyright 2016-2019 Total Spectrum Software Inc.
 * 
 * +--------------------------------------------------------------------
 * ¦  TERMS OF USE: MIT License
 * +--------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * +--------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "spinc.h"
#include "outasm.h"

typedef struct PtrFrequency {
    int count; // the count of its use
    void *ptr; // the operand or instruction
} PtrFreq;

Flexbuf dstcount;
Flexbuf srccount;
Flexbuf instrcount;

#define MAX_COMPRESS 32

IR *cinstr_table[MAX_COMPRESS];
Operand *cdst_table[MAX_COMPRESS];
Operand *csrc_table[MAX_COMPRESS];

// sorter function for PtrFreq
int freqsort_fn(const void *aptr, const void *bptr)
{
    const PtrFreq *a = aptr;
    const PtrFreq *b = bptr;
    return b->count - a->count;
}

// check to see if two operands match
static int MatchOperand(void *aptr, void *bptr)
{
    Operand *a = (Operand *)aptr;
    Operand *b = (Operand *)bptr;
    if (a->kind != b->kind) {
        return 0;
    }
    if (a->kind == IMM_INT) {
        return a->val == b->val;
    }
    return a == b;
}

static int MatchIR(void *aptr, void *bptr)
{
    IR *a = (IR *)aptr;
    IR *b = (IR *)bptr;
    if (a->instr != b->instr) {
        return 0;
    }
    if (a->flags != b->flags) {
        return 0;
    }
    return a->srceffect == b->srceffect && a->dsteffect == b->dsteffect;
}

// record an operand in a frequency tables
#define RecordOperand(fb, oper) RecordItem(fb, oper, MatchOperand)
#define RecordInstr(fb, instr) RecordItem(fb, instr, MatchIR)


static void RecordItem(Flexbuf *fb, void *ptr, int (*matchptr)(void *, void *))
{
    size_t count = flexbuf_curlen(fb) / sizeof(PtrFreq);
    PtrFreq *freqtable = (PtrFreq *)flexbuf_peek(fb);
    PtrFreq newfreq;
    size_t i;
    
    if (!ptr) {
        return;
    }
    for (i = 0; i < count; i++) {
        if (matchptr(freqtable[i].ptr, ptr)) {
            freqtable[i].count++;
            return;
        }
    }
    newfreq.ptr = ptr;
    newfreq.count = 1;
    flexbuf_addmem(fb, (char *)&newfreq, sizeof(newfreq));
}

static void DoPrintOp(void *ptr)
{
    Flexbuf irbuf;
    const char *s;
    Operand *op = (Operand *)ptr;
    flexbuf_init(&irbuf, 1024);
    PrintOperandAsValue(&irbuf, op);
    flexbuf_addchar(&irbuf, 0);
    
    s = flexbuf_get(&irbuf);
    flexbuf_delete(&irbuf);
    printf("%s%s\n", (op->kind == IMM_INT) ? "#" : "", s);
}

static void DoPrintIR(void *ptr)
{
    IR *ir = (IR *)ptr;
    printf("%s\n", ir->instr->name);
}

static void
SortPrint(const char *msg, Flexbuf *buf, void (*doprint)(void *), void **tableptr, void *defaultval)
{
    size_t size, members;
    size_t i;
    int counted;
    PtrFreq *table;
    int maxprint;
    int running;
    
    // now sort by usage
    size = flexbuf_curlen(buf);
    members = size / sizeof(PtrFreq);
    table = (PtrFreq *)flexbuf_peek(buf);
    qsort(table, members, sizeof(PtrFreq), freqsort_fn);

    // now print most popular 32
    if (members > MAX_COMPRESS)
        maxprint = MAX_COMPRESS;
    else
        maxprint = members;
    
    counted = 0;
    for (i = 0; i < members; i++) {
        counted += table[i].count;
    }
#ifdef DEBUG
    printf("%s\n", msg);
#endif
    running = 0;
    for (i = 0; i < maxprint; i++) {
        running += table[i].count;
#ifdef DEBUG        
        printf("freq: %d / %d  ", table[i].count, running);
        doprint(table[i].ptr);
#else
        (void)doprint;
#endif
        tableptr[i] = table[i].ptr;
    }
    while (i < MAX_COMPRESS) {
        tableptr[i] = defaultval;
        i++;
    }
#ifdef DEBUG    
    printf("total count: %d / %d\n", running, counted);
#endif    
}

// walk through and record the most popular instructions/operands in the
// list "irl" into the list "kernel"
void IRCompress(IRList *irl, IRList *kernel)
{
    IR *ir, *origir;
    Operand *opsrc, *opdst;
    int i;
    
    flexbuf_init(&instrcount, 1024);
    flexbuf_init(&srccount, 1024);
    flexbuf_init(&dstcount, 1024);
    
    for (ir = irl->head; ir; ir = ir->next) {
        if (ir->opc >= OPC_GENERIC) {
            // not an opcode we're comfortable with
            continue;
        }
        if (!ir->instr || ir->instr->ops != TWO_OPERANDS) {
            continue;
        }
        RecordInstr(&instrcount, (void *)ir);
        RecordOperand(&dstcount, (void *)ir->dst);
        RecordOperand(&srccount, (void *)ir->src);
    }

    // establish default values for various instructions
    opsrc = opdst = GetOneGlobal(REG_HW, "ina", 0);
    ir = NewIR(OPC_MOV);
    ir->src = opsrc;
    ir->dst = opdst;
    SortPrint("instructions", &instrcount, DoPrintIR, (void **)cinstr_table, (void *)ir);
    SortPrint("dst operands:", &dstcount, DoPrintOp, (void **)cdst_table, (void *)opdst);
    SortPrint("src operands:", &srccount, DoPrintOp, (void **)csrc_table, (void *)opsrc);
    // now add 32 instructions to COMPRESS_TABLE in the kernel
    EmitNamedCogLabel(kernel, "COMPRESS_TABLE");
    
    for (i = 0; i < MAX_COMPRESS; i++) {
        opdst = cdst_table[i];
        opsrc = csrc_table[i];
        origir = cinstr_table[i];
        ir = EmitOp2(kernel, origir->opc, opdst, opsrc);
        ir->flags = origir->flags;
        ir->srceffect = origir->srceffect;
        ir->dsteffect = origir->dsteffect;
    }
}
