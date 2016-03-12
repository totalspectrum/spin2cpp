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
#include "outasm.h"

static int inDat;
static int inCon;
static int didOrg;

static void
doPrintOperand(struct flexbuf *fb, Operand *reg, int useimm)
{
    char temp[128];
    if (!reg) {
        ERROR(NULL, "internal error bad operand");
        flexbuf_addstr(fb, "???");
        return;
    }
    switch (reg->kind) {
    case IMM_INT:
        if (reg->val >= 0 && reg->val < 512) {
            flexbuf_addstr(fb, "#");
            if (reg->name && reg->name[0]) {
                flexbuf_addstr(fb, reg->name);
            } else {
                sprintf(temp, "%d", (int)(int32_t)reg->val);
                flexbuf_addstr(fb, temp);
            }
        } else {
            // the immediate actually got processed as a register
            flexbuf_addstr(fb, reg->name);
        }
        break;
    case BYTE_REF:
    case WORD_REF:
    case LONG_REF:
        ERROR(NULL, "Internal error: tried to use memory directly");
        break;
    case IMM_COG_LABEL:
        if (useimm) {
            flexbuf_addstr(fb, "#");
        }
        /* fall through */
    default:
        flexbuf_addstr(fb, reg->name);
        break;
    }
}

static void
PrintOperandSrc(struct flexbuf *fb, Operand *reg)
{
    doPrintOperand(fb, reg, 1);
}

static void
PrintOperand(struct flexbuf *fb, Operand *reg)
{
    doPrintOperand(fb, reg, 0);
}

void
PrintOperandAsValue(struct flexbuf *fb, Operand *reg)
{
    Operand *indirect;
    
    switch (reg->kind) {
    case IMM_INT:
        flexbuf_printf(fb, "%d", (int)(int32_t)reg->val);
        break;
    case IMM_HUB_LABEL:
    case STRING_DEF:
        flexbuf_addstr(fb, "@@@");
        // fall through
    case IMM_COG_LABEL:
        flexbuf_addstr(fb, reg->name);
        break;
    case IMM_STRING:
        flexbuf_addchar(fb, '"');
        flexbuf_addstr(fb, reg->name);
        flexbuf_addchar(fb, '"');
        break;
    case REG_HUBPTR:
        indirect = (Operand *)reg->val;
        flexbuf_addstr(fb, indirect->name);
        break;
    default:
        PrintOperand(fb, reg);
        break;
    }
}

static void
PrintCond(struct flexbuf *fb, IRCond cond)
{
    switch (cond) {
    case COND_TRUE:
      break;
    case COND_EQ:
      flexbuf_addstr(fb, " if_e");
      break;
    case COND_NE:
      flexbuf_addstr(fb, " if_ne");
      break;
    case COND_LT:
      flexbuf_addstr(fb, " if_b");
      break;
    case COND_GE:
      flexbuf_addstr(fb, " if_ae");
      break;
    case COND_GT:
      flexbuf_addstr(fb, " if_a");
      break;
    case COND_LE:
      flexbuf_addstr(fb, " if_be");
      break;
    case COND_C:
      flexbuf_addstr(fb, " if_c");
      break;
    case COND_NC:
      flexbuf_addstr(fb, " if_nc");
      break;
    default:
      flexbuf_addstr(fb, " if_??");
      break;
    }
    flexbuf_addchar(fb, '\t');
}

static void
OutputBlob(Flexbuf *fb, Operand *label, Operand *op)
{
    unsigned char *data;
    int len;
    
    if (op->kind != IMM_STRING) {
        ERROR(NULL, "Internal: bad binary blob");
        return;
    }
    flexbuf_printf(fb, "\tlong\n"); // ensure long alignment
    flexbuf_printf(fb, label->name);
    flexbuf_printf(fb, "\n");
    data = (unsigned char *)op->name;
    len = op->val;
    while (len > 8) {
        flexbuf_printf(fb, "\tbyte\t$%02x,$%02x,$%02x,$%02x,$%02x,$%02x,$%02x,$%02x\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
        len -= 8;
        data += 8;
    }
    if (len > 0) {
        flexbuf_printf(fb, "\tbyte\t");
        while (len > 0) {
            flexbuf_printf(fb, "$%02x%s", data[0], len == 1 ? "\n" : ",");
            --len;
            ++data;
        }
    }
}

/* find string for opcode */
static const char *
StringFor(IROpcode opc)
{
    switch(opc) {
    case OPC_STRING:
    case OPC_BYTE:
        return "byte";
    case OPC_LONG:
        return "long";
    case OPC_WORD:
        return "word";
    default:
        ERROR(NULL, "internal error, bad StringFor call");
        return "???";
    }
}

/* convert IR list into p1 assembly language */
void
P1AssembleIR(struct flexbuf *fb, IR *ir)
{
    if (ir->opc == OPC_CONST) {
        // handle const declaration
        if (!inCon) {
            flexbuf_addstr(fb, "CON\n");
            inCon = 1;
            inDat = 0;
        }
        flexbuf_addstr(fb, "\t");
        PrintOperand(fb, ir->dst);
        flexbuf_addstr(fb, " = ");
        PrintOperandAsValue(fb, ir->src);
        flexbuf_addstr(fb, "\n");
        return;
    }
    if (!inDat) {
        flexbuf_addstr(fb, "DAT\n");
        inCon = 0;
        inDat = 1;
        if (!didOrg) {
            flexbuf_addstr(fb, "\torg\t0\n");
            didOrg = 1;
        }
    }
    if (ir->instr) {
        int ccset;
        
        PrintCond(fb, ir->cond);
        flexbuf_addstr(fb, ir->instr->name);
        switch (ir->instr->ops) {
        case NO_OPERANDS:
            break;
        case SRC_OPERAND_ONLY:
        case DST_OPERAND_ONLY:
        case CALL_OPERAND:
            flexbuf_addstr(fb, "\t");
            PrintOperandSrc(fb, ir->dst);
            break;
        default:
            flexbuf_addstr(fb, "\t");
            PrintOperand(fb, ir->dst);
            flexbuf_addstr(fb, ", ");
            PrintOperandSrc(fb, ir->src);
            break;
        }
        ccset = ir->flags & (FLAG_WC|FLAG_WZ|FLAG_NR|FLAG_WR);
        if (ccset) {
            const char *sepstring = " ";
            if (ccset & FLAG_WC) {
                flexbuf_printf(fb, "%swc", sepstring);
                sepstring = ",";
            }
            if (ccset & FLAG_WZ) {
                flexbuf_printf(fb, "%swz", sepstring);
                sepstring = ",";
            }
            if (ccset & FLAG_NR) {
                flexbuf_printf(fb, "%snr", sepstring);
            } else if (ccset & FLAG_WR) {
                flexbuf_printf(fb, "%swr", sepstring);
            }
        }
        flexbuf_addstr(fb, "\n");
        return;
    }
    
    switch(ir->opc) {
    case OPC_DUMMY:
        break;
    case OPC_DEAD:
        /* no code necessary, internal opcode */
        flexbuf_addstr(fb, "\t.dead\t");
        flexbuf_addstr(fb, ir->dst->name);
        flexbuf_addstr(fb, "\n");
        break;
    case OPC_COMMENT:
        PrintOperand(fb, ir->dst);
	break;
    case OPC_LABEL:
        flexbuf_addstr(fb, ir->dst->name);
        flexbuf_addstr(fb, "\n");
        break;
    case OPC_RET:
        flexbuf_addchar(fb, '\t');
        flexbuf_addstr(fb, "ret\n");
        break;
    case OPC_BYTE:
    case OPC_WORD:
    case OPC_LONG:
    case OPC_STRING:
        flexbuf_addchar(fb, '\t');
	flexbuf_addstr(fb, StringFor(ir->opc));
	flexbuf_addstr(fb, "\t");
	PrintOperandAsValue(fb, ir->dst);
        flexbuf_addstr(fb, "\n");
	break;
    case OPC_LABELED_BLOB:
        // output a binary blob
        // dst has a label
        // data is in a string in src
        OutputBlob(fb, ir->dst, ir->src);
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
    
    inDat = 0;
    inCon = 0;
    didOrg = 0;
    
    flexbuf_init(&fb, 512);
    for (ir = list->head; ir; ir = ir->next) {
        P1AssembleIR(&fb, ir);
    }
    flexbuf_addchar(&fb, 0);
    ret = flexbuf_get(&fb);
    flexbuf_delete(&fb);
    return ret;
}
