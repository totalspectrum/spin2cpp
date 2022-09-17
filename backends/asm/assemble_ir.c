/*
 * Spin to C/C++ translator
 * Copyright 2016-2022 Total Spectrum Software Inc.
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
#include "becommon.h"
#include "outasm.h"
#include <ctype.h>

// used for converting Spin relative addresses to absolute addresses
// (only needed for OUTPUT_COGSPIN)
static int fixup_number = 0;
static int pending_fixup = 0;

// flags for what has been output so far
static int inDat;
static int inCon;
static int didOrg;
static int lmmMode;

// table for remapping label names
static SymbolTable localLabelMap;

// we remap local names (starting with L__0xxx)
// this is to help with debugging and testing; almost any trivial
// change in optimization can change the numbers associated with labels,
// which in turn makes tests fail with trivial changes (e.g. L_0001 => L_0003)
// to avoid this we do a remapping of all the temporary labels in the program
static bool mayRemap(const char *name)
{
    if (name[0] == 'L' && name[1] == '_' && name[2] == '_' && isdigit(name[3])) {
        return true;
    }
    return false;
}

#define MAX_BUF 128

static const char *QuotedName(const char *orig_name)
{
    static char buf[MAX_BUF];
    const char *name = orig_name;
    int i = 0;
    int c;
    while (i < MAX_BUF-2) {
        c = *name++;
        if (c == 0) break;
        switch(c) {
        case '!':
        case '$':
        case '%':
        case '#':
            buf[i++] = '`';
            break;
        case ' ':
        case '\n':
            return orig_name;
        default:
            break;
        }
        buf[i++] = c;
    }
    buf[i++] = 0;
    return buf;
}

static const char *RemappedName(const char *name)
{
    Symbol *sym;
    unsigned num;
    static char buf[MAX_BUF];
    
    if (!mayRemap(name))
        return QuotedName(name);
    sym = FindSymbol(&localLabelMap, name);
    if (!sym) return QuotedName(name);
    num = (uintptr_t)sym->val;
    sprintf(buf, "LR__%04u", num);
    return buf;
}

// helper function for printing operands
static void
doPrintOperand(struct flexbuf *fb, Operand *reg, int useimm, enum OperandEffect effect_orig)
{
    char temp[128];
    const char *regname;
    int usehubaddr;
    int useabsaddr;
    int opoffset;
    int skipimm;
    unsigned effect = (unsigned)effect_orig;
    
    opoffset = ((int)effect) >> OPEFFECT_OFFSET_SHIFT;
    effect &= ~(OPEFFECT_OFFSET_MASK);

    skipimm = ((int)effect) & OPEFFECT_NOIMM;
    effect &= ~(OPEFFECT_NOIMM);
    
    usehubaddr = effect & OPEFFECT_FORCEHUB;
    effect &= ~(OPEFFECT_FORCEHUB);
    if (gl_p2) {
        useabsaddr = effect & OPEFFECT_FORCEABS;
        effect &= ~OPEFFECT_FORCEABS;
    } else {
        useabsaddr = 0;
    }
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
    case IMM_PCRELATIVE:
        ERROR(NULL, "internal error, pcrelative operand found");
        break;
    case IMM_INT:
        if (reg->val >= 0 && reg->val < 512) {
            flexbuf_addstr(fb, "#");
            if (reg->name && reg->name[0]) {
                flexbuf_addstr(fb, RemappedName(reg->name));
            } else {
                sprintf(temp, "%d", (int)(int32_t)reg->val);
                flexbuf_addstr(fb, temp);
            }
        } else if (gl_p2) {
            flexbuf_addstr(fb, "##");
            if (reg->name && reg->name[0]) {
                flexbuf_addstr(fb, RemappedName(reg->name));
            } else {
                sprintf(temp, "%d", (int)(int32_t)reg->val);
                flexbuf_addstr(fb, temp);
            }            
        } else {
            // the immediate actually got processed as a register
            flexbuf_addstr(fb, RemappedName(reg->name));
        }
        break;
    case HUBMEM_REF:
    case COGMEM_REF:
    {
        Operand *regptr = NULL;
        if (gl_p2 && useimm) {
            regptr = (Operand *)reg->name;
            int offset = reg->val;
            if (regptr && regptr->kind == REG_HUBPTR) {
                regptr = (Operand *)regptr->val;
                flexbuf_printf(fb, "#@%s + %d", regptr->name, offset);
            } else {
                regptr = NULL;
            }
        }
        if (!regptr) {
            ERROR(NULL, "Internal error, tried to use memory directly");
            flexbuf_addstr(fb, "#@@@#");
        }
        break;
    }
    case STRING_DEF:
        if (gl_p2 && useimm) {
            flexbuf_addstr(fb, "##");
            if (useabsaddr) {
                flexbuf_addstr(fb, "\\");
            }
            flexbuf_addstr(fb, "@");
        } else {
            ERROR(NULL, "Internal error, tried to use string directly");
            flexbuf_addstr(fb, "#@@@");
        }
        flexbuf_addstr(fb, RemappedName(reg->name));
        break;
    case IMM_HUB_LABEL:
        if (gl_p2 && useimm) {
            flexbuf_addstr(fb, "#");
            if (useabsaddr) {
                flexbuf_addstr(fb, "\\");
                flexbuf_addstr(fb, "@");
            }
        }
        
        flexbuf_addstr(fb, RemappedName(reg->name));
        break;
    default:
         if (!useabsaddr) {
            useimm = 0;
        }
        /* fall through */
   case IMM_COG_LABEL:
        if (useimm && !skipimm) {
            flexbuf_addstr(fb, "#");
            if (useabsaddr) {
                flexbuf_addstr(fb, "\\");
            }
            if (usehubaddr) {
                flexbuf_addstr(fb, "@");
            }
        }
        if (effect == OPEFFECT_PREINC) {
            flexbuf_printf(fb, "++");
        } else if (effect == OPEFFECT_PREDEC) {
            flexbuf_printf(fb, "--");
        }
        if (reg->kind == REG_SUBREG) {
            regname = OffsetName( ((Operand *)reg->name)->name, reg->val );
        } else {
            regname = reg->name;
        }
        flexbuf_addstr(fb, RemappedName(regname));
        if ( (reg->kind == REG_HW || reg->kind == IMM_COG_LABEL) && reg->val != 0) {
            flexbuf_printf(fb, " + %d", reg->val);
        }
        if (effect == OPEFFECT_POSTINC) {
            flexbuf_printf(fb, "++");
        } else if (effect == OPEFFECT_POSTDEC) {
            flexbuf_printf(fb, "--");
        } else if (opoffset) {
            if (opoffset > 31 || opoffset < -32) {
                flexbuf_printf(fb, "[##%d]", opoffset);
            } else {
                flexbuf_printf(fb, "[%d]", opoffset);
            }
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
        } else if (gl_output == OUTPUT_COGSPIN) {
            // record fixup info
            if (fixup_number > 0) {
                flexbuf_printf(fb, "( (@__fixup_%d - 4) << 16) + @", fixup_number);
            } else {
                flexbuf_addstr(fb, "@");
            }
            fixup_number++;
            pending_fixup = fixup_number;
        } else {
            flexbuf_addstr(fb, "@@@");
        }
        // fall through
    case IMM_COG_LABEL:
        flexbuf_addstr(fb, RemappedName(reg->name));
        break;
    case IMM_STRING:
    {
        const char *s = reg->name;
        int c;
        int needquote = 1;
        int needcomma = 0;
        
        while ( (c = *s++) != 0) {
            if (c < 0x20 || c >= 0x7f || c == '\"') {
                if (needquote == 0) {
                    flexbuf_addchar(fb, '"');
                    needquote = needcomma = 1;
                }
                if (needcomma == 1) {
                    flexbuf_addchar(fb, ',');
                }
                flexbuf_printf(fb, "%d", c);
                needcomma = 1;
            } else {
                if (needquote) {
                    if (needcomma) {
                        flexbuf_addchar(fb, ',');
                        needcomma = 0;
                    }
                    flexbuf_addchar(fb, '"');
                    needquote = 0;
                }
                flexbuf_addchar(fb, c);
            }
        }
        if (needquote == 0) {
            flexbuf_addchar(fb, '"');
        }
        break;
    }
    case REG_HUBPTR:
    case REG_COGPTR:
        indirect = (Operand *)reg->val;
        flexbuf_addstr(fb, RemappedName(indirect->name));
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
    // case COND_C:
    //   flexbuf_addstr(fb, " if_c");
    //   break;
    // case COND_NC:
    //   flexbuf_addstr(fb, " if_nc");
    //   break;
    // case COND_NC_AND_NZ:
    //   flexbuf_addstr(fb, " if_nc_and_nz");
    //   break;
    case COND_NC_AND_Z:
      flexbuf_addstr(fb, " if_nc_and_z");
      break;        
    case COND_C_AND_NZ:
      flexbuf_addstr(fb, " if_c_and_nz");
      break;
    case COND_C_AND_Z:
      flexbuf_addstr(fb, " if_c_and_z");
      break;        
    case COND_C_OR_NZ:
      flexbuf_addstr(fb, " if_c_or_nz");
      break;        
    case COND_NC_OR_NZ:
      flexbuf_addstr(fb, " if_nc_or_nz");
      break;        
    case COND_NC_OR_Z:
      flexbuf_addstr(fb, " if_nc_or_z");
      break;        
    case COND_C_EQ_Z:
      flexbuf_addstr(fb, " if_c_eq_z");
      break;
    case COND_C_NE_Z:
      flexbuf_addstr(fb, " if_c_ne_z");
      break;
    default:
      ERROR(NULL, "Internal error, unexpected condition");
      flexbuf_addstr(fb, " if_??");
      break;
    }
    flexbuf_addchar(fb, '\t');
}

static uint32_t
fetchUint32(uint8_t *data)
{
    uint32_t r;
    r = data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
    return r;
}

#define MAX_BYTES_ON_LINE 16

void
OutputAlignLong(Flexbuf *fb)
{
    if (gl_p2 || gl_compress) {
        flexbuf_printf(fb, "\talignl\n"); // ensure long alignment
    } else {
        flexbuf_printf(fb, "\tlong\n"); // ensure long alignment
    }
}

void
OutputDataBlob(Flexbuf *fb, Flexbuf *databuf, Flexbuf *relocbuf, const char *startLabel)
{
    uint8_t *data;
    int len;
    int addr;
    Reloc *nextreloc;
    int relocs;
    uint32_t runlen;
    int lastdata;

    if (startLabel) {
        OutputAlignLong(fb);
        flexbuf_printf(fb, startLabel);
        flexbuf_printf(fb, "\n");
    }
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
    data = (uint8_t *)flexbuf_peek(databuf);
    addr = 0;
    while (addr < len) {
        // figure out how many bytes we can output
        int bytesPending = len - addr;
        int bytesToReloc;

    again:
        if (relocs > 0) {
            bytesToReloc = nextreloc->addr - addr;
            if (bytesToReloc == 0) {
                int32_t offset;
                uint32_t baseWord;
                Symbol *sym;
                const char *symname;
                
                // we have to output a relocation or debug entry now
                if (nextreloc->kind == RELOC_KIND_DEBUG) {
                    LineInfo *info = (LineInfo *)nextreloc->sym;
                    if (info && info->linedata) {
                        flexbuf_printf(fb, "'-' %s", info->linedata);
                    }
                    nextreloc++;
                    --relocs;
                    goto again;
                }
                if (nextreloc->kind == RELOC_KIND_I32) {
                    if (bytesPending < 4) {
                        ERROR(NULL, "Internal error, not enough space for reloc");
                        return;
                    }
                } else if (nextreloc->kind == RELOC_KIND_AUGD || nextreloc->kind == RELOC_KIND_AUGS) {
                    if (bytesPending < 8) {
                        ERROR(NULL, "Internal error, not enough space for reloc");
                        return;
                    }
                } else if (nextreloc->kind == RELOC_KIND_NONE) {
                    // reloc was cancelled
                    nextreloc++;
                    --relocs;
                    goto again;
                } else {
                    ERROR(NULL, "Internal error, bad reloc kind %d", nextreloc->kind);
                    return;
                }
                flexbuf_printf(fb, "\tlong\t");
                sym = nextreloc->sym;
                offset = nextreloc->symoff;
                if (!sym) {
                    symname = startLabel;
                } else {
                    symname = BackendNameForSymbol(sym);
                }
                switch(nextreloc->kind) {
                case RELOC_KIND_I32:
                    if (offset == 0) {
                        flexbuf_printf(fb, "@@@%s\n", symname);
                    } else if (offset > 0) {
                        flexbuf_printf(fb, "@@@%s + %d\n", symname, offset);
                    } else {
                        flexbuf_printf(fb, "@@@%s - %d\n", symname, -offset);
                    }
                    data += 4;
                    addr += 4;
                    break;
                case RELOC_KIND_AUGS:
                case RELOC_KIND_AUGD:
                    baseWord = fetchUint32(data);
                    flexbuf_printf(fb, "((@@@%s + %d)>>9) | $%08x\n", symname, offset, baseWord);
                    data += 4;
                    addr += 4;
                    baseWord = fetchUint32(data);
                    if (nextreloc->kind == RELOC_KIND_AUGD) {
                        flexbuf_printf(fb, "\tlong\t(((@@@%s + %d)&$1ff)<<9) | $%08x\n", symname, offset, baseWord);
                    } else {
                        flexbuf_printf(fb, "\tlong\t((@@@%s + %d)&$1ff) | $%08x\n", symname, offset, baseWord);
                    }
                    data += 4;
                    addr += 4;
                    break;
                default:
                    ERROR(NULL, "internal error in relocs");
                    break;
                }
                nextreloc++;
                --relocs;
                continue;
            }

            if (bytesPending > bytesToReloc) {
                bytesPending = bytesToReloc;
            }
        }

        /* if we have more than 7 bytes pending, look for runs of data */
        if (bytesPending > MAX_BYTES_ON_LINE) {
            /* check for a run of data */
            runlen = 0;
            lastdata = data[0];
            while (addr+runlen < len && runlen < bytesPending && data[runlen] == lastdata) {
                runlen++;
            }
            if (runlen > 4) {
                flexbuf_printf(fb, "\tbyte\t$%02x[%d]\n", lastdata, runlen);
                addr += runlen;
                data += runlen;
                continue;
            }
        }
        /* try to chunk into longs if we can */
        if (bytesPending > MAX_BYTES_ON_LINE) {
            bytesPending = MAX_BYTES_ON_LINE;
        }
        flexbuf_printf(fb, "\tbyte\t$%02x", data[0]);
        data++;
        addr++;
        bytesPending--;
        while (bytesPending > 0) {
            flexbuf_printf(fb, ", $%02x", data[0]);
            data++; addr++; --bytesPending;
        }
        flexbuf_printf(fb,"\n");
    }
    while (relocs > 0 && nextreloc) {
        if (nextreloc->kind == RELOC_KIND_DEBUG) {
            LineInfo *info = (LineInfo *)nextreloc->sym;
            if (info && info->linedata) {
                flexbuf_printf(fb, "'-' %s", info->linedata);
            }
        }
        nextreloc++;
        --relocs;
    }
}

static void
OutputBlob(Flexbuf *fb, Operand *label, Operand *op, Module *P)
{
    Flexbuf *databuf;
    Flexbuf *relocbuf;
    const char *baseLabel;
    
    if (op->kind != IMM_BINARY) {
        ERROR(NULL, "Internal: bad binary blob");
        return;
    }
    baseLabel = RemappedName(label->name);
    databuf = (Flexbuf *)op->name;
    relocbuf = (Flexbuf *)op->val;
    OutputDataBlob(fb, databuf, relocbuf, baseLabel);
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
    AST *rettype;
    if (!P) return;
    if (gl_output == OUTPUT_COGSPIN) {
        int varlen = P->varsize;
        varlen = (varlen + 3) & ~3; // round up to long boundary
        if (varlen < 1) varlen = 1;
        Function *f;
        
        flexbuf_addstr(fb, "var\n");
        flexbuf_addstr(fb, "  long __mbox[__MBOX_SIZE]   ' mailbox for communicating with remote COG\n");
        flexbuf_printf(fb, "  long __objmem[%d]          ' space for hub data in COG code\n", varlen / 4);
        flexbuf_addstr(fb, "  long __stack[__STACK_SIZE] ' stack for new COG\n");
        flexbuf_addstr(fb, "  byte __cognum              ' 1 + the ID of the running COG (0 if nothing running)\n\n");

        flexbuf_addstr(fb, "'' Code to start the object running in its own COG\n");
        flexbuf_addstr(fb, "'' This must always be called before any other methods\n");
        flexbuf_addstr(fb, "pub __coginit(id)\n");
        flexbuf_addstr(fb, "  if (__cognum == 0) ' if the cog isn't running yet\n");
        flexbuf_addstr(fb, "    __fixup_addresses\n");
        flexbuf_addstr(fb, "    longfill(@__mbox, 0, __MBOX_SIZE)\n");
        if (gl_p2) {
            flexbuf_addstr(fb, "    __mbox[1] := @entry\n");
        } else {
            flexbuf_addstr(fb, "    __mbox[1] := @pasm__init - @entry\n");
        }
        flexbuf_addstr(fb, "    __mbox[2] := @__objmem\n");
        flexbuf_addstr(fb, "    __mbox[3] := @__stack\n");
        flexbuf_addstr(fb, "    if (id < 0)\n");
        flexbuf_addstr(fb, "      id := cognew(@entry, @__mbox)\n");
        flexbuf_addstr(fb, "    else\n");
        flexbuf_addstr(fb, "      coginit(id, @entry, @__mbox) ' actually start the cog\n");
        flexbuf_addstr(fb, "    __cognum := id + 1\n");
        flexbuf_addstr(fb, "  return id\n\n");

        flexbuf_addstr(fb, "pub __cognew\n");
        flexbuf_addstr(fb, "  return __coginit(-1)\n\n");
                       
        flexbuf_addstr(fb, "'' Code to stop the remote COG\n");
        flexbuf_addstr(fb, "pub __cogstop\n");
        flexbuf_addstr(fb, "  if __cognum\n");
        flexbuf_addstr(fb, "    __lock  ' wait until everyone else is finished\n");
        flexbuf_addstr(fb, "    cogstop(__cognum~ - 1)\n");
	flexbuf_addstr(fb, "    __mbox[0] := 0\n");
	flexbuf_addstr(fb, "    __cognum := 0\n\n");
        
        flexbuf_addstr(fb, "'' Code to lock access to the PASM COG\n");
        flexbuf_addstr(fb, "'' The idea here is that (in theory) multiple Spin bytecode threads might\n");
        flexbuf_addstr(fb, "'' want access to the PASM COG, so this lock makes sure they don't step on each other.\n");
        flexbuf_addstr(fb, "'' This method also makes sure the remote COG is idle and ready to receive commands.\n");
        flexbuf_addstr(fb, "pri __lock\n");
        flexbuf_addstr(fb, "  repeat\n");
        flexbuf_addstr(fb, "    repeat until __mbox[0] == 0   ' wait until no other Spin code is using remote\n");
        flexbuf_addstr(fb, "    __mbox[0] := __cognum         ' try to claim it\n");
        flexbuf_addstr(fb, "  until __mbox[0] == __cognum     ' make sure we really did get it\n\n");
        flexbuf_addstr(fb, "  repeat until __mbox[1] == 0     ' now wait for the COG itself to be idle\n\n");

        flexbuf_addstr(fb, "'' Code to release access to the PASM COG\n");
        flexbuf_addstr(fb, "pri __unlock\n");
        flexbuf_addstr(fb, "  __mbox[0] := 0\n\n");

        flexbuf_addstr(fb, "'' Check to see if the PASM COG is busy (still working on something)\n");
        flexbuf_addstr(fb, "pub __busy\n");
        flexbuf_addstr(fb, "  return __mbox[1] <> 0\n\n");

        flexbuf_addstr(fb, "'' Code to send a message to the remote COG asking it to perform a method\n");
        flexbuf_addstr(fb, "'' func is the PASM entrypoint of the method to perform\n");
        flexbuf_addstr(fb, "'' if getresult is nonzero then we wait for the remote COG to answer us with a result\n");
        flexbuf_addstr(fb, "'' if getresult is 0 then we continue without waiting (the remote COG runs in parallel\n");
        flexbuf_addstr(fb, "'' We must always call __lock before this, and set up the parameters starting in __mbox[2]\n");
        flexbuf_addstr(fb, "pri __invoke(func, getresult) : r\n");
        flexbuf_addstr(fb, "  __mbox[1] := func - @entry     ' set the function to perform (NB: this is a HUB address)\n");
        flexbuf_addstr(fb, "  if getresult                   ' if we should wait for an answer\n");
        flexbuf_addstr(fb, "    repeat until __mbox[1] == 0  ' wait for remote COG to be idle\n");
        flexbuf_addstr(fb, "    r := __mbox[2]               ' pick up remote COG result\n");
        flexbuf_addstr(fb, "  __unlock                       ' release to other COGs\n");
        flexbuf_addstr(fb, "  return r\n\n");

        flexbuf_addstr(fb, "'' Code to convert Spin relative addresses to absolute addresses\n");
        flexbuf_addstr(fb, "'' The PASM code contains some absolute pointers internally; but the\n");
        flexbuf_addstr(fb, "'' regular Spin compiler cannot emit these (bstc and fastspin can, with the\n");
        flexbuf_addstr(fb, "'' @@@ operator, but we don't want to rely on having those compilers).\n");
        flexbuf_addstr(fb, "'' So the compiler inserts a chain of fixups, with each entry having the Spin\n");
        flexbuf_addstr(fb, "'' relative address in the low word, and a pointer to the next fixup in the high word.\n");
        flexbuf_addstr(fb, "'' This code follows that chain and adjusts the relative addresses to absolute ones.\n");
        
        flexbuf_addstr(fb, "pri __fixup_addresses | ptr, nextptr, temp\n");
        flexbuf_addstr(fb, "  ptr := __fixup_ptr[0]\n");
        flexbuf_addstr(fb, "  repeat while (ptr)      ' the fixup chain is terminated with a 0 pointer\n");
        flexbuf_addstr(fb, "    ptr := @@ptr          ' point to next fixup\n");
        flexbuf_addstr(fb, "    temp := long[ptr]     ' get the data\n");
        flexbuf_addstr(fb, "    nextptr := temp >> 16 ' high 16 bits contains link to next fixup\n");
        flexbuf_addstr(fb, "    temp := temp & $ffff  ' low 16 bits contains real pointer\n");
        flexbuf_addstr(fb, "    long[ptr] := @@temp   ' replace fixup data with real pointer\n");
        flexbuf_addstr(fb, "    ptr := nextptr\n");
        flexbuf_addstr(fb, "  __fixup_ptr[0] := 0 ' mark fixups as done\n\n");

        flexbuf_addstr(fb, "'--------------------------------------------------\n");
        flexbuf_addstr(fb, "' Stub functions to perform remote calls to the COG\n");
        flexbuf_addstr(fb, "'--------------------------------------------------\n\n");
        
        // now we have to create the stub functions
        for (f = P->functions; f; f = f->next) {
            if (f->is_public) {
                AST *list = f->params;
                AST *ast;
                int paramnum = 2;
                int needcomma = 0;
                int synchronous;
                int i;
                flexbuf_printf(fb, "pub %s", f->name);
                if (list) {
                    flexbuf_addstr(fb, "(");
                    while (list) {
                        ast = list->left;
                        if (needcomma) {
                            flexbuf_addstr(fb, ", ");
                        }
                        flexbuf_addstr(fb, VarName(ast));
                        needcomma = 1;
                        list = list->right;
                    }
                    flexbuf_addstr(fb, ")");
                }
                if (f->numresults > 1) {
                    flexbuf_addstr(fb, " : r0");
                    for (i = 1; i < f->numresults; i++) {
                        flexbuf_printf(fb, ", r%d", i);
                    }
                }
                flexbuf_addstr(fb, "\n");
                flexbuf_addstr(fb, "  __lock\n");
                list = f->params;
                while (list) {
                    flexbuf_printf(fb, "  __mbox[%d] := %s\n", paramnum, VarName(list->left));
                    list = list->right;
                    paramnum++;
                }
                // if there's a result from the function, make this call
                // synchronous
                rettype = GetFunctionReturnType(f);
                if (rettype && rettype->kind == AST_VOIDTYPE)
                    synchronous = 0;
                else
                    synchronous = 1;
                if (f->numresults < 2) {
                    flexbuf_printf(fb, "  return __invoke(@pasm_%s, %d)\n\n", f->name, synchronous);
                } else {
                    // synchronous call, fetch all results
                    flexbuf_printf(fb, "  __mbox[1] := @pasm_%s - @entry\n", f->name);
                    flexbuf_printf(fb, "  repeat until __mbox[1] == 0\n");
                    for (i = 0; i < f->numresults; i++) {
                        flexbuf_printf(fb, "  r%d := __mbox[%d]\n", i, 2+i);
                    }
                    flexbuf_printf(fb, "  __unlock\n\n");
                }
            }
        }
        flexbuf_addstr(fb, "'--------------------------------------------------\n");
        flexbuf_addstr(fb, "' The converted object (Spin translated to PASM)\n");
        flexbuf_addstr(fb, "' This is the code that will run in the remote COG\n");
        flexbuf_addstr(fb, "'--------------------------------------------------\n\n");

    } else {
        flexbuf_addstr(fb, "pub main\n");
        flexbuf_addstr(fb, "  coginit(0, @entry, 0)\n");
    }        
}

// print a compressed local call to "labelStr"
static void
PrintCompressLocalCall(struct flexbuf *fb, const char *labelStr)
{
    flexbuf_printf(fb, "\tbyte\t$E0 + (%s>>8)\n\tbyte\t%s & $FF\n", labelStr, labelStr);
}

// print a conditional jump to "dst"
static void
PrintCompressCondJump(struct flexbuf *fb, int cond, Operand *dst)
{
    int flag = 0xd0;
    switch (cond) {
    case COND_TRUE:
        flag |= 0xf;
        break;
    case COND_EQ:
        flag |= 0xa;
        break;
    case COND_NE:
        flag |= 0x5;
        break;
    case COND_LT:
        flag |= 0xc;
        break;
    case COND_GE:
        flag |= 0x3;
        break;
    case COND_GT:
        flag |= 0x1;
        break;
    case COND_LE:
        flag |= 0xe;
        break;
    default:
        ERROR(NULL, "bad condition for compressed instruction");
        break;
    }
    flexbuf_printf(fb, "\tbyte\t$%02x\n", flag);
    flexbuf_addstr(fb, "\tword\t");
    PrintOperandAsValue(fb, dst);
    flexbuf_addstr(fb, "\n");
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
    if (ir->opc == OPC_COMMENT) {
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
        return;
    }
    if (ir->opc == OPC_DUMMY) {
        return;
    }
    if (ir->opc == OPC_REPEAT_END) {
        // not an actual instruction, just a marker for
        // avoiding moving instructions
        return;
    }
    if (ir->opc == OPC_CONST) {
        // handle const declaration
        if (!inCon) {
            flexbuf_addstr(fb, "con\n");
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
    if (!inDat && P) {
        if (!didPub && P) {
            EmitSpinMethods(fb, P);
            didPub = 1;
        }
        flexbuf_addstr(fb, "dat\n");
        inCon = 0;
        inDat = 1;
        if (!didOrg) {
            if (gl_p2 && !gl_no_coginit && gl_output != OUTPUT_COGSPIN) {
                unsigned int clkfreq, clkreg;
                // on P2, make room for CLKFREQ and CLKMODE
                if (!GetClkFreq(P, &clkfreq, &clkreg)) {
                    clkfreq = 160000000;
                    clkreg = 0x010007fb;
                }
                flexbuf_addstr(fb, "\tnop\n");
                flexbuf_addstr(fb, "\tcogid\tpa\n");
                flexbuf_printf(fb, "\tcoginit\tpa,##$%x\n", P2_HUB_BASE + 4);
                flexbuf_printf(fb, "\torgh\t$%x\n", P2_CONFIG_BASE);
                flexbuf_printf(fb, "\tlong\t0\t'reserved\n");
                flexbuf_printf(fb, "\tlong\t0 ' clock frequency: will default to %d\n", clkfreq); 
                flexbuf_printf(fb, "\tlong\t0 ' clock mode: will default to $%x\n", clkreg); 
                flexbuf_printf(fb, "\torgh\t$%x\n", P2_HUB_BASE);
                flexbuf_printf(fb, " _ret_\tmov\tresult1, #0\n");
            }
            flexbuf_addstr(fb, "\torg\t0\n");
            didOrg = 1;
        }
    }
    if (gl_p2) {
        // check for fcache stuff
        if (ir->fcache) {
            switch(ir->opc) {
            case OPC_JUMP:
            case OPC_DJNZ:
                // we hope all of these will be generated as relative branches
                break;
            case OPC_RET:
                ERROR(NULL, "call/return from fcached code not supported");
                break;
            default:
                break;
            }
        }
        if (gl_output == OUTPUT_COGSPIN) {
            // use call/ret instead of calla/reta
            // (this is obsolete, call/ret is now standard, but it's
            // also harmless)
            if (ir->opc == OPC_CALL) {
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "call\t");
                PrintOperandSrc(fb, ir->dst, ir->dsteffect);
                flexbuf_addstr(fb, "\n");
                return;
            }
            if (ir->opc == OPC_RET) {
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "ret\n");
                return;
            }
        }
        // watch out for djnz going out of range
        // (it can only address +-256 longs
        if (ir->opc == OPC_DJNZ) {
            if (ir->aux) {
                IR *dest = (IR *)ir->aux;
                int offset = dest->addr - ir->addr;
                if (offset < -MAX_REL_JUMP_OFFSET || offset > MAX_REL_JUMP_OFFSET) {
                    static int djzlab = 0;
                    djzlab++;
                    flexbuf_printf(fb, "\tdjz\t");
                    PrintOperand(fb, ir->dst);
                    flexbuf_printf(fb, ", #tmp_djnz_%03u\n", djzlab);
                    flexbuf_printf(fb, "\tjmp\t");
                    PrintOperandSrc(fb, ir->src, ir->srceffect);
                    flexbuf_printf(fb, "\ntmp_djnz_%03u\n", djzlab);
                    return;
                }
            }
        }
    } else {
        // handle jumps in LMM mode
        switch (ir->opc) {
        case OPC_CALL:
            if (IsHubDest(ir->dst)) {
                if (ir->dst->kind != IMM_HUB_LABEL) {
                    ERROR(NULL, "Internal error, non-hub label in LMM jump");
                }
                if (!lmmMode) {
                    // call of hub function from COG
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "mov\t__pc, $+2\n");
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "call\t#LMM_CALL_FROM_COG\n");
                    flexbuf_addstr(fb, "\tlong\t");
                    PrintOperandAsValue(fb, ir->dst);
                    flexbuf_addstr(fb, "\n");
                } else {
                    if (gl_lmm_kind == LMM_KIND_TRACE) {
                        static int retlabel;
                        PrintCond(fb, ir->cond);
                        flexbuf_addstr(fb, "call\t#LMM_PUSH\n");
                        flexbuf_printf(fb, "\tlong\t@@@LMM_ret_%04d\n", retlabel);
                        PrintCond(fb, ir->cond);
                        flexbuf_addstr(fb, "call\t#LMM_JUMP\n");
                        flexbuf_addstr(fb, "\tlong\t");
                        PrintOperandAsValue(fb, ir->dst);
                        flexbuf_addstr(fb, "\n");
                        flexbuf_printf(fb, "LMM_ret_%04d\n", retlabel);
                        ++retlabel;
                    } else if (gl_lmm_kind == LMM_KIND_COMPRESS) {
                        if (ir->cond != COND_TRUE) {
                            ERROR(NULL, "Internal error, compressing conditional call\n");
                        }
                        flexbuf_printf(fb, "\tbyte\t$C0\n");
                        flexbuf_addstr(fb, "\tword\t");
                        PrintOperandAsValue(fb, ir->dst);
                        flexbuf_addstr(fb, "\n");
                    } else {
                        PrintCond(fb, ir->cond);
                        flexbuf_addstr(fb, "call\t#LMM_CALL\n");
                        flexbuf_addstr(fb, "\tlong\t");
                        PrintOperandAsValue(fb, ir->dst);
                        flexbuf_addstr(fb, "\n");
                    }
                }
                return;
            } else if (IsLocalOrArg(ir->dst)) {
                if (lmmMode) {
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "mov\tLMM_NEW_PC, ");
                    PrintOperand(fb, ir->dst);
                    flexbuf_addstr(fb, "\n");
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "call\t#LMM_CALL_PTR\n");
                    return;
                } else if (!gl_p2) {
                    WARNING(NULL, "indirect function calls are not supported in COG mode");
                }
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
                /* we're going to issue a sub x, #1 and then if_nz jmp
                   if there's already a condition, we need the new
                   condition to be if_nz AND if_x jmp
                */
                int jmp_cond = COND_NE;
                if (ir->cond == COND_NC) {
                    jmp_cond = COND_NC_AND_NZ;
                } else if (ir->cond == COND_LE) {
                    /* <= 0 AND != 0 becomes < 0 */
                    jmp_cond = COND_LT;
                } else if (ir->cond == COND_GE) {
                    jmp_cond = COND_GT;
                } else if (ir->cond != COND_TRUE) {
                    ERROR(NULL, "Internal error, cannot do conditional djnz in HUB");
                }
                if (gl_lmm_kind == LMM_KIND_COMPRESS) {
                    flexbuf_addstr(fb, "byte $D0\n");
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "sub\t");
                    PrintOperand(fb, ir->dst);
                    flexbuf_addstr(fb,", #1 wz\n");
                    PrintCompressCondJump(fb, jmp_cond, ir->src);
                    return;
                } else if (gl_lmm_kind != LMM_KIND_ORIG) {
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "sub\t");
                    PrintOperand(fb, ir->dst);
                    flexbuf_addstr(fb,", #1 wz\n");
                    PrintCond(fb, (IRCond)jmp_cond);
                    flexbuf_addstr(fb, "call\t#LMM_JUMP\n");
                } else {
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "djnz\t");
                    PrintOperand(fb, ir->dst);
                    flexbuf_addstr(fb, ", #LMM_JUMP\n");
                }
                flexbuf_addstr(fb, "\tlong\t");
                if (ir->src->kind != IMM_HUB_LABEL) {
                    ERROR(NULL, "Internal error, non-hub label in LMM jump");
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
            } else if (IsLocalOrArg(ir->dst) && lmmMode) {
                if (lmmMode) {
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "mov\tLMM_NEW_PC, ");
                    PrintOperand(fb, ir->dst);
                    flexbuf_addstr(fb, "\n");
                    PrintCond(fb, ir->cond);
                    flexbuf_addstr(fb, "call\t#LMM_JUMP_PTR\n");
                    return;
                } else if (!gl_p2) {
                    WARNING(NULL, "indirect function calls are not supported in COG mode");
                }
            } else if (IsHubDest(ir->dst)) {
                IR *dest;
                if (!lmmMode) {
                    ERROR(NULL, "jump from COG to LMM not supported yet");
                }
                if (ir->dst->kind != IMM_HUB_LABEL) {
                    ERROR(NULL, "Internal error, non-hub label in LMM jump");
                }
                if (gl_lmm_kind == LMM_KIND_COMPRESS) {
                    PrintCompressCondJump(fb, ir->cond, ir->dst);
                    return;
                }                    
                PrintCond(fb, ir->cond);
                // if we know the destination we may be able to optimize
                // the branch
                if (ir->aux && gl_lmm_kind == LMM_KIND_ORIG && !(ir->flags & (FLAG_KEEP_INSTR|FLAG_JMPTABLE_INSTR))) {
                    int offset;
                    dest = (IR *)ir->aux;
                    offset = dest->addr - ir->addr;
                    if ( offset > 0 && offset < MAX_REL_JUMP_OFFSET) {
                        flexbuf_printf(fb, "add\t__pc, #4*(");
                        PrintOperand(fb, ir->dst);
                        flexbuf_printf(fb, " - ($+1))\n");
                        return;
                    }
                    if ( offset < 0 && offset > -MAX_REL_JUMP_OFFSET) {
                        flexbuf_printf(fb, "sub\t__pc, #4*(($+1) - ");
                        PrintOperand(fb, ir->dst);
                        flexbuf_printf(fb, ")\n");
                        return;
                    }
                }
                if ((ir->flags & FLAG_JMPTABLE_INSTR)) {
                    flexbuf_addstr(fb, "word\t");
                    PrintOperandAsValue(fb, ir->dst);
                } else {
                    if (gl_lmm_kind == LMM_KIND_ORIG) {
                        flexbuf_addstr(fb, "rdlong\t__pc,__pc\n");
                    } else {
                        flexbuf_addstr(fb, "call\t#LMM_JUMP\n");
                    }
                    flexbuf_addstr(fb, "\tlong\t");
                    PrintOperandAsValue(fb, ir->dst);
                }
                flexbuf_addstr(fb, "\n");
                return;
            }
            break;
        case OPC_RET:
            if (ir->fcache) {
                ERROR(NULL, "return from fcached code not supported");
                return;
            } else if (lmmMode) {
                if (gl_lmm_kind == LMM_KIND_COMPRESS) {
                    if (ir->cond != COND_TRUE) {
                        ERROR(NULL, "conditional return in compressed code");
                    }
                    PrintCompressLocalCall(fb, "LMM_RET");
                    return;
                }
                PrintCond(fb, ir->cond);
                flexbuf_addstr(fb, "call\t#LMM_RET\n");
                return;
            }
        default:
            break;
        }
    }

    if (ir->instr) {
        int ccset;

        if (ir->cond == COND_FALSE) {
            flexbuf_addstr(fb, "\tnop\n");
            return;
        }
        if (lmmMode && gl_lmm_kind == LMM_KIND_COMPRESS) {
            if (ir->cond == COND_TRUE) {
                flexbuf_addstr(fb, "\t<");
            } else {
                flexbuf_addstr(fb, "\tbyte $D0\n");
                PrintCond(fb, ir->cond);
            }
        } else {
            PrintCond(fb, ir->cond);
        }
        flexbuf_addstr(fb, ir->instr->name);
        switch (ir->instr->ops) {
        case NO_OPERANDS:
            break;
        case JMP_OPERAND:
        case SRC_OPERAND_ONLY:
        case DST_OPERAND_ONLY:
        case CALL_OPERAND:
        case P2_JUMP:
        case P2_DST_CONST_OK:
            flexbuf_addstr(fb, "\t");
            PrintOperandSrc(fb, ir->dst, ir->dsteffect);
            break;
        default:
            flexbuf_addstr(fb, "\t");
            if (ir->opc == OPC_REPEAT && ir->dst->kind != IMM_INT) {
                flexbuf_addstr(fb, "@");
            }
            if (ir->instr->ops == P2_RDWR_OPERANDS) {
                PrintOperandSrc(fb, ir->dst, ir->dsteffect);
            } else {
                PrintOperand(fb, ir->dst);
            }
            if (ir->src) {
                flexbuf_addstr(fb, ", ");
                PrintOperandSrc(fb, ir->src, ir->srceffect);
            }
            if (ir->src2) {
                flexbuf_addstr(fb, ", ");
                PrintOperandSrc(fb, ir->src2, OPEFFECT_NONE);
            }
            break;
        }
        ccset = ir->flags & (FLAG_WC|FLAG_WZ|FLAG_NR|FLAG_WR|FLAG_WCZ|FLAG_ANDC|FLAG_ANDZ|FLAG_ORC|FLAG_ORZ|FLAG_XORC|FLAG_XORZ);
        if (ccset) {
            const char *sepstring = " ";
            if (gl_p2 && ((FLAG_WC|FLAG_WZ) == (ccset & (FLAG_WC|FLAG_WZ)))) {
                flexbuf_printf(fb, "%swcz", sepstring);
                sepstring = ",";
            } else { 
                if (ccset & FLAG_WC) {
                    flexbuf_printf(fb, "%swc", sepstring);
                    sepstring = ",";
                }
                if (ccset & FLAG_WZ) {
                    flexbuf_printf(fb, "%swz", sepstring);
                    sepstring = ",";
                }
            }
            if (ccset & FLAG_NR) {
                flexbuf_printf(fb, "%snr", sepstring);
            } else if (ccset & FLAG_WR) {
                flexbuf_printf(fb, "%swr", sepstring);
            } else if (ccset & FLAG_WCZ) {
                flexbuf_printf(fb, "%swcz", sepstring);
            } else if (ccset & FLAG_ANDC) {
                flexbuf_printf(fb, "%sandc", sepstring);
            } else if (ccset & FLAG_ANDZ) {
                flexbuf_printf(fb, "%sandz", sepstring);
            } else if (ccset & FLAG_ORC) {
                flexbuf_printf(fb, "%sorc", sepstring);
            } else if (ccset & FLAG_ORZ) {
                flexbuf_printf(fb, "%sorz", sepstring);
            } else if (ccset & FLAG_XORC) {
                flexbuf_printf(fb, "%sxorc", sepstring);
            } else if (ccset & FLAG_XORZ) {
                flexbuf_printf(fb, "%sxorz", sepstring);
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
    case OPC_LIVE:
        /* no code necessary, internal opcode */
        flexbuf_addstr(fb, "\t'.live\t");
        flexbuf_addstr(fb, ir->dst->name);
        flexbuf_addstr(fb, "\n");
        break;
    case OPC_LITERAL:
        PrintOperand(fb, ir->dst);
	break;
    case OPC_LABEL:
        flexbuf_addstr(fb, RemappedName(ir->dst->name));
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
    case OPC_ALIGNL:
        flexbuf_printf(fb, "\tlong\n");
        break;
    case OPC_FCACHE:
        if (gl_p2) {
            flexbuf_printf(fb, "\tloc\tpa,\t#(");
            PrintOperandAsValue(fb, ir->dst);
            flexbuf_printf(fb, "-");
            PrintOperandAsValue(fb, ir->src);
            flexbuf_printf(fb, ")\n");
            flexbuf_printf(fb, "\tcall\t#FCACHE_LOAD_\n");
        } else {
            flexbuf_printf(fb, "\tcall\t#LMM_FCACHE_LOAD\n");
            flexbuf_printf(fb, "\tlong\t(");
            PrintOperandAsValue(fb, ir->dst);
            flexbuf_printf(fb, "-");
            PrintOperandAsValue(fb, ir->src);
            flexbuf_printf(fb, ")\n");
        }
        break;
    case OPC_LABELED_BLOB:
        // output a binary blob
        // dst has a label
        // data is in a string in src
        // src2 is re-used as a pointer to the module
        OutputBlob(fb, ir->dst, ir->src, (Module *)ir->src2);
        break;
    case OPC_FIT:
        flexbuf_printf(fb, "\tfit\t");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_printf(fb, "\n");
        break;
    case OPC_ORG:
        flexbuf_printf(fb, "\torg\t");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_printf(fb, "\n");
        break;
    case OPC_ORGF:
        flexbuf_printf(fb, "\torgf\t");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_printf(fb, "\n");
        break;
    case OPC_HUBMODE:
        if (gl_p2) {
            flexbuf_printf(fb, "\torgh\n");
        }
        lmmMode = 1;
        break;
    case OPC_COMPRESS3:
        flexbuf_addstr(fb, "\tbyte\t(");
        PrintOperandAsValue(fb, ir->src2);
        flexbuf_addstr(fb, "<<3) + ((");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_addstr(fb, ">>8)<<2) + ((");
        PrintOperandAsValue(fb, ir->src);
        flexbuf_addstr(fb, ">>8)<<1)");
        if (!IsRegister(ir->src->kind)) {
            flexbuf_addstr(fb, " + 1");
        }
        flexbuf_addstr(fb, ", (");
        PrintOperandAsValue(fb, ir->dst);
        flexbuf_addstr(fb, ") & $FF, (");
        PrintOperandAsValue(fb, ir->src);
        flexbuf_addstr(fb, ") & $FF\n");
        break;
    default:
        ERROR(NULL, "Internal error, unable to process IR\n");
        break;
    }
}

static void
RenameLabels(IRList *list)
{
    IR *ir;
    Symbol *sym;
    uintptr_t labelValue = 1;
    
    for (ir = list->head; ir; ir = ir->next) {
        if (ir->opc == OPC_LABEL) {
            const char *name = ir->dst->name;
            if (mayRemap(name)) {
                sym = FindSymbol(&localLabelMap, ir->dst->name);
                if (!sym) {
                    sym = AddSymbol(&localLabelMap, ir->dst->name, SYM_UNKNOWN, (void *)0, NULL);
                }
                sym->val = (void *)(labelValue++);
            }
        }
    }
}

/* assemble an IR list */
static char *
doIRAssemble(IRList *list, Module *P, int flags)
{
    IR *ir;
    struct flexbuf fb;
    char *ret;
    
    inDat = 0;
    inCon = 0;
    didOrg = 0;
    didPub = 0;
    if (P) {
        lmmMode = 0;
        RenameLabels(list);
    }
    
    if (gl_p2 && gl_output != OUTPUT_COGSPIN) {
        didPub = 1; // we do not want pub declaration in P2 code
    }
    flexbuf_init(&fb, 512);
    for (ir = list->head; ir; ir = ir->next) {
        if (flags && (0 != (ir->flags & FLAG_KEEP_INSTR))) {
            flexbuf_printf(&fb, "*");
        }
        DoAssembleIR(&fb, ir, P);
        if (gl_output == OUTPUT_COGSPIN) {
            if (pending_fixup) {
                flexbuf_printf(&fb, "__fixup_%d\n", pending_fixup);
                pending_fixup = 0;
            }
        }
    }
    if (gl_output == OUTPUT_COGSPIN) {
        flexbuf_printf(&fb, "__fixup_ptr\n\tlong\t");
        if (fixup_number > 0) {
            flexbuf_printf(&fb, "@__fixup_%d - 4\n", fixup_number);
        } else {
            flexbuf_printf(&fb, "0\n");
        }
    }
    flexbuf_addchar(&fb, 0);
    ret = flexbuf_get(&fb);
    flexbuf_delete(&fb);
    return ret;
}

char *
IRAssemble(IRList *list, Module *P)
{
    return doIRAssemble(list, P, 0);
}

void
DumpIRL(IRList *irl)
{
    int saveLmmMode = lmmMode;
    lmmMode = 1;
    puts(doIRAssemble(irl, NULL, 1));
    lmmMode = saveLmmMode;
}
