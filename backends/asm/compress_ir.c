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

// check to see if an operand is a simple 9 bit op
static int
IsSimple9BitOperand(Operand *op)
{
    if (!op) return 0;
    switch (op->kind) {
    case IMM_INT:
        return (op->val >= 0 && op->val < 512);
    case REG_HW:
    case REG_REG:
    case REG_LOCAL:
    case REG_ARG:
    case REG_RESULT:
        return 1;
    default:
        return 0;
    }
}

// sorter function for PtrFreq
int freqsort_fn(const void *aptr, const void *bptr)
{
    const PtrFreq *a = (const PtrFreq *)aptr;
    const PtrFreq *b = (const PtrFreq *)bptr;
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
    if (a->cond != b->cond) {
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

static void DoPrintOp(struct flexbuf *fb, void *ptr)
{
    Operand *op = (Operand *)ptr;
    if (op->kind == IMM_INT) {
        flexbuf_addstr(fb, "#");
    }
    PrintOperandAsValue(fb, op);
}

static void DoPrintIR(struct flexbuf *fb, void *ptr)
{
    IR *ir = (IR *)ptr;
    flexbuf_printf(fb, "%s", ir->instr->name);
}

static void
SortPrint(const char *msg, Flexbuf *buf, void (*doprint)(struct flexbuf *, void *), void **tableptr, void *defaultval)
{
    size_t size, members;
    size_t i;
    int counted;
    PtrFreq *table;
    int maxprint;
    int running;
#ifdef DEBUG    
    struct flexbuf sbuf;
#endif
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
        flexbuf_init(&sbuf, 1024);
        doprint(&sbuf, table[i].ptr);
        flexbuf_addchar(&sbuf, 0);
        printf("%s\n", flexbuf_get(&sbuf));
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
    (void)counted;
    (void)running;
}

static int FindPtr(void **table, void *ptr, int (*matchptr)(void *, void *))
{
    int i;
    for (i = 0; i < MAX_COMPRESS; i++) {
        if (matchptr(table[i], ptr)) {
            break;
        }
    }
    return i;
}

// walk through and record the most popular instructions/operands in the
// list "irl" into the list "kernel"
int gl_printstats = 0;

void IRCompress(IRList *irl, IRList *kernel)
{
    IR *ir, *origir;
    IR *newir;
    Operand *opsrc, *opdst;
    int i;
    struct flexbuf comment;
    int byte_hits, word_hits, tuple_hits;
    int misses = 0;
    
    byte_hits = word_hits = 0;
    tuple_hits = 0;
    
    flexbuf_init(&instrcount, 1024);
    flexbuf_init(&srccount, 1024);
    flexbuf_init(&dstcount, 1024);
    flexbuf_init(&comment, 1024);
    
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
        ir->cond = origir->cond;
        ir->srceffect = origir->srceffect;
        ir->dsteffect = origir->dsteffect;
    }

    // now go back and re-write matching instructions...
    ir = irl->head;
    while (ir) {
        int instr_idx;
        int dst_idx;
        int src_idx;
        uint8_t byte0, byte1;
        origir = ir->next;
        if (ir->opc >= OPC_GENERIC) {
            // not an opcode we're comfortable with
            ir = origir;
            continue;
        }
        if (!ir->instr || ir->instr->ops != TWO_OPERANDS) {
            ir = origir;
            continue;
        }
        instr_idx = FindPtr((void **)cinstr_table, ir, MatchIR);
        dst_idx = FindPtr((void **)cdst_table, ir->dst, MatchOperand);
        src_idx = FindPtr((void **)csrc_table, ir->src, MatchOperand);
        // 4 bits for instruction, 5 bits each for src and dst
        if (instr_idx < 16 && dst_idx < 32 && src_idx < 32) {
            word_hits++;
            DoAssembleIR(&comment, ir, NULL);
            flexbuf_addchar(&comment, 0);
            newir = NewIR(OPC_COMMENT);
            newir->opc = OPC_COMMENT;
            newir->dst = NewOperand(IMM_STRING, flexbuf_get(&comment), 0);
            newir->src = NULL;
            newir->instr = NULL;
            InsertAfterIR(irl, ir, newir);
            DeleteIR(irl, ir);
            ir = newir;
            if (instr_idx < 2 && dst_idx < 8 && src_idx < 8) {
#ifdef NEVER                
                // 1 bit for instruction, 3 bits each for dst and src
                // no longer supported, but for debug keep track
                byte0 = (instr_idx << 6) + (dst_idx << 3) + src_idx;
                newir = NewIR(OPC_BYTE);
                newir->dst = NewImmediate(byte0);
                InsertAfterIR(irl, ir, newir);
#endif                
                byte_hits++;
            }
            {
                byte0 = 0x80 + (instr_idx << 2) + (dst_idx >> 3);
                byte1 = ((dst_idx & 0x7) << 5) + src_idx;
                newir = NewIR(OPC_BYTE);
                newir->dst = NewImmediate(byte1);
                InsertAfterIR(irl, ir, newir);
                newir = NewIR(OPC_BYTE);
                newir->dst = NewImmediate(byte0);
                InsertAfterIR(irl, ir, newir);                
            }
        } else if (instr_idx < 16 && IsSimple9BitOperand(ir->dst) && IsSimple9BitOperand(ir->src)) {
            Operand *src, *dst;

            src = ir->src;
            dst = ir->dst;
            
            DoAssembleIR(&comment, ir, NULL);
            flexbuf_addchar(&comment, 0);
            newir = NewIR(OPC_COMMENT);
            newir->opc = OPC_COMMENT;
            newir->dst = NewOperand(IMM_STRING, flexbuf_get(&comment), 0);
            newir->src = NULL;
            newir->instr = NULL;
            InsertAfterIR(irl, ir, newir);
            DeleteIR(irl, ir);
            ir = newir;
            
            newir = NewIR(OPC_COMPRESS3);
            newir->dst = dst;
            newir->src = src;
            newir->src2 = NewImmediate(instr_idx);
            InsertAfterIR(irl, ir, newir);
            tuple_hits++;
        } else {
            misses++;
        }
        ir = origir;
    }
    if (gl_printstats) {
        printf("hit %d words, %d tuples, %d short bytes; %d misses\n", word_hits, tuple_hits, byte_hits, misses);
    }
}
