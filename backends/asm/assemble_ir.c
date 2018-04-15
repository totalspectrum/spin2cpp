/*
 * Spin to C/C++ translator
 * Copyright 2016-2018 Total Spectrum Software Inc.
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
static int lmmMode;

static void
doPrintOperand(struct flexbuf *fb, Operand *reg, int useimm, enum OperandEffect effect)
{
    char temp[128];
    
    if (!reg) {
        ERROR(NULL, "internal error bad operand");
        flexbuf_addstr(fb, "???");
        return;
    }
    if (effect != OPEFFECT_NONE) {
        if (gl_p2) {
            if (reg->kind != REG_HW) {
                ERROR(NULL, "operand effect on wrong register");
            }
        } else {
            ERROR(NULL, "illegal operand effect");
        }
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
        } else if (gl_p2) {
            flexbuf_addstr(fb, "##");
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
    case IMM_HUB_LABEL:
        if (gl_p2 && useimm) {
            flexbuf_addstr(fb, "#@");
        }
        flexbuf_addstr(fb, reg->name);
        break;
    case IMM_COG_LABEL:
        if (useimm) {
            flexbuf_addstr(fb, "#");
        }
        /* fall through */
    default:
        if (effect == OPEFFECT_PREINC) {
            flexbuf_printf(fb, "++");
        } else if (effect == OPEFFECT_PREDEC) {
            flexbuf_printf(fb, "--");
        }
        flexbuf_addstr(fb, reg->name);
        if (effect == OPEFFECT_POSTINC) {
            flexbuf_printf(fb, "++");
        } else if (effect == OPEFFECT_POSTDEC) {
            flexbuf_printf(fb, "--");
        }
        break;
    }
}

static void
PrintOperandSrc(struct flexbuf *fb, Operand *reg, enum OperandEffect effect)
{
    doPrintOperand(fb, reg, 1, effect);
}

static void
PrintOperand(struct flexbuf *fb, Operand *reg)
{
    doPrintOperand(fb, reg, 0, OPEFFECT_NONE);
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
        if (gl_p2) {
            flexbuf_addstr(fb, "@");
        } else {
            flexbuf_addstr(fb, "@@@");
        }
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
    case REG_COGPTR:
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
    Flexbuf *databuf;
    Flexbuf *relocbuf;
    uint32_t *data;
    int len;
    int addr;
    Reloc *nextreloc;
    int relocs;
    
    if (op->kind != IMM_BINARY) {
        ERROR(NULL, "Internal: bad binary blob");
        return;
    }
    flexbuf_printf(fb, "\tlong\n"); // ensure long alignment
    flexbuf_printf(fb, label->name);
    flexbuf_printf(fb, "\n");
    databuf = (Flexbuf *)op->name;
    relocbuf = (Flexbuf *)op->val;
    if (relocbuf) {
        relocs = flexbuf_curlen(relocbuf) / sizeof(Reloc);
        nextreloc = (Reloc *)flexbuf_peek(relocbuf);
    } else {
        relocs = 0;
        nextreloc = NULL;
    }
    len = flexbuf_curlen(databuf);
    // make sure it is a multiple of 4
    while ( 0 != (len & 3) ) {
        flexbuf_addchar(databuf, 0);
        len = flexbuf_curlen(databuf);
    }
    data = (uint32_t *)flexbuf_peek(databuf);
    for (addr = 0; addr < len; addr += 4) {
        flexbuf_printf(fb, "\tlong\t");
        if (relocs > 0) {
            // see if this particular long needs a reloc
            if (nextreloc->addr == addr) {
                int offset = nextreloc->value;
                if (offset == 0) {
                    flexbuf_printf(fb, "@@@%s\n", label->name);
                } else if (offset > 0) {
                    flexbuf_printf(fb, "@@@%s + %d\n", label->name, offset);
                } else {
                    flexbuf_printf(fb, "@@@%s - %d\n", label->name, -offset);
                }
                data++;
                nextreloc++;
                --relocs;
                continue;
            }
        }
        flexbuf_printf(fb, "$%08x\n", data[0]);
        data ++;
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

//
// emit public methods for Spin
// this is needed if we want to assemble the .pasm output with
// bstc or some similar compiler
//
void
EmitSpinMethods(struct flexbuf *fb, Module *P)
{
    if (!P) return;
    if (gl_output == OUTPUT_COGSPIN) {
        int varlen = P->varsize;
        varlen = (varlen + 3) & ~3; // round up to long boundary
        if (varlen < 1) varlen = 1;
        Function *f;
        
        flexbuf_addstr(fb, "VAR\n");
        flexbuf_addstr(fb, "  long __mbox[__MBOX_SIZE]\n");
        flexbuf_printf(fb, "  long __objmem[%d]\n", varlen / 4);
        flexbuf_addstr(fb, "  long __stack[__STACK_SIZE]\n");
        flexbuf_addstr(fb, "  byte __cognum\n\n");

        flexbuf_addstr(fb, "PUB __cognew\n");
        flexbuf_addstr(fb, "  if (__cognum == 0)\n");
        flexbuf_addstr(fb, "    longfill(@__mbox, 0, __MBOX_SIZE)\n");
        flexbuf_addstr(fb, "    __mbox[0] := @__objmem\n");
        flexbuf_addstr(fb, "    __mbox[1] := @__stack\n");
        flexbuf_addstr(fb, "    __cognum := cognew(@entry, @__mbox) + 1\n");
        flexbuf_addstr(fb, "  return __cognum\n\n");

        flexbuf_addstr(fb, "PUB __cogstop\n");
        flexbuf_addstr(fb, "  if __cognum\n");
        flexbuf_addstr(fb, "    __lock  ' wait until everyone else is finished\n");
        flexbuf_addstr(fb, "    cogstop(__cognum~ - 1)\n\n");

        flexbuf_addstr(fb, "PRI __lock\n");
        flexbuf_addstr(fb, "  repeat\n");
        flexbuf_addstr(fb, "    repeat until __mbox[0] == 0\n");
        flexbuf_addstr(fb, "    __mbox[0] := __cognum\n");
        flexbuf_addstr(fb, "  until __mbox[0] == __cognum\n\n");
        flexbuf_addstr(fb, "  repeat until __mbox[1] == 0\n");

        flexbuf_addstr(fb, "PRI __unlock\n");
        flexbuf_addstr(fb, "  __mbox[0] := 0\n\n");

        flexbuf_addstr(fb, "PRI __invoke(func, getresult) : r\n");
        flexbuf_addstr(fb, "  __mbox[1] := func - @entry\n");
        flexbuf_addstr(fb, "  if getresult\n");
        flexbuf_addstr(fb, "    repeat until __mbox[1] == 0\n");
        flexbuf_addstr(fb, "    r := __mbox[2]\n");
        flexbuf_addstr(fb, "  __unlock\n");
        flexbuf_addstr(fb, "  return r\n\n");

        // now we have to create the stub functions
        for (f = P->functions; f; f = f->next) {
            if (f->is_public) {
                AST *list = f->params;
                AST *ast;
                int paramnum = 2;
                int needcomma = 0;
                int synchronous;
                flexbuf_printf(fb, "PUB %s", f->name);
                if (list) {
                    flexbuf_addstr(fb, "(");
                    while (list) {
                        ast = list->left;
                        if (needcomma) {
                            flexbuf_addstr(fb, ", ");
                        }
                        flexbuf_addstr(fb, ast->d.string);
                        needcomma = 1;
                        list = list->right;
                    }
                    flexbuf_addstr(fb, ")");
                }
                flexbuf_addstr(fb, "\n");
                flexbuf_addstr(fb, "  __lock\n");
                list = f->params;
                while (list) {
                    flexbuf_printf(fb, "  __mbox[%d] := %s\n", paramnum, list->left->d.string);
                    list = list->right;
                    paramnum++;
                }
                // if there's a result from the function, make this call
                // synchronous
                if (f->rettype && f->rettype->kind == AST_VOIDTYPE)
                    synchronous = 0;
                else
                    synchronous = 1;
                flexbuf_printf(fb, "  return __invoke(@pasm_%s, %d)\n\n", f->name, synchronous);
            }
        }
    } else {
        flexbuf_addstr(fb, "PUB main\n");
        flexbuf_addstr(fb, "  coginit(0, @entry, 0)\n");
    }        
}

// LMM jumps +- this amount are turned into add/sub of the pc
// pick a conservative value
// 127 would be the absolute maximum here
#define MAX_REL_JUMP_OFFSET 100

/* convert IR list into assembly language */
static int didPub = 0;

void
DoAssembleIR(struct flexbuf *fb, IR *ir, Module *P)
{
    const char *str;
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
        if (!didPub && P) {
            EmitSpinMethods(fb, P);
            didPub = 1;
        }
        flexbuf_addstr(fb, "DAT\n");
        inCon = 0;
        inDat = 1;
        if (!didOrg) {
            flexbuf_addstr(fb, "\torg\t0\n");
            didOrg = 1;
        }
    }
    if (!gl_p2) {
        // handle jumps in LMM mode
        switch (ir->opc) {
        case OPC_CALL:
            if (IsHubDest(ir->dst)) {
                if (!lmmMode) {
                    // call of hub function from COG
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "mov\tpc, $+2\n");
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "call\t#LMM_CALL_FROM_COG\n");
                } else {
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "jmp\t#LMM_CALL\n");
                }
                flexbuf_addstr(fb, "\tlong\t");
                if (ir->dst->kind != IMM_HUB_LABEL) {
                    ERROR(NULL, "internal error: non-hub label in LMM jump");
                }
                PrintOperandAsValue(fb, ir->dst);
                flexbuf_addstr(fb, "\n");
                return;
            }
            break;
        case OPC_DJNZ:
            if (ir->fcache) {
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "djnz\t");
                PrintOperand(fb, ir->dst);
                flexbuf_addstr(fb, ", #LMM_FCACHE_START + (");
                PrintOperand(fb, ir->src);
                flexbuf_addstr(fb, " - ");
                PrintOperand(fb, ir->fcache);
                flexbuf_addstr(fb, ")\n");
                return;
            } else if (IsHubDest(ir->src)) {
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "djnz\t");
                PrintOperand(fb, ir->dst);
                flexbuf_addstr(fb, ", #LMM_JUMP\n");
                flexbuf_addstr(fb, "\tlong\t");
                if (ir->src->kind != IMM_HUB_LABEL) {
                    ERROR(NULL, "internal error: non-hub label in LMM jump");
                }
                PrintOperandAsValue(fb, ir->src);
                flexbuf_addstr(fb, "\n");
                return;
            }
            break;
        case OPC_JUMP:
            if (ir->fcache) {
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "jmp\t#LMM_FCACHE_START + (");
                PrintOperand(fb, ir->dst);
                flexbuf_addstr(fb, " - ");
                PrintOperand(fb, ir->fcache);
                flexbuf_addstr(fb, ")\n");
                return;
            } else if (IsHubDest(ir->dst)) {
                IR *dest;
                if (!lmmMode) {
                    ERROR(NULL, "jump from COG to LMM not supported yet");
                }
                if (ir->dst->kind != IMM_HUB_LABEL) {
                    ERROR(NULL, "internal error: non-hub label in LMM jump");
                }
                PrintCond(fb, ir->cond);
                // if we know the destination we may be able to optimize
                // the branch
                if (ir->aux) {
                    int offset;
                    dest = (IR *)ir->aux;
                    offset = dest->addr - ir->addr;
                    if ( offset > 0 && offset < MAX_REL_JUMP_OFFSET) {
                        flexbuf_printf(fb, "add\tpc, #4*(");
                        PrintOperand(fb, ir->dst);
                        flexbuf_printf(fb, " - ($+1))\n");
                        return;
                    }
                    if ( offset < 0 && offset > -MAX_REL_JUMP_OFFSET) {
                        flexbuf_printf(fb, "sub\tpc, #4*(($+1) - ");
                        PrintOperand(fb, ir->dst);
                        flexbuf_printf(fb, ")\n");
                        return;
                    }
                }
                flexbuf_addstr(fb, "rdlong\tpc,pc\n");
                flexbuf_addstr(fb, "\tlong\t");
                PrintOperandAsValue(fb, ir->dst);
                flexbuf_addstr(fb, "\n");
                return;
            }
            break;
        case OPC_RET:
            if (ir->fcache) {
                ERROR(NULL, "return from fcached code not supported");
                return;
            } else if (lmmMode) {
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "jmp\t#LMM_RET\n");
                return;
            }
        default:
            break;
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
        case P2_JUMP:
        case P2_DST_CONST_OK:
            flexbuf_addstr(fb, "\t");
            PrintOperandSrc(fb, ir->dst, OPEFFECT_NONE);
            break;
        default:
            flexbuf_addstr(fb, "\t");
            PrintOperand(fb, ir->dst);
            flexbuf_addstr(fb, ", ");
            PrintOperandSrc(fb, ir->src, ir->srceffect);
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
#if 0        
        if (ir->flags & FLAG_KEEP_INSTR) {
            flexbuf_printf(fb, " '' (volatile)");
        }
#endif
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
        if (ir->dst->kind != IMM_STRING) {
            ERROR(NULL, "COMMENT is not a string");
            return;
        }
        flexbuf_addstr(fb, "' ");
        str = ir->dst->name;
        while (*str && *str != '\n') {
            flexbuf_addchar(fb, *str);
            str++;
        }
        flexbuf_addchar(fb, '\n');
        break;
        /* fall through */
    case OPC_LITERAL:
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
        if (ir->src) {
            // repeat count
            flexbuf_addstr(fb, "[");
            PrintOperandAsValue(fb, ir->src);
            flexbuf_addstr(fb, "]");
        }
        flexbuf_addstr(fb, "\n");
	break;
    case OPC_RESERVE:
        flexbuf_printf(fb, "\tres\t");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_addstr(fb, "\n");
        break;
    case OPC_RESERVEH:
        flexbuf_printf(fb, "\tlong\t0[");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_addstr(fb, "]\n");
        break;
    case OPC_FCACHE:
        flexbuf_printf(fb, "\tcall\t#LMM_FCACHE_LOAD\n");
        flexbuf_printf(fb, "\tlong\t(");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_printf(fb, "-");
        PrintOperandAsValue(fb, ir->src);
        flexbuf_printf(fb, ")\n");
        break;
    case OPC_LABELED_BLOB:
        // output a binary blob
        // dst has a label
        // data is in a string in src
        OutputBlob(fb, ir->dst, ir->src);
        break;
    case OPC_FIT:
        flexbuf_addstr(fb, "\tfit\t496\n");
        break;
    case OPC_ORG:
        flexbuf_printf(fb, "\torg\t");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_printf(fb, "\n");
        break;
    case OPC_HUBMODE:
        if (gl_p2) {
            flexbuf_addstr(fb, "\torgh\t$800\n");
        }
        lmmMode = 1;
        break;
    default:
        ERROR(NULL, "Internal error: unable to process IR\n");
        break;
    }
}

/* assemble an IR list */
char *
IRAssemble(IRList *list, Module *P)
{
    IR *ir;
    struct flexbuf fb;
    char *ret;
    
    inDat = 0;
    inCon = 0;
    didOrg = 0;
    didPub = 0;
    lmmMode = 0;
    
    if (gl_p2) {
        didPub = 1; // we do not want PUB declaration in P2 code
    }
    flexbuf_init(&fb, 512);
    for (ir = list->head; ir; ir = ir->next) {
        DoAssembleIR(&fb, ir, P);
    }
    flexbuf_addchar(&fb, 0);
    ret = flexbuf_get(&fb);
    flexbuf_delete(&fb);
    return ret;
}

void
DumpIRL(IRList *irl)
{
    puts(IRAssemble(irl, NULL));
}
