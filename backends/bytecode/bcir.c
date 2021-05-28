
#include "bcir.h"
#include <stdlib.h>

#pragma GCC diagnostic ignored "-Wmisleading-indentation" // GCC is very smart

const char *byteOpKindNames[] = {
    #define X(en) #en,
    BYTE_OP_KINDS_XMACRO
    #undef X
};
const char *mathOpKindNames[] = {
    #define X(en) #en,
    MATH_OP_KINDS_XMACRO
    #undef X
};

static const char *(*CompileIROP_Func)(uint8_t *,int,ByteOpIR *);
static void (*GetSizeBound_Func)(ByteOpIR *,int *,int *,int);

static int pbase_offset; // distance of function from PBASE (obj header)
static BCIRBuffer *current_birb;


ByteOpIR *BIRB_PushCopy(BCIRBuffer *buf,ByteOpIR *ir) {
    ByteOpIR *newIR = malloc(sizeof(ByteOpIR));
    memcpy(newIR,ir,sizeof(ByteOpIR));
    BIRB_Push(buf,newIR);
    return newIR;
}

void BIRB_Push(BCIRBuffer *buf,ByteOpIR *ir) {
    ir->next = NULL;

    if(!buf->head) buf->head = ir;
    if(!buf->tail) buf->tail = ir;
    else {
        buf->tail->next = ir;
        ir->prev = buf->tail;
        buf->tail = ir;
    }
    buf->opCount++;
}

void BIRB_AppendPending(BCIRBuffer *buf) {
    printf("in BIRB_AppendPending with %d pending\n",buf->pending->opCount);
    if (!buf->pending || buf->pending->opCount == 0) return;
    buf->tail->next = buf->pending->head;
    buf->pending->head->prev = buf->tail;
    buf->tail = buf->pending->tail;

    buf->pending->head = NULL;
    buf->pending->tail = NULL;
    buf->pending->opCount = 0;
}

static bool isPowerOf2(uint32_t x)
{
    return (x & (x-1)) == 0;
}

static bool BCIR_SizeDetermined(ByteOpIR *ir) {
    return ir->fixedSize || ir->kind == BOK_LABEL;
}

static void GetJumpOffsetBounds(ByteOpIR *jump,bool func_relative,int *minDist, int *maxDist,int recursionsLeft) {
    ByteOpIR *searchingFor = jump->data.jumpTo;
    *minDist = *maxDist = 0;
    bool found = false;
    if (recursionsLeft) --recursionsLeft;
    // Try searching forward (don't include jump itself)
    for (ByteOpIR *ir=func_relative?current_birb->head:jump->next;ir;ir=ir->next) {
        if (ir==searchingFor) {
            found = true;
            break;
        };
        int min,max;
        GetSizeBound_Func(ir,&min,&max,false);
        *minDist += min;
        *maxDist += max;
    }
    // .. or backward (include jump itself)
    if (!found && !func_relative) {
        *minDist = *maxDist = 0;
        for (ByteOpIR *ir=jump;ir;ir=ir->prev) {
            if (ir==searchingFor) {
                found = true;
                break;
            };
            int min,max;
            GetSizeBound_Func(ir,&min,&max,false);
            *minDist -= min;
            *maxDist -= max;
        }
    }
    if (!found) {
        *minDist = 0xBADBAD;
        *maxDist = -1;
    }
}

static int GetJumpOffset(ByteOpIR *jump,bool func_relative) {
    int min,max;
    GetJumpOffsetBounds(jump,func_relative,&min,&max,0);
    if (min!=max) ERROR(NULL,"Internal error: GetJumpOffset called, got indeterminate offset (%d..%d)",min,max);
    return max;
}

static int CompileJumpOffset_Spin1(uint8_t *buf,int *pos,ByteOpIR *ir,int baseSize,bool func_relative,int offset_offset) {
    int offset = GetJumpOffset(ir,func_relative) + offset_offset;
    if (func_relative && offset < 0) ERROR(NULL,"CompileJumpOffset_Spin1 with doUnsigned but negative offset");
    switch (ir->fixedSize-baseSize) {
    case 1:
        if (func_relative ? (offset > 0x7F) : (offset > 0x3F || offset < -0x40)) ERROR(NULL,"Jump offset %d too big for 1 byte",offset);
        buf[(*pos)++] = offset&0x7F;
        break;
    case 2:
        if (func_relative ? (offset > 0x7FFF) : (offset > 0x3FFF || offset < -0x4000)) ERROR(NULL,"Jump offset %d too big for 2 bytes",offset);
        buf[(*pos)++] = ((offset>>8)&0x7F)|0x80;
        buf[(*pos)++] = offset&0xFF;
        break;
    default:
        ERROR(NULL,"Internal error: Trying to emit jump offset of size %d",ir->fixedSize);
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
};

static enum Spin1ConstEncoding GetSpin1ConstEncoding(int32_t imm) {
    uint32_t immu = imm;
    if(abs(imm)<=1) return S1ConEn_TINY;
    else if(isPowerOf2(immu+1)) return S1ConEn_BMASKLOW;
    else if(isPowerOf2((~immu)+1)) return S1ConEn_BMASKHIGH;
    else if(isPowerOf2(immu)) return S1ConEn_DECOD;
    else if(isPowerOf2(~immu)) return S1ConEn_DECODNOT;
    else if (immu<=0x100) return S1ConEn_1B;
    else if (immu<=0x10000) return S1ConEn_2B;
    else if (immu<=0x1000000) return S1ConEn_3B;
    else return S1ConEn_4B;
}

static bool IsShortFormMemOp(ByteOpIR *ir) {
    if (!(ir->kind == BOK_MEM_READ || ir->kind == BOK_MEM_WRITE || ir->kind == BOK_MEM_MODIFY || ir->kind == BOK_MEM_ADDRESS)) return false;
    if (!(ir->attr.memop.base == MEMOP_BASE_VBASE || ir->attr.memop.base == MEMOP_BASE_DBASE)) return false;
    return ir->data.int32 < 8*4 && !(ir->data.int32 & 3)  && ir->attr.memop.memSize == MEMOP_SIZE_LONG && !ir->attr.memop.popIndex;
}

static void GetSizeBound_Spin1(ByteOpIR *ir, int *min, int *max, int recursionsLeft) {
    if (ir->fixedSize) {
        *min = *max = ir->fixedSize;
        return;
    }
    switch (ir->kind) {
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
        }
    break;
    // Jump ops
    case BOK_JUMP:
    case BOK_JUMP_DJNZ:
    case BOK_JUMP_TJZ:
    case BOK_JUMP_IF_Z:
    case BOK_JUMP_IF_NZ:
        if (recursionsLeft) {
            int minDist,maxDist;
            GetJumpOffsetBounds(ir,false,&minDist,&maxDist,recursionsLeft);
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
        if (recursionsLeft) {
            int minDist,maxDist;
            GetJumpOffsetBounds(ir,true,&minDist,&maxDist,recursionsLeft);
            minDist += pbase_offset; maxDist += pbase_offset;
            if (maxDist < 0 || minDist < 0) {
                ERROR(NULL,"FUNDATA_PUSHADDRESS can only index forwards");
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
            *min = 2;
            *max = 3;
        }
        break;
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
            if (ir->mathKind == MOK_MOD_REPEATN) ERROR(NULL,"MOK_MOD_REPEATN is NYI");
        }
        break;
    case BOK_FUNDATA_STRING: 
        *min = *max = ir->attr.stringLength;
        break;
    // Virtual ops
    case BOK_LABEL: *min = *max = 0; break;
    // Single byte ops
    case BOK_MATHOP:
    case BOK_RETURN_PLAIN:
    case BOK_RETURN_POP:
    case BOK_ABORT_PLAIN:
    case BOK_ABORT_POP:
    case BOK_WAIT:
    case BOK_BUILTIN_STRSIZE:
    case BOK_BUILTIN_STRCOMP:
    case BOK_BUILTIN_BULKMEM:
    case BOK_COGINIT:
    case BOK_COGINIT_PREPARE:
    case BOK_COGSTOP:
    case BOK_CLKSET:
    case BOK_ANCHOR:
    case BOK_POP:
        *min = *max = 1; break;
    // Two byte ops
    case BOK_REG_READ:
    case BOK_REG_WRITE:
    case BOK_CALL_SELF:
        *min = *max = 2; break;
    // Three byte ops
    case BOK_CALL_OTHER:
    case BOK_CALL_OTHER_IDX:
        *min = *max = 3; break;
    default: 
        ERROR(NULL,"Unhandled ByteOpIR kind %d = %s in GetSizeBound_Spin1",ir->kind,byteOpKindNames[ir->kind]);
        return;
    }

}

const char *CompileIROP_Spin1(uint8_t *buf,int size,ByteOpIR *ir) {
    int pos = 0;
    const char *comment = NULL;
    switch (ir->kind) {
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
        }
        comment = auto_printf(32,"CONSTANT %d",imm);
    } break;
    case BOK_MATHOP: {
        int mathID = MathOp_to_ID_Spin1(ir->mathKind);
        buf[pos++] = 0xE0+mathID;
        comment = auto_printf(28,"MATHOP: %s",mathOpKindNames[ir->mathKind]);
    } break;
    case BOK_REG_WRITE: {
        int reg = ir->data.int32;
        buf[pos++] = 0b00111111; // register op prefix
        buf[pos++] = 0xA0+reg; // I think the top bit has to be set?
        comment = auto_printf(20,"REG_WRITE %03X",reg+0x1E0);
    } break;
    case BOK_REG_READ: {
        int reg = ir->data.int32;
        buf[pos++] = 0b00111111; // register op prefix
        buf[pos++] = 0x80+reg; // I think the top bit has to be set?
        comment = auto_printf(20,"REG_READ %03X",reg+0x1E0);
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
        if (ir->kind == BOK_MEM_MODIFY) {
            uint8_t modifyCode = 0;
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
            buf[pos++] = modifyCode;
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
            if (ir->kind == BOK_MEM_MODIFY) {
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
        // Really just a special case of MEM_ADDRESS with PBASE
        buf[pos++] = 0x87;
        CompileJumpOffset_Spin1(buf,&pos,ir,1,true,pbase_offset);
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
        buf[pos++] = 0b00000101;
        buf[pos++] = ir->attr.call.funID;
    } break;
    case BOK_CALL_OTHER: {
        buf[pos++] = 0b00000110;
        buf[pos++] = ir->attr.call.objID;
        buf[pos++] = ir->attr.call.funID;
    } break;
    case BOK_CALL_OTHER_IDX: {
        buf[pos++] = 0b00000111;
        buf[pos++] = ir->attr.call.objID;
        buf[pos++] = ir->attr.call.funID;
    } break;
    case BOK_JUMP: {
        buf[pos++] = 0b00000100;
        goto jump_common;
    }
    case BOK_JUMP_TJZ: {
        buf[pos++] = 0b00001000;
        goto jump_common;
    }
    case BOK_JUMP_DJNZ: {
        buf[pos++] = 0b00001001;
        goto jump_common;
    }
    case BOK_JUMP_IF_Z: {
        buf[pos++] = 0b00001010;
        goto jump_common;
    }
    case BOK_JUMP_IF_NZ: {
        buf[pos++] = 0b00001011;
        goto jump_common;
    }
    jump_common: {
        int offset = CompileJumpOffset_Spin1(buf,&pos,ir,1,false,0);
        comment = auto_printf(40,"%s %d",byteOpKindNames[ir->kind],offset);
    } break;
    case BOK_RETURN_PLAIN: {
        buf[pos++] = 0b00110010;
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
        buf[pos++] = ir->attr.coginit.pushCogID ? 0b00101000 : 0b00101100;
    } break;
    case BOK_POP: buf[pos++] = 0b00010100; break;
    case BOK_COGINIT_PREPARE: buf[pos++] = 0b00010101; break;
    case BOK_BUILTIN_STRSIZE: buf[pos++] = 0b00010110; break;
    case BOK_BUILTIN_STRCOMP: buf[pos++] = 0b00010111; break;
    case BOK_CLKSET:   buf[pos++] = 0b00100000; break;
    case BOK_COGSTOP:  buf[pos++] = 0b00100001; break;
    case BOK_LABEL: break;
    default: 
        ERROR(NULL,"Unhandled ByteOpIR kind %d = %s",ir->kind,byteOpKindNames[ir->kind]);
        return auto_printf(256,"Unhandled ByteOpIR kind %d = %s",ir->kind,byteOpKindNames[ir->kind]);
    }
    if (!comment) comment = byteOpKindNames[ir->kind];
    if (pos!=size) {
        ERROR(NULL,"Internal error: compiled size (%d) doesn't match op's determined size (%d) (op is a %s)",pos,size,byteOpKindNames[ir->kind]); 
        return "!!! WRONG SIZE !!!";
    }
    return comment;
}

static bool
BCIR_DetermineSizes(BCIRBuffer *irbuf,bool force,int maxRecursion) {
    printf("In BCIR_DetermineSizes with maxRecursion = %d and force = %c\n",maxRecursion,force?'Y':'N');
    bool didSomething = false;
    for (ByteOpIR *ir=irbuf->head;ir;ir=ir->next) {
        if (BCIR_SizeDetermined(ir)) continue;
        int min=-1,max=-1;
        GetSizeBound_Func(ir,&min,&max,maxRecursion);
        if (min<0||max<0) ERROR(NULL,"Internal error: size bounds negative");
        if (min==max) {
            ir->fixedSize = max;
            didSomething = true;
        } else if (force) {
            ir->fixedSize = max;
            return true;
        }
    }
    return didSomething;
}

static bool
BCIR_AllDetermined(BCIRBuffer *irbuf) {
    for (ByteOpIR *ir=irbuf->tail;ir;ir=ir->prev) { // Iterate backwards, likely to be a bit faster
        if (BCIR_SizeDetermined(ir)) continue;
        printf("An IR of kind %s is not determined\n",byteOpKindNames[ir->kind]);
        return false;
    }
    return true;
}

static void
BCIR_Compact(BCIRBuffer *irbuf,int maxRecursion) {
    for(;;) {
        // Fix sizes until we cant anymore
        while (BCIR_DetermineSizes(irbuf,false,maxRecursion));
        printf("All possible determined!\n");
        if (BCIR_AllDetermined(irbuf)) return; // All good
        else BCIR_DetermineSizes(irbuf,true,maxRecursion); // Do an oversized encoding
    }
}

void BCIR_to_BOB(BCIRBuffer *irbuf,ByteOutputBuffer *bob,int pbase_funoffset) {
    pbase_offset = pbase_funoffset;
    current_birb = irbuf;
    if (!irbuf->opCount) return;
    printf("Before BCIR_Compact\n");
    BCIR_Compact(irbuf,2);
    printf("Compacted!\n");
    for(ByteOpIR *ir=irbuf->head;ir;ir=ir->next) {
        if (ir->fixedSize<0) {
            ERROR(NULL,"Internal Errror: IR with negative size");
            continue;
        }
        uint8_t code[ir->fixedSize];
        memset(code,0,ir->fixedSize);
        const char *comment = CompileIROP_Func(code,ir->fixedSize,ir);
        if (!comment) comment = "(MISSING COMMENT)";
        BOB_Push(bob,code,ir->fixedSize,comment);
    }
}

void BCIR_Init() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        CompileIROP_Func = &CompileIROP_Spin1;
        GetSizeBound_Func = &GetSizeBound_Spin1;
        break;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return;
    }
}
