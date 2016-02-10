/*
 * Spin to C/C++ translator
 * Copyright 2016 Total Spectrum Software Inc.
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
#include "spinc.h"
#include "ir.h"
#include "flexbuf.h"

void
PrintOperand(struct flexbuf *fb, Operand *reg)
{
    switch (reg->kind) {
    default:
        flexbuf_addstr(fb, reg->name);
        break;
    }
}

/* convert IR list into p1 assembly language */
void
P1AssembleIR(struct flexbuf *fb, IR *ir)
{
    switch(ir->opc) {
    case OPC_COMMENT:
        PrintOperand(fb, ir->dst);
	break;
    case OPC_LABEL:
        PrintOperand(fb, ir->dst);
        flexbuf_addstr(fb, "\n");
        break;
    case OPC_RET:
        flexbuf_addchar(fb, '\t');
        flexbuf_addstr(fb, "ret\n");
        break;
    default:
        ERROR(NULL, "Internal error: unable to process IR\n");
        break;
    }
}

/* assemble an IR list */
char *
IRAssemble(IRList *list)
{
    IR *ir;
    struct flexbuf fb;
    char *ret;
    
    flexbuf_init(&fb, 512);
    for (ir = list->head; ir; ir = ir->next) {
        P1AssembleIR(&fb, ir);
    }
    flexbuf_addchar(&fb, 0);
    ret = flexbuf_get(&fb);
    flexbuf_delete(&fb);
    return ret;
}
