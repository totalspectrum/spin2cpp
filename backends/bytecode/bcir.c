
#include "bcir.h"
#include "bc_spin1.h"
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

int pbase_offset; // distance of function from PBASE (obj header)
BCIRBuffer *current_birb;


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

bool BCIR_SizeDetermined(ByteOpIR *ir) {
    return ir->fixedSize || ir->kind == BOK_LABEL;
}

void BCIR_GetJumpOffsetBounds(ByteOpIR *jump,bool func_relative,int *minDist, int *maxDist,int recursionsLeft) {
    ByteOpIR *searchingFor = jump->jumpTo;
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

int BCIR_GetJumpOffset(ByteOpIR *jump,bool func_relative) {
    int min,max;
    BCIR_GetJumpOffsetBounds(jump,func_relative,&min,&max,0);
    if (min!=max) ERROR(NULL,"Internal error: GetJumpOffset called, got indeterminate offset (%d..%d)",min,max);
    return max;
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
