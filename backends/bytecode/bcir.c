
#include "bcir.h"
#include <stdlib.h>

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

static bool isPowerOf2(uint32_t x)
{
    return (x & (x-1)) == 0;
}

static bool BCIR_SizeDetermined(ByteOpIR *ir) {
    return ir->fixedSize || ir->kind == BOK_LABEL;
}

static void GetJumpOffsetBounds(ByteOpIR *jump,int *minDist, int *maxDist,int recursionsLeft) {
    ByteOpIR *searchingFor = jump->data.jumpTo;
    *minDist = *maxDist = 0;
    bool found = false;
    if (recursionsLeft) --recursionsLeft;
    // Try searching forward (don't include jump itself)
    for (ByteOpIR *ir=jump->next;ir;ir=ir->next) {
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
    if (!found) {
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

static int GetJumpOffset(ByteOpIR *jump) {
    int min,max;
    GetJumpOffsetBounds(jump,&min,&max,0);
    if (min!=max) ERROR(NULL,"Internal error: GetJumpOffset called, got indeterminate offset (%d..%d)",min,max);
    return max;
}

static int CompileJumpOffset_Spin1(uint8_t *buf,int *pos,ByteOpIR *ir,int baseSize) {
    int offset = GetJumpOffset(ir);
    switch (ir->fixedSize-baseSize) {
    case 1:
        if (offset > 0x3F || offset < -0x40) ERROR(NULL,"Jump offset %d too big for 1 byte",offset);
        buf[(*pos)++] = offset&0x7F;
        break;
    case 2:
        if (offset > 0x3FFF || offset < -0x4000) ERROR(NULL,"Jump offset %d too big for 2 bytes",offset);
        buf[(*pos)++] = ((offset>>8)&0x7F)|0x80;
        buf[(*pos)++] = offset&0xFF;
        break;
    default:
        ERROR(NULL,"Internal error: Trying to emit jump offset of size %d",ir->fixedSize);
        break;
    }
    return offset;
}

enum Spin1ConstEncoding {
    S1ConEn_TINY, // -1..1
    S1ConEn_DECOD, // powers of 2
    S1ConEn_1B,
    S1ConEn_2B,
    S1ConEn_3B,
    S1ConEn_4B,
    // TODO: There's more
};

static enum Spin1ConstEncoding GetSpin1ConstEncoding(int32_t imm) {
    uint32_t immu = imm;
    if(abs(imm)<=1) return S1ConEn_TINY;
    else if (immu<=0x100) return S1ConEn_1B;
    else if (immu<=0x10000) return S1ConEn_2B;
    else if (immu<=0x1000000) return S1ConEn_3B;
    else return S1ConEn_4B;
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
        case S1ConEn_DECOD: *min = *max = 2; break;
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
            GetJumpOffsetBounds(ir,&minDist,&maxDist,recursionsLeft);
            if (maxDist <= 0x3F && maxDist >= -0x40) { // 1-byte either way
                *min = *max = 2;
            } else if (minDist > 0x3F || minDist < -40) { // 2-byte either way
                *min = *max = 3;
            }
        } else {
            *min = 2;
            *max = 3;
        }
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
        *min = *max = 1; break;
    // Two byte ops
    case BOK_REG_READ:
    case BOK_REG_WRITE:
        *min = *max = 2; break;
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
        comment = auto_printf(32,"PUSH_IMM %d",imm);
    } break;
    case BOK_MATHOP: {
        int mathID;
        switch(ir->mathKind) {
        case MOK_ROR:       mathID = 0b00000; break;
        case MOK_ROL:       mathID = 0b00001; break;
        case MOK_SHR:       mathID = 0b00010; break;
        case MOK_SHL:       mathID = 0b00011; break;
        case MOK_MIN:       mathID = 0b00100; break;
        case MOK_MAX:       mathID = 0b00101; break;
        case MOK_NEG:       mathID = 0b00110; break;
        case MOK_BITNOT:    mathID = 0b00111; break;
        case MOK_BITAND:    mathID = 0b01000; break;
        case MOK_ABS:       mathID = 0b01001; break;
        case MOK_BITOR:     mathID = 0b01010; break;
        case MOK_BITXOR:    mathID = 0b01011; break;
        case MOK_ADD:       mathID = 0b01100; break;
        case MOK_SUB:       mathID = 0b01101; break;
        case MOK_SAR:       mathID = 0b01110; break;
        case MOK_REV:       mathID = 0b01111; break;
        case MOK_LOGICAND:  mathID = 0b10000; break;
        case MOK_ENCODE:    mathID = 0b10001; break;
        case MOK_LOGICOR:   mathID = 0b10010; break;
        case MOK_DECODE:    mathID = 0b10011; break;
        case MOK_MULLOW:    mathID = 0b10100; break;
        case MOK_MULHIGH:   mathID = 0b10101; break;
        case MOK_DIVIDE:    mathID = 0b10110; break;
        case MOK_REMAINDER: mathID = 0b10111; break;
        case MOK_SQRT:      mathID = 0b11000; break;
        case MOK_CMP_B:     mathID = 0b11001; break;
        case MOK_CMP_A:     mathID = 0b11010; break;
        case MOK_CMP_NE:    mathID = 0b11011; break;
        case MOK_CMP_E:     mathID = 0b11100; break;
        case MOK_CMP_BE:    mathID = 0b11101; break;
        case MOK_CMP_AE:    mathID = 0b11110; break;
        case MOK_BOOLNOT:   mathID = 0b11111; break;
        default:
            ERROR(NULL,"Unhandled math op type %d = %s",ir->mathKind,mathOpKindNames[ir->mathKind]);
            break;
        }
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
        int offset = CompileJumpOffset_Spin1(buf,&pos,ir,1);
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
    case BOK_LABEL: break;;
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

void BCIR_to_BOB(BCIRBuffer *irbuf,ByteOutputBuffer *bob) {
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
