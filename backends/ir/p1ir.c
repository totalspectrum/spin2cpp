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

static int inDat;
static int inCon;
static int didOrg;

static void
PrintOperand(struct flexbuf *fb, Operand *reg)
{
    char temp[128];
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
    case IMM_LABEL:
        flexbuf_addstr(fb, "#");
        /* fall through */
    default:
        flexbuf_addstr(fb, reg->name);
        break;
    }
}

void
PrintOperandAsValue(struct flexbuf *fb, Operand *reg)
{
    char temp[128];
    switch (reg->kind) {
    case IMM_INT:
        sprintf(temp, "%d", (int)(int32_t)reg->val);
        flexbuf_addstr(fb, temp);
        break;
    case IMM_LABEL:
        flexbuf_addstr(fb, reg->name);
        break;
    case IMM_STRING:
        flexbuf_addchar(fb, '"');
        flexbuf_addstr(fb, reg->name);
        flexbuf_addchar(fb, '"');
        break;
    default:
        PrintOperand(fb, reg);
        break;
    }
}

static const char *
StringFor(int opc)
{
  switch(opc) {
  case OPC_MOVE:
      return "mov";
  case OPC_ABS:
      return "abs";
  case OPC_ADD:
      return "add";
  case OPC_AND:
      return "and";
  case OPC_ANDN:
      return "andn";
  case OPC_CALL:
      return "call";
  case OPC_CMP:
      return "cmp";
  case OPC_CMPS:
      return "cmps";
  case OPC_DJNZ:
      return "djnz";
  case OPC_JUMP:
      return "jmp";
  case OPC_MAXS:
      return "maxs";
  case OPC_MINS:
      return "mins";
  case OPC_NEG:
      return "neg";
  case OPC_NOT:
      return "not";
  case OPC_OR:
      return "or";
  case OPC_RDBYTE:
      return "rdbyte";
  case OPC_RDLONG:
      return "rdlong";
  case OPC_RDWORD:
      return "rdword";
  case OPC_REV:
      return "rev";
  case OPC_ROL:
      return "rol";
  case OPC_ROR:
      return "ror";
  case OPC_SHL:
      return "shl";
  case OPC_SHR:
      return "shr";
  case OPC_SAR:
      return "sar";
  case OPC_SUB:
      return "sub";
  case OPC_WAITCNT:
      return "waitcnt";
  case OPC_WAITPEQ:
      return "waitpeq";
  case OPC_WAITPNE:
      return "waitpne";
  case OPC_WAITVID:
      return "waitvid";
  case OPC_XOR:
      return "xor";
  case OPC_WRBYTE:
      return "wrbyte";
  case OPC_WRLONG:
      return "wrlong";
  case OPC_WRWORD:
      return "wrword";
  case OPC_STRING:
  case OPC_BYTE:
      return "byte";
  case OPC_WORD:
      return "word";
  case OPC_LONG:
      return "long";
  default:
      break;
  }
  return "???";
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

/* convert IR list into p1 assembly language */
void
P1AssembleIR(struct flexbuf *fb, IR *ir)
{
    int ccset; // condition codes set
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
    switch(ir->opc) {
    case OPC_DEAD:
        /* no code necessary, internal opcode */
        if (gl_optimize_flags & OPT_NO_ASM) {
          flexbuf_addstr(fb, "\t.dead\t");
	  flexbuf_addstr(fb, ir->dst->name);
          flexbuf_addstr(fb, "\n");
	}
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
    case OPC_JUMP:
    case OPC_CALL:
        PrintCond(fb, ir->cond);
	flexbuf_addstr(fb, StringFor(ir->opc));
	flexbuf_addstr(fb, "\t");
	PrintOperand(fb, ir->dst);
        flexbuf_addstr(fb, "\n");
	break;
    case OPC_DJNZ:
        PrintCond(fb, ir->cond);
	flexbuf_addstr(fb, StringFor(ir->opc));
	flexbuf_addstr(fb, "\t");
	PrintOperand(fb, ir->dst);
	flexbuf_addstr(fb, ",");
	PrintOperand(fb, ir->src);
	flexbuf_addstr(fb, "\n");
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
    case OPC_MOVE:
    case OPC_ABS:
    case OPC_ADD:
    case OPC_AND:
    case OPC_ANDN:
    case OPC_CMP:
    case OPC_CMPS:
    case OPC_MAXS:
    case OPC_MINS:
    case OPC_NEG:
    case OPC_NOT:
    case OPC_OR:
    case OPC_RDBYTE:
    case OPC_RDWORD:
    case OPC_RDLONG:
    case OPC_REV:
    case OPC_ROL:
    case OPC_ROR:
    case OPC_SAR:
    case OPC_SHL:
    case OPC_SHR:
    case OPC_SUB:
    case OPC_WAITCNT:
    case OPC_WAITPEQ:
    case OPC_WAITPNE:
    case OPC_WAITVID:
    case OPC_WRBYTE:
    case OPC_WRWORD:
    case OPC_WRLONG:
    case OPC_XOR:
        PrintCond(fb, ir->cond);
	flexbuf_addstr(fb, StringFor(ir->opc));
	flexbuf_addstr(fb, "\t");
	PrintOperand(fb, ir->dst);
	flexbuf_addstr(fb, ", ");
	PrintOperand(fb, ir->src);
	ccset = ir->flags & (FLAG_WC|FLAG_WZ);
	switch(ccset) {
	case FLAG_WZ:
	  flexbuf_addstr(fb, " wz");
	  break;
	case FLAG_WC:
	  flexbuf_addstr(fb, " wc");
	  break;
	case FLAG_WC|FLAG_WZ:
	  flexbuf_addstr(fb, " wc,wz");
	  break;
	default:
	  break;
	}
	flexbuf_addstr(fb, "\n");
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
