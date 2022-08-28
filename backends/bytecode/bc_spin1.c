//
// Spin1 Bytecode definitons for spin2cpp
//
// Copyright 2021 Ada Gottensträter and Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include "outbc.h"
#include "bc_spin1.h"
#include "becommon.h"
#include <stdlib.h>

#pragma GCC diagnostic ignored "-Wmisleading-indentation" // GCC is very smart

enum Spin1OffsetEncoding {
    S1OffEn_VARLEN_SIGNED,
    S1OffEn_VARLEN_UNSIGNED,
    S1OffEn_FIXLEN,
    S1OffEn_FIXLEN_LE, // little endian as opposed to everything else
};

static int CompileJumpOffset_Spin1(uint8_t *buf,int *pos,ByteOpIR *ir,int baseSize,bool func_relative,int offset_offset,enum Spin1OffsetEncoding encoding) {
    int offset = BCIR_GetJumpOffset(ir,func_relative) + offset_offset;
    if (encoding != S1OffEn_VARLEN_SIGNED && offset < 0) ERROR(NULL,"CompileJumpOffset_Spin1 with unsigned encoding but negative offset");
    int hlimit1=0,llimit1=0,hlimit2=0,llimit2=0;
    bool isVarlen = true, isLittleEndian = false;
    switch (encoding) {
    case S1OffEn_VARLEN_SIGNED: hlimit1 = 0x3F, llimit1 = -0x40, hlimit2 = 0x3FFF, llimit2 = -0x4000; break;
    case S1OffEn_VARLEN_UNSIGNED: hlimit1 = 0x7F, llimit1 = 0, hlimit2 = 0x7FFF, llimit2 = 0; break;
    case S1OffEn_FIXLEN_LE: isLittleEndian = true; // \/ same otherwise \/
    case S1OffEn_FIXLEN: hlimit1 = 0xFF, llimit1 = 0, hlimit2 = 0xFFFF, llimit2 = 0, isVarlen = false; break;
    }
    switch (ir->fixedSize-baseSize) {
    case 1:
        if (offset > hlimit1 || offset < llimit1) ERROR(NULL,"Jump offset %d too big for 1 byte with encoding %d",offset,encoding);
        buf[(*pos)++] = isVarlen ? offset& 0x7F : offset & 0xFF;
        break;
    case 2:
        if (offset > hlimit2 || offset < llimit2) ERROR(NULL,"Jump offset %d too big for 2 bytes with encoding %d",offset,encoding);
        if (isLittleEndian) buf[(*pos)++] = offset&0xFF;
        buf[(*pos)++] = isVarlen ? ((offset>>8)&0x7F)|0x80 : (offset>>8)&0xFF;
        if (!isLittleEndian) buf[(*pos)++] = offset&0xFF;
        break;
    default:
        ERROR(NULL,"Internal error, Trying to emit jump offset of size %d",ir->fixedSize);
        break;
    }
    return offset;
}

uint8_t MathOp_to_ID_Spin1(enum MathOpKind mathKind) {
    switch(mathKind) {
        case MOK_ROR:       return 0b00000; break;
        case MOK_ROL:       return 0b00001; break;
        case MOK_SHR:       return 0b00010; break;
        case MOK_SHL:       return 0b00011; break;
        case MOK_MIN:       return 0b00100; break;
        case MOK_MAX:       return 0b00101; break;
        case MOK_NEG:       return 0b00110; break;
        case MOK_BITNOT:    return 0b00111; break;
        case MOK_BITAND:    return 0b01000; break;
        case MOK_ABS:       return 0b01001; break;
        case MOK_BITOR:     return 0b01010; break;
        case MOK_BITXOR:    return 0b01011; break;
        case MOK_ADD:       return 0b01100; break;
        case MOK_SUB:       return 0b01101; break;
        case MOK_SAR:       return 0b01110; break;
        case MOK_REV:       return 0b01111; break;
        case MOK_LOGICAND:  return 0b10000; break;
        case MOK_ENCODE:    return 0b10001; break;
        case MOK_LOGICOR:   return 0b10010; break;
        case MOK_DECODE:    return 0b10011; break;
        case MOK_MULLOW:    return 0b10100; break;
        case MOK_MULHIGH:   return 0b10101; break;
        case MOK_DIVIDE:    return 0b10110; break;
        case MOK_REMAINDER: return 0b10111; break;
        case MOK_SQRT:      return 0b11000; break;
        case MOK_CMP_B:     return 0b11001; break;
        case MOK_CMP_A:     return 0b11010; break;
        case MOK_CMP_NE:    return 0b11011; break;
        case MOK_CMP_E:     return 0b11100; break;
        case MOK_CMP_BE:    return 0b11101; break;
        case MOK_CMP_AE:    return 0b11110; break;
        case MOK_BOOLNOT:   return 0b11111; break;
        default:
            ERROR(NULL,"Unhandled math op type %d = %s",mathKind,mathOpKindNames[mathKind]);
            return 0;
        }
}

enum Spin1ConstEncoding {
    S1ConEn_TINY, // -1..1
    S1ConEn_DECOD, // powers of 2
    S1ConEn_DECODNOT, // powers of 2, inverted
    S1ConEn_BMASKLOW, // low bitmasks
    S1ConEn_BMASKHIGH, // high bitmasks
    S1ConEn_1B,
    S1ConEn_2B,
    S1ConEn_3B,
    S1ConEn_4B,
    S1ConEn_NEG1B,
    S1ConEn_NEG2B,
};

static enum Spin1ConstEncoding GetSpin1ConstEncoding(int32_t imm) {
    uint32_t immu = imm;
    if(abs(imm)<=1) return S1ConEn_TINY;
    else if (immu<0x100) return S1ConEn_1B; // A 1-byte literal is preferrable to a bit literal
    else if (isPowerOf2(immu+1)) return S1ConEn_BMASKLOW;
    else if (isPowerOf2((~immu)+1)) return S1ConEn_BMASKHIGH;
    else if (isPowerOf2(immu)) return S1ConEn_DECOD;
    else if (isPowerOf2(~immu)) return S1ConEn_DECODNOT;
    else if (immu<0x10000) return S1ConEn_2B;
    else if (immu<0x1000000) return S1ConEn_3B;
    else if (imm < 0 && imm > -0x00FF  && (curfunc->optimize_flags & OPT_EXTRASMALL)) return S1ConEn_NEG1B;
    else if (imm < 0 && imm > -0xFFFF  && (curfunc->optimize_flags & OPT_EXTRASMALL)) return S1ConEn_NEG2B;
    else return S1ConEn_4B;
}

static void Spin1RelocFuncAddr(Module *P, uint8_t *dataptr, uint32_t addr) {
    dataptr[0] = (addr>>8) & 0xff;
    dataptr[1] = (addr>>0) & 0xff;
}

static void Spin1RelocDatAddr(Module *P, uint8_t *dataptr, uint32_t addr) {
    uint32_t off = (dataptr[0]<<8) + dataptr[1];
    addr += off;
    addr += BCgetDAToffset(P, false, NULL, true);
    dataptr[0] = (addr>>8) & 0xff;
    dataptr[1] = (addr>>0) & 0xff;
}

static bool IsShortFormMemOp(ByteOpIR *ir) {
    if (!(ir->kind == BOK_MEM_READ || ir->kind == BOK_MEM_WRITE || ir->kind == BOK_MEM_MODIFY || ir->kind == BOK_MEM_ADDRESS)) return false;
    if (!(ir->attr.memop.base == MEMOP_BASE_VBASE || ir->attr.memop.base == MEMOP_BASE_DBASE)) return false;
    return ir->data.int32 < 8*4 && !(ir->data.int32 & 3)  && ir->attr.memop.memSize == MEMOP_SIZE_LONG && !ir->attr.memop.popIndex;
}

void GetSizeBound_Spin1(ByteOpIR *ir, int *min, int *max, int recursionsLeft) {
    if (ir->fixedSize >= 0) {
        *min = *max = ir->fixedSize;
        return;
    }
    switch (ir->kind) {
    case BOK_ALIGN: {
        // Align is slightly tricky, since it's size can only be determined if everything in front of it is
        // However, it is only used in data appended to the function, so it should always resolve at some point
        ir->jumpTo = ir; // Hack to get BCIR_GetJumpOffsetBounds to get offset to the align itself
        int minoffset,maxoffset;
        int alignto = ir->data.int32;
        if (recursionsLeft) BCIR_GetJumpOffsetBounds(ir,true,&minoffset,&maxoffset,recursionsLeft);
        if (!recursionsLeft || minoffset != maxoffset) {
            // Can't determine size yet
            *min = 0; *max = alignto-1;
        } else {
            int addr = minoffset+pbase_offset; // Assume PBASE is sufficiently aligned
            *min = *max = ((alignto - (addr%alignto))%alignto);
        }
        } break;
    case BOK_CONSTANT:
        switch(GetSpin1ConstEncoding(ir->data.int32)) {
        case S1ConEn_TINY: *min = *max = 1; break;
        case S1ConEn_DECOD:
        case S1ConEn_DECODNOT:
        case S1ConEn_BMASKLOW:
        case S1ConEn_BMASKHIGH:
            *min = *max = 2; break;
        case S1ConEn_1B: *min = *max = 2; break;
        case S1ConEn_2B: *min = *max = 3; break;
        case S1ConEn_3B: *min = *max = 4; break;
        case S1ConEn_4B: *min = *max = 5; break;
        case S1ConEn_NEG1B: *min = *max = 3; break;
        case S1ConEn_NEG2B: *min = *max = 4; break;
        }
        break;
    case BOK_CONSTANT_FUNCREF:
        if (0 == (ir->data.int32 & 0xff00) ) {
            *min = *max = 4;
        } else {
            *min = *max = 5;
        }
        break;
    case BOK_CONSTANT_DATREF:
        *min = *max = 3;
        break;
    // Jump ops
    case BOK_JUMP:
    case BOK_JUMP_DJNZ:
    case BOK_JUMP_TJZ:
    case BOK_JUMP_IF_Z:
    case BOK_JUMP_IF_NZ:
    case BOK_CASE:
    case BOK_CASE_RANGE:
        if (recursionsLeft) {
            int minDist,maxDist;
            BCIR_GetJumpOffsetBounds(ir,false,&minDist,&maxDist,recursionsLeft);
            if (maxDist <= 0x3F && maxDist >= -0x40) { // 1-byte either way
                *min = *max = 2;
            } else if (minDist > 0x3F || minDist < -40) { // 2-byte either way
                *min = *max = 3;
            } else { // Uncertain
                *min = 2;
                *max = 3;
            }
        } else {
            *min = 2;
            *max = 3;
        }
        break;
    // Get function data
    case BOK_FUNDATA_PUSHADDRESS:
    case BOK_FUNDATA_LOOKUPJUMP:
        if (recursionsLeft) {
            int minDist,maxDist;
            BCIR_GetJumpOffsetBounds(ir,true,&minDist,&maxDist,recursionsLeft);
            minDist += pbase_offset; maxDist += pbase_offset;
            if (ir->kind == BOK_FUNDATA_LOOKUPJUMP || ir->attr.pushaddress.addPbase) {
                if (maxDist < 0 || minDist < 0) {
                    ERROR(NULL,"Internal Error: negative distance in FUNDATA_PUSHADDRESS with PBASE");
                    *min = *max = 3;
                } else if (maxDist <= 0x7F) { // 1-byte either way
                    *min = *max = 2;
                } else if (minDist > 0x7F) { // 2-byte either way
                    *min = *max = 3;
                } else { // Uncertain
                    *min = 2;
                    *max = 3;
                }
            } else {
                if (maxDist < 0 || minDist < 0) {
                    ERROR(NULL,"Internal Error: negative distance in FUNDATA_PUSHADDRESS without PBASE");
                    *min = *max = 5;
                } else if (maxDist <= 0xFF) { // 1-byte either way
                    *min = *max = 2;
                } else if (minDist > 0xFF) { // 2-byte either way
                    *min = *max = 3;
                } else { // Uncertain
                    *min = 2;
                    *max = 3;
                }
            }
        } else {
            if (ir->kind == BOK_FUNDATA_LOOKUPJUMP || ir->attr.pushaddress.addPbase) {
                *min = 2;
                *max = 3;
            } else {
                *min = 2; // 0/1/-1 are highly unlikely
                *max = 3; // max useful pbase offset is 0x7FEF, which fits in 3 bytes
            }
        }
        break;
    case BOK_FUNDATA_JUMPENTRY: *min = *max = 2; break; // Always word-sized
    // Memory ops
    case BOK_MEM_READ:
    case BOK_MEM_WRITE:
    case BOK_MEM_ADDRESS:
    case BOK_MEM_MODIFY:
             if (ir->attr.memop.base == MEMOP_BASE_POP) *min = *max = 1; // Pop base is always 1 byte
        else if (IsShortFormMemOp(ir))   *min = *max = 1; // Short access for first 8 VAR/locals
        else if (ir->data.int32 < 0x80)  *min = *max = 2; // with 1 base byte
        else                             *min = *max = 3; // with 2 base bytes

        if (ir->kind == BOK_MEM_MODIFY) {
            *min += 1; *max += 1; // Extra byte
            if (ir->mathKind == MOK_MOD_REPEATSTEP) {  // Insanity
                if (recursionsLeft) {
                    int minDist,maxDist;
                    BCIR_GetJumpOffsetBounds(ir,false,&minDist,&maxDist,recursionsLeft);
                    if (maxDist <= 0x3F && maxDist >= -0x40) { // 1-byte either way
                        *min += 1;
                        *max += 1;
                    } else if (minDist > 0x3F || minDist < -40) { // 2-byte either way
                        *min += 2;
                        *max += 2;
                    } else { // Uncertain
                        *min += 1;
                        *max += 2;
                    }
                } else {
                    *min += 1;
                    *max += 2;
                }
            }
        }
        break;
    case BOK_FUNDATA_STRING: 
        *min = *max = ir->attr.stringLength;
        break;
    // Virtual ops
    case BOK_LABEL: *min = *max = 0; break;
    // returns: the higher level is responsible for adding any pushes/pops needed before the returns
    case BOK_RETURN_PLAIN:
    case BOK_RETURN_POP:
        *min = *max = 1;
        break;
    // Single byte ops
    case BOK_MATHOP:
    case BOK_ABORT_PLAIN:
    case BOK_ABORT_POP:
    case BOK_WAIT:
    case BOK_CASE_DONE:
    case BOK_LOOKDOWN:
    case BOK_LOOKUP:
    case BOK_LOOKDOWN_RANGE:
    case BOK_LOOKUP_RANGE:
    case BOK_LOOKEND:
    case BOK_BUILTIN_STRSIZE:
    case BOK_BUILTIN_STRCOMP:
    case BOK_BUILTIN_BULKMEM:
    case BOK_COGINIT:
    case BOK_COGINIT_PREPARE:
    case BOK_COGSTOP:
    case BOK_LOCKNEW:
    case BOK_LOCKRET:
    case BOK_LOCKSET:
    case BOK_LOCKCLR:
    case BOK_CLKSET:
    case BOK_ANCHOR:
    case BOK_POP:
        *min = *max = 1; break;
    // Two byte ops
    case BOK_REG_READ:
    case BOK_REG_WRITE:
    case BOK_REGBIT_READ:
    case BOK_REGBIT_WRITE:
    case BOK_REGBITRANGE_READ:
    case BOK_REGBITRANGE_WRITE:
        *min = *max = 2; break;
    // Three byte ops
    case BOK_REG_MODIFY:
    case BOK_REGBIT_MODIFY:
    case BOK_REGBITRANGE_MODIFY:
        *min = *max = 3; break;
    case BOK_CALL_SELF:
    case BOK_CALL_OTHER:
    case BOK_CALL_OTHER_IDX: {
        int sz = (ir->kind == BOK_CALL_SELF) ? 2 : 3;
        *min = *max = sz;
    } break;
    default: 
        ERROR(NULL,"Unhandled ByteOpIR kind %d = %s in GetSizeBound_Spin1",ir->kind,byteOpKindNames[ir->kind]);
        return;
    }

}

static uint8_t GetModifyByte_Spin1(ByteOpIR *ir, bool *sizedOpReturn) {
    uint8_t modifyCode = 0;
    bool sizedModifyOp = false;
    if (isModOperator(ir->mathKind)) {
        int modsize = 0;
        switch(ir->attr.memop.modSize) {
        case MEMOP_SIZE_BIT:   modsize = 0; break;
        case MEMOP_SIZE_BYTE:  modsize = 2; break;
        case MEMOP_SIZE_WORD:  modsize = 4; break;
        case MEMOP_SIZE_LONG:  modsize = 6; break;
        }
        switch(ir->mathKind) {
        case MOK_MOD_WRITE:        modifyCode = 0b0000000; break;
        case MOK_MOD_RANDFORWARD:  modifyCode = 0b0001000; break;
        case MOK_MOD_RANDBACKWARD: modifyCode = 0b0001100; break;
        case MOK_MOD_SIGNX_BYTE:   modifyCode = 0b0010000; break;
        case MOK_MOD_SIGNX_WORD:   modifyCode = 0b0010100; break;
        case MOK_MOD_POSTCLEAR:    modifyCode = 0b0011000; break;
        case MOK_MOD_POSTSET:      modifyCode = 0b0011100; break;
        case MOK_MOD_PREINC:       modifyCode = 0b0100000+modsize; sizedModifyOp = true; break;
        case MOK_MOD_POSTINC:      modifyCode = 0b0101000+modsize; sizedModifyOp = true; break;
        case MOK_MOD_PREDEC:       modifyCode = 0b0110000+modsize; sizedModifyOp = true; break;
        case MOK_MOD_POSTDEC:      modifyCode = 0b0111000+modsize; sizedModifyOp = true; break;
        default: 
            ERROR(NULL,"unhandled modify operator %s",mathOpKindNames[ir->mathKind]);
            break;
        }
    } else {
        modifyCode = 0b01000000 + MathOp_to_ID_Spin1(ir->mathKind) + (ir->attr.memop.modifyReverseMath << 5);
    }
    modifyCode += ir->attr.memop.pushModifyResult << 7;
    if (sizedOpReturn) *sizedOpReturn = sizedModifyOp;
    return modifyCode;
}

static const char *Spin1RegNames[32] = {
    // Interpreter oopsies
    "???",
    "???",
    "???",
    "???",
    "???",
    "???",
    "???",
    "???",
    // Interpreter registers
    "LSB",
    "ID",
    "DCALL",
    "PBASE",
    "VBASE",
    "DBASE",
    "PCURR", 
    "DCURR",
    // Hardware registers
    "PAR","CNT",
    "INA","INB",
    "OUTA","OUTB",
    "DIRA","DIRB",
    "CTRA","CTRB",
    "FRQA","FRQB",
    "PHSA","PHSB",
    "VCFG","VSCL",
};

const char *CompileIROP_Spin1(uint8_t *buf,int size,ByteOpIR *ir) {
    int pos = 0;
    const char *comment = NULL;
    switch (ir->kind) {
    case BOK_CONSTANT_FUNCREF: {
        int32_t id = ir->data.int32;
        Module *P = ir->attr.funcval.modref;
        BCRelocList *reloc = calloc(1, sizeof(*reloc));
        reloc->next = ModData(P)->relocList;
        ModData(P)->relocList = reloc;
        reloc->func = Spin1RelocFuncAddr;
        if (0 != (id & 0xff00)) {
            buf[pos++] = 0b00111011; // 4 byte immediate
            buf[pos++] = (id>>8) & 255;
            buf[pos++] = (id>>0) & 255;
        } else {
            buf[pos++] = 0b00111010; // 3 byte immediate
            buf[pos++] = (id>>0) & 255;
        }
        // add a relocation for this
        reloc->pos = &buf[pos];
        reloc->M = P;
        
        buf[pos++] = 0;          // module address
        buf[pos++] = 0;          // module address
    } break;
    case BOK_CONSTANT_DATREF: {
        int32_t off = ir->data.int32;
        Module *P = ir->attr.datval.modref;
        BCRelocList *reloc = calloc(1, sizeof(*reloc));
        reloc->next = ModData(P)->relocList;

        ModData(P)->relocList = reloc;
        reloc->func = Spin1RelocDatAddr;
        buf[pos++] = 0b00111001; // 2 byte immediate
        // add a relocation for this
        reloc->pos = &buf[pos];
        reloc->M = P;
        buf[pos++] = (off>>8) & 255;
        buf[pos++] = (off>>0) & 255;
    } break;
    case BOK_CONSTANT: {
        int32_t imm = ir->data.int32;
        uint32_t immu = imm;
        switch(GetSpin1ConstEncoding(imm)) {
        case S1ConEn_TINY: // constant -1..1
            buf[pos++] = 0b00110101 + imm;
            break;
        case S1ConEn_DECOD: // power of 2
            buf[pos++] = 0b00110111;
            buf[pos++] = 0b00000000 + ((30-__builtin_clz(immu))&31);
            break;
        case S1ConEn_DECODNOT: // inverted power of 2
            buf[pos++] = 0b00110111;
            buf[pos++] = 0b01000000 + ((30-__builtin_clz(~immu))&31);
            break;
        case S1ConEn_BMASKLOW:
            buf[pos++] = 0b00110111;
            buf[pos++] = 0b00100000 + ((30-__builtin_clz(immu+1))&31);
            break;
        case S1ConEn_BMASKHIGH:
            buf[pos++] = 0b00110111;
            buf[pos++] = 0b01100000 + ((30-__builtin_clz((~immu)+1))&31);
            break;
        case S1ConEn_1B: // 1 byte
            buf[pos++] = 0b00111000;
            buf[pos++] = (immu>>0)&255;
            break;
        case S1ConEn_2B: // 2 byte
            buf[pos++] = 0b00111001;
            buf[pos++] = (immu>>8)&255;
            buf[pos++] = (immu>>0)&255;
            break;
        case S1ConEn_3B: // 3 byte
            buf[pos++] = 0b00111010;
            buf[pos++] = (immu>>16)&255;
            buf[pos++] = (immu>>8)&255;
            buf[pos++] = (immu>>0)&255;
            break;
        case S1ConEn_4B: // 4 byte
            buf[pos++] = 0b00111011;
            buf[pos++] = (immu>>24)&255;
            buf[pos++] = (immu>>16)&255;
            buf[pos++] = (immu>>8)&255;
            buf[pos++] = (immu>>0)&255;
            break;
        case S1ConEn_NEG1B: // 1 byte + NEG
            buf[pos++] = 0b00111000;
            buf[pos++] = ((-imm)>>0)&255;
            buf[pos++] = 0xE0+0b00110; // NEG
            break;
        case S1ConEn_NEG2B: // 2 byte + NEG
            buf[pos++] = 0b00111001;
            buf[pos++] = ((-imm)>>8)&255;
            buf[pos++] = ((-imm)>>0)&255;
            buf[pos++] = 0xE0+0b00110; // NEG
            break;
        }
        comment = auto_printf(32,"CONSTANT %d",imm);
    } break;
    case BOK_MATHOP: {
        int mathID = MathOp_to_ID_Spin1(ir->mathKind);
        buf[pos++] = 0xE0+mathID;
        comment = auto_printf(28,"MATHOP: %s",mathOpKindNames[ir->mathKind]);
    } break;
    case BOK_REGBIT_READ:
    case BOK_REGBIT_WRITE:
    case BOK_REGBIT_MODIFY:
    case BOK_REGBITRANGE_READ:
    case BOK_REGBITRANGE_WRITE:
    case BOK_REGBITRANGE_MODIFY:
    case BOK_REG_READ:
    case BOK_REG_WRITE:
    case BOK_REG_MODIFY: {
        int reg = ir->data.int32;
        // GCC is very smart
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wswitch"
        #pragma GCC diagnostic ignored "-Wswitch-enum"

        switch (ir->kind) {
        case BOK_REGBIT_READ:
        case BOK_REGBIT_WRITE:
        case BOK_REGBIT_MODIFY:
            buf[pos++] = 0b00111101; // register bit op prefix
            break;
        case BOK_REGBITRANGE_READ: 
        case BOK_REGBITRANGE_WRITE:
        case BOK_REGBITRANGE_MODIFY: 
            buf[pos++] = 0b00111110; // register range op prefix
            break;
        case BOK_REG_READ:
        case BOK_REG_WRITE:
        case BOK_REG_MODIFY:
            buf[pos++] = 0b00111111; // register op prefix
            break;
        }

        uint8_t regop = (reg&0x1F)+0x80; // top bit has to be set for internal purposes
        bool isModify = false;
        switch (ir->kind) {
        case BOK_REGBITRANGE_READ: 
        case BOK_REGBIT_READ:
        case BOK_REG_READ:
            regop |= 0x00;
            break;
        case BOK_REGBITRANGE_WRITE:
        case BOK_REGBIT_WRITE:
        case BOK_REG_WRITE:
            regop |= 0x20;
            break;
        case BOK_REGBITRANGE_MODIFY: 
        case BOK_REGBIT_MODIFY:
        case BOK_REG_MODIFY:
            regop |= 0x40;
            isModify = true;
            break;
        }
        buf[pos++] = regop;
        if (isModify) {
            buf[pos++] = GetModifyByte_Spin1(ir,NULL);
            comment = auto_printf(128,"%s %03X(%s) %s %s%s",byteOpKindNames[ir->kind],reg+0x1E0,Spin1RegNames[reg&31],
                mathOpKindNames[ir->mathKind],ir->attr.memop.modifyReverseMath?"(REVERSE)":"",ir->attr.memop.pushModifyResult?"(PUSH RESULT)":"");
        } else {
            comment = auto_printf(40,"%s %03X(%s)",byteOpKindNames[ir->kind],reg+0x1E0,Spin1RegNames[reg&31]);
        }
        #pragma GCC diagnostic pop
    } break;
    case BOK_MEM_READ:
    case BOK_MEM_WRITE:
    case BOK_MEM_ADDRESS:
    case BOK_MEM_MODIFY: {
        uint32_t offset = ir->data.int32;

        bool shortForm = IsShortFormMemOp(ir);
        if (shortForm) {
            // Compile short form
            uint8_t opcode = 0x40 + (offset&0x1C);
                 if (ir->kind == BOK_MEM_READ) opcode += 0;
            else if (ir->kind == BOK_MEM_WRITE) opcode += 1;
            else if (ir->kind == BOK_MEM_MODIFY) opcode += 2;
            else if (ir->kind == BOK_MEM_ADDRESS) opcode += 3;
                 if (ir->attr.memop.base == MEMOP_BASE_VBASE) opcode += 0<<5;
            else if (ir->attr.memop.base == MEMOP_BASE_DBASE) opcode += 1<<5;

            buf[pos++] = opcode;
        } else {
            uint8_t opcode = 0x80 + ((ir->attr.memop.popIndex)<<4);
                 if (ir->attr.memop.memSize == MEMOP_SIZE_BYTE) opcode += 0<<5;
            else if (ir->attr.memop.memSize == MEMOP_SIZE_WORD) opcode += 1<<5;
            else if (ir->attr.memop.memSize == MEMOP_SIZE_LONG) opcode += 2<<5;
            else ERROR(NULL,"Internal Error: Invalid memSize");
                 if (ir->attr.memop.base == MEMOP_BASE_POP)   opcode += 0<<2;
            else if (ir->attr.memop.base == MEMOP_BASE_PBASE) opcode += 1<<2;
            else if (ir->attr.memop.base == MEMOP_BASE_VBASE) opcode += 2<<2;
            else if (ir->attr.memop.base == MEMOP_BASE_DBASE) opcode += 3<<2;
            else ERROR(NULL,"Internal Error: Invalid base");
                 if (ir->kind == BOK_MEM_READ) opcode += 0;
            else if (ir->kind == BOK_MEM_WRITE) opcode += 1;
            else if (ir->kind == BOK_MEM_MODIFY) opcode += 2;
            else if (ir->kind == BOK_MEM_ADDRESS) opcode += 3;
            else ERROR(NULL,"Internal Error: Invalid kind");

            buf[pos++] = opcode;

            if (ir->attr.memop.base != MEMOP_BASE_POP) {
                if (offset < 0x80) {
                    buf[pos++] = offset;
                } else if (offset < 0X8000) {
                    buf[pos++] = ((offset>>8)&0x7F)|0x80;
                    buf[pos++] = offset&0xFF;
                } else ERROR(NULL,"Mem op offset exceeds 0x8000");
            }
        }
        
        bool sizedModifyOp = false;
        int jumpOffset = 0;
        if (ir->kind == BOK_MEM_MODIFY) {
            if (ir->mathKind == MOK_MOD_REPEATSTEP) {
                if (ir->attr.memop.pushModifyResult) ERROR(NULL,"Internal Error: pushModifyResult set on MOK_MOD_REPEATSTEP");
                buf[pos++] = ir->attr.memop.repeatPopStep ? 0b00000110 : 0b00000010;
                jumpOffset = CompileJumpOffset_Spin1(buf,&pos,ir,pos,false,0,S1OffEn_VARLEN_SIGNED);
            } else { 
                buf[pos++] = GetModifyByte_Spin1(ir,&sizedModifyOp);
            }
        }

        if (gl_listing) { // This is complex, so only run when we actually want a listing
            const char* basetext = "oh no";
            switch(ir->attr.memop.base) {
            case MEMOP_BASE_POP: basetext = "(POP base)"; break;
            case MEMOP_BASE_PBASE: basetext = auto_printf(14,"PBASE+$%04X",offset); break;
            case MEMOP_BASE_VBASE: basetext = auto_printf(14,"VBASE+$%04X",offset); break;
            case MEMOP_BASE_DBASE: basetext = auto_printf(14,"DBASE+$%04X",offset); break;
            }
            const char* sizetext = "oh no"; 
            switch (ir->attr.memop.memSize) {
            case MEMOP_SIZE_BIT:  sizetext = "(invalid)"; break;
            case MEMOP_SIZE_BYTE: sizetext = "BYTE"; break;
            case MEMOP_SIZE_WORD: sizetext = "WORD"; break;
            case MEMOP_SIZE_LONG: sizetext = "LONG"; break;
            }
            const char* modsizetext = "oh no"; 
            switch (ir->attr.memop.modSize) {
            case MEMOP_SIZE_BIT:  modsizetext = "(BIT)"; break;
            case MEMOP_SIZE_BYTE: modsizetext = "(BYTE)"; break;
            case MEMOP_SIZE_WORD: modsizetext = "(WORD)"; break;
            case MEMOP_SIZE_LONG: modsizetext = "(LONG)"; break;
            }
            if (ir->kind == BOK_MEM_MODIFY && ir->mathKind == MOK_MOD_REPEATSTEP) {
                comment = auto_printf(256,"%s %s %s%s %s %s %s%+d",
                    byteOpKindNames[ir->kind],sizetext,basetext,ir->attr.memop.popIndex ? "+(POP index)":"",shortForm?"(short)":"",
                    mathOpKindNames[ir->mathKind],ir->attr.memop.repeatPopStep?"WITH_STEP ":"",jumpOffset);
            } else if (ir->kind == BOK_MEM_MODIFY) {
                comment = auto_printf(256,"%s %s %s%s %s %s %s%s%s",
                    byteOpKindNames[ir->kind],sizetext,basetext,ir->attr.memop.popIndex ? "+(POP index)":"",shortForm?"(short)":"",
                    mathOpKindNames[ir->mathKind],sizedModifyOp?modsizetext:"",ir->attr.memop.modifyReverseMath?"(REVERSE)":"",ir->attr.memop.pushModifyResult?"(PUSH RESULT)":"");
            } else {
                comment = auto_printf(128,"%s %s %s%s %s",
                    byteOpKindNames[ir->kind],sizetext,basetext,ir->attr.memop.popIndex ? "+(POP index)":"",shortForm?"(short)":"");
            }
        }
    } break;
    case BOK_FUNDATA_PUSHADDRESS: {
        // Really just a special case of either MEM_ADDRESS with PBASE or a constant
        bool addPbase = ir->attr.pushaddress.addPbase;
        buf[pos++] = addPbase ? 0x87 : (size==2 ? 0x38 : 0x39);
        int offset = CompileJumpOffset_Spin1(buf,&pos,ir,1,true,pbase_offset, addPbase ? S1OffEn_VARLEN_UNSIGNED : S1OffEn_FIXLEN);
        comment = auto_printf(64,"FUNDATA_PUSHADDRESS %+d%s",offset,addPbase?" +PBASE":"");
    } break;
    case BOK_FUNDATA_LOOKUPJUMP: {
        buf[pos++] = 0xB4; // MEM_READ WORD with INDEX and PBASE
        int offset = CompileJumpOffset_Spin1(buf,&pos,ir,1,true,pbase_offset,S1OffEn_VARLEN_UNSIGNED);
        comment = auto_printf(64,"BOK_FUNDATA_LOOKUPJUMP %04X (+PBASE)",offset);
    } break;
    case BOK_FUNDATA_JUMPENTRY: {
        int offset = CompileJumpOffset_Spin1(buf,&pos,ir,0,true,pbase_offset, S1OffEn_FIXLEN_LE);
        comment = auto_printf(64,"FUNDATA_JUMPENTRY %04X (+PBASE)",offset);
    } break;
    case BOK_FUNDATA_STRING : {
        memcpy((char*)buf,ir->data.stringPtr,size);
        pos+=size;
    } break;
    case BOK_BUILTIN_BULKMEM: {
        uint8_t opcode = 0b00011000;
        const char *sizetext = "oh no";
        switch (ir->attr.bulkmem.memSize) {
        case BULKMEM_SIZE_BYTE: opcode += 0; sizetext = "BYTE"; break;
        case BULKMEM_SIZE_WORD: opcode += 1; sizetext = "WORD"; break;
        case BULKMEM_SIZE_LONG: opcode += 2; sizetext = "LONG"; break;
        }
        if (ir->attr.bulkmem.isMove) opcode += 4;
        buf[pos++] = opcode;
        comment = auto_printf(10,"%s%s",sizetext,ir->attr.bulkmem.isMove ?"MOVE":"FILL");
    } break;
    case BOK_ANCHOR: {
        buf[pos++] = 0b00000000 + ((!ir->attr.anchor.withResult)&1) + (ir->attr.anchor.rescue<<1);
        comment = auto_printf(32,"ANCHOR %s %s",ir->attr.anchor.rescue?"(RESCUE)":"",ir->attr.anchor.withResult?"":"(DISCARD)");
    } break;
    case BOK_CALL_SELF: {
        int funID = ir->attr.call.funID;
        buf[pos++] = 0b00000101;
        buf[pos++] = funID;
        comment = auto_printf(128,"CALL_SELF %d (%s)",funID,BCgetFuncNameForId(current,funID));
    } break;
    case BOK_CALL_OTHER: {
        int funID = ir->attr.call.funID, objID = ir->attr.call.objID;
        buf[pos++] = 0b00000110;
        buf[pos++] = objID;
        buf[pos++] = funID;
        Module *object = BCgetModuleForOBJID(current,objID);
        comment = auto_printf(128,"CALL_OTHER %d.%d (%s.%s)",objID,funID,BCgetNameForOBJID(current,objID),BCgetFuncNameForId(object,funID));
    } break;
    case BOK_CALL_OTHER_IDX: {
        int funID = ir->attr.call.funID, objID = ir->attr.call.objID;
        buf[pos++] = 0b00000111;
        buf[pos++] = objID;
        buf[pos++] = funID;
        Module *object = BCgetModuleForOBJID(current,objID);
        comment = auto_printf(128,"CALL_OTHER_IDX %d[].%d (%s[].%s)",objID,funID,BCgetNameForOBJID(current,objID),BCgetFuncNameForId(object,funID));
    } break;
    case BOK_JUMP:       buf[pos++] = 0b00000100; goto jump_common;
    case BOK_JUMP_TJZ:   buf[pos++] = 0b00001000; goto jump_common;
    case BOK_JUMP_DJNZ:  buf[pos++] = 0b00001001; goto jump_common;
    case BOK_JUMP_IF_Z:  buf[pos++] = 0b00001010; goto jump_common;
    case BOK_JUMP_IF_NZ: buf[pos++] = 0b00001011; goto jump_common;
    case BOK_CASE:       buf[pos++] = 0b00001101; goto jump_common;
    case BOK_CASE_RANGE: buf[pos++] = 0b00001110; goto jump_common;
    jump_common: {
        int offset = CompileJumpOffset_Spin1(buf,&pos,ir,1,false,0,S1OffEn_VARLEN_SIGNED);
        comment = auto_printf(40,"%s %+d",byteOpKindNames[ir->kind],offset);
    } break;
    case BOK_WAIT: {
        switch(ir->attr.wait.type) {
        case BCW_WAITPEQ: buf[pos++] = 0b00011011; comment = "WAITPEQ"; break;
        case BCW_WAITPNE: buf[pos++] = 0b00011111; comment = "WAITPNE"; break;
        case BCW_WAITCNT: buf[pos++] = 0b00100011; comment = "WAITCNT"; break;
        case BCW_WAITVID: buf[pos++] = 0b00100111; comment = "WAITVID"; break;
        default: ERROR(NULL,"Unhandled wait type %d",ir->attr.wait.type); break;
        }
    } break;
    case BOK_COGINIT: {
        buf[pos++] = ir->attr.coginit.pushResult ? 0b00101000 : 0b00101100;
    } break;
    case BOK_LOCKNEW: {
        buf[pos++] = ir->attr.coginit.pushResult ? 0b00101001 : 0b00101101;
    } break;
    case BOK_LOCKSET: {
        buf[pos++] = ir->attr.coginit.pushResult ? 0b00101010 : 0b00101110;
    } break;
    case BOK_LOCKCLR: {
        buf[pos++] = ir->attr.coginit.pushResult ? 0b00101011 : 0b00101111;
    } break;
    case BOK_RETURN_PLAIN:
    case BOK_RETURN_POP: {
        buf[pos++] = (ir->kind == BOK_RETURN_PLAIN) ? 0b00110010 : 0b00110011;
    } break;
    case BOK_CASE_DONE:      buf[pos++] = 0b00001100; break;
    case BOK_LOOKEND:        buf[pos++] = 0b00001111; break;
    case BOK_LOOKUP:         buf[pos++] = 0b00010000; break;
    case BOK_LOOKDOWN:       buf[pos++] = 0b00010001; break;
    case BOK_LOOKUP_RANGE:   buf[pos++] = 0b00010010; break;
    case BOK_LOOKDOWN_RANGE: buf[pos++] = 0b00010011; break;
    case BOK_POP: buf[pos++] = 0b00010100; break;
    case BOK_COGINIT_PREPARE: buf[pos++] = 0b00010101; break;
    case BOK_BUILTIN_STRSIZE: buf[pos++] = 0b00010110; break;
    case BOK_BUILTIN_STRCOMP: buf[pos++] = 0b00010111; break;
    case BOK_CLKSET:   buf[pos++] = 0b00100000; break;
    case BOK_COGSTOP:  buf[pos++] = 0b00100001; break;
    case BOK_LOCKRET:  buf[pos++] = 0b00100010; break;
    case BOK_ABORT_PLAIN:  buf[pos++] = 0b00110000; break;
    case BOK_ABORT_POP:    buf[pos++] = 0b00110001; break;
    case BOK_LABEL: break;
    case BOK_ALIGN: while(pos<size) buf[pos++] = 0; comment = auto_printf(14,"ALIGN %d",ir->data.int32); break;
    default: 
        ERROR(NULL,"Unhandled ByteOpIR kind %d = %s",ir->kind,byteOpKindNames[ir->kind]);
        return auto_printf(256,"Unhandled ByteOpIR kind %d = %s",ir->kind,byteOpKindNames[ir->kind]);
    }
    if (!comment) comment = byteOpKindNames[ir->kind];
    if (pos!=size) {
        ERROR(NULL,"Internal error, compiled size (%d) doesn't match op's determined size (%d) (op is a %s)",pos,size,byteOpKindNames[ir->kind]); 
        return "!!! WRONG SIZE !!!";
    }
    return comment;
}

