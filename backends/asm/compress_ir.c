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

// sorter function for PtrFreq
int freqsort_fn(const void *aptr, const void *bptr)
{
    const PtrFreq *a = aptr;
    const PtrFreq *b = bptr;
    return b->count - a->count;
}

// record an operand in a frequency tables
#define RecordOperand(fb, oper) RecordItem(fb, oper, 1)
#define RecordInstr(fb, instr) RecordItem(fb, instr, 0)

static void RecordItem(Flexbuf *fb, void *ptr, int checkImm)
{
    size_t count = flexbuf_curlen(fb) / sizeof(PtrFreq);
    PtrFreq *freqtable = (PtrFreq *)flexbuf_peek(fb);
    PtrFreq newfreq;
    size_t i;
    Operand *ptrop;
    Operand *oper = (Operand *)ptr;
    
    if (!oper) {
        return;
    }
    for (i = 0; i < count; i++) {
        ptrop = (Operand *)freqtable[i].ptr;
        if (checkImm && oper->kind == IMM_INT) {
            if (ptrop->kind == IMM_INT &&
                oper->val == ptrop->val)
            {
                freqtable[i].count++;
                return;
            }
        } else if (ptrop == oper) {
            freqtable[i].count++;
            return;
        }
    }
    newfreq.ptr = (void *)oper;
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

static void DoPrintInstr(void *ptr)
{
    Instruction *instr = (Instruction *)ptr;
    printf("%s\n", instr->name);
}

#define MAXSIZE 32

static void
SortPrint(Flexbuf *buf, void (*doprint)(void *))
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
    table = (PtrFreq *)flexbuf_get(buf);
    qsort(table, members, sizeof(PtrFreq), freqsort_fn);

    // now print most popular 32
    if (members > MAXSIZE)
        maxprint = MAXSIZE;
    else
        maxprint = members;
    
    counted = 0;
    for (i = 0; i < members; i++) {
        counted += table[i].count;
    }
    running = 0;
    for (i = 0; i < maxprint; i++) {
        running += table[i].count;
        printf("freq: %d / %d  ", table[i].count, running);
        doprint(table[i].ptr);
    }
    printf("total count: %d / %d\n", running, counted);
}

// walk through and record the most popular instructions/operands
void IRCompress(IRList *irl)
{
    IR *ir;

    flexbuf_init(&instrcount, 1024);
    flexbuf_init(&srccount, 1024);
    flexbuf_init(&dstcount, 1024);
    
    for (ir = irl->head; ir; ir = ir->next) {
        if (ir->opc >= OPC_GENERIC) {
            // not an opcode we're comfortable with
            continue;
        }
        RecordInstr(&instrcount, (void *)ir->instr);
        RecordOperand(&dstcount, (void *)ir->dst);
        RecordOperand(&srccount, (void *)ir->src);
    }
    printf("instructions:\n");
    SortPrint(&instrcount, DoPrintInstr);
    printf("dest operands:\n");
    SortPrint(&dstcount, DoPrintOp);
    printf("\nsrc operands:\n");
    SortPrint(&srccount, DoPrintOp);
}
