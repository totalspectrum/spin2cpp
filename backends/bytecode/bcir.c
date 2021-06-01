
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

void BCIR_Remove(BCIRBuffer *buf,ByteOpIR *ir) {
    if (ir->prev) ir->prev->next = ir->next;
    else buf->head = ir->next;
    if (ir->next) ir->next->prev = ir->prev;
    else buf->tail = ir->prev;

    // the passed node alone to make iteration easier
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
    if (min!=max) ERROR(NULL,"Internal error: GetJumpOffset called (on a %s), got indeterminate offset (%d..%d)",byteOpKindNames[jump->kind],min,max);
    return max;
}

bool BCIR_UsesLabel(ByteOpIR *ir) {
    switch(ir->kind) {
    case BOK_JUMP:
    case BOK_JUMP_TJZ:
    case BOK_JUMP_DJNZ:
    case BOK_JUMP_IF_Z:
    case BOK_JUMP_IF_NZ:
    case BOK_FUNDATA_PUSHADDRESS:
    case BOK_CASE:
    case BOK_CASE_RANGE:
        return true;
    case BOK_MEM_MODIFY:
    case BOK_REG_MODIFY:
        return ir->mathKind == MOK_MOD_REPEATSTEP;
    default: return false;
    }
}

bool BCIR_IsLabel(ByteOpIR *ir) {
    return ir->kind == BOK_LABEL;
}

bool BCIR_IsTerminalOp(ByteOpIR *ir) {
    switch(ir->kind) {
    case BOK_JUMP:
    case BOK_CASE_DONE:
    case BOK_LOOKEND:
    case BOK_RETURN_PLAIN:
    case BOK_RETURN_POP:
    case BOK_ABORT_PLAIN:
    case BOK_ABORT_POP:
        return true;
    default: return false;
    }
}

// Is this memOp constant (i.e. doesn't pop any values to determine its address)
bool BCIR_IsConstMemOp(ByteOpIR *ir) {
    return ir->attr.memop.base != MEMOP_BASE_POP && !ir->attr.memop.popIndex && ir->mathKind != MOK_MOD_REPEATSTEP;
}

// Are two MemOps targeting the same address?
bool BCIR_IsEqualMemOpTarget(ByteOpIR *ir1,ByteOpIR *ir2) {
    if (ir1->data.int32 != ir2->data.int32) return false;
    if (!BCIR_IsConstMemOp(ir1) || !BCIR_IsConstMemOp(ir2)) return false;
    if (memcmp(&ir1->attr.memop,&ir2->attr.memop,sizeof(struct bcir_memop_attr))) return false;

    return true;
}

unsigned BCIR_GetRefCount(BCIRBuffer *irbuf,ByteOpIR *label) {
    unsigned refs = 0;
    for (ByteOpIR *ir=irbuf->head;ir;ir=ir->next) {
        if (ir->jumpTo == label && BCIR_UsesLabel(ir)) refs++;
    }
    return refs;
}

bool BCIR_AnyRef(BCIRBuffer *irbuf,ByteOpIR *label) {
    for (ByteOpIR *ir=irbuf->head;ir;ir=ir->next) {
        if (ir->jumpTo == label && BCIR_UsesLabel(ir)) return true;
    }
    return false;
}

static bool BCIR_OptDeadCode() {
    // Any code between a terminal op and a label can be deleted safely
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        if(BCIR_IsTerminalOp(ir)) {
            while (ir->next && !BCIR_IsLabel(ir->next)) {
                BCIR_Remove(current_birb,ir->next);
                didWork = true;
            }
        }
    }
    return didWork;
}

static bool BCIR_OptPointlessJump() {
    // A jump directly before the label it targets can be removed
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        if(ir->kind == BOK_JUMP && ir->jumpTo == ir->next) {
            BCIR_Remove(current_birb,ir);
            didWork = true;
        }
    }
    return didWork;
}

static bool BCIR_OptUnusedLabel() {
    // An unreferenced label can be removed
    // This is an O(x^2) operation, but it's only per-function, so it's ok(tm)
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        if(ir->kind == BOK_LABEL && !BCIR_AnyRef(current_birb,ir)) {
            BCIR_Remove(current_birb,ir);
            didWork = true;
        }
    }
    return didWork;
}

static bool BCIR_OptContractWriteRead() {
    // Writing a location and then immediately reading it can be contracted into a single modify op
    // Modifying a location without pop and then immediately reading it can be contracted into
    // All of this for long types only, I think
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        if(ir->kind == BOK_MEM_WRITE && ir->next && ir->next->kind == BOK_MEM_READ &&
        ir->attr.memop.memSize == MEMOP_SIZE_LONG && BCIR_IsEqualMemOpTarget(ir,ir->next)) {
            BCIR_Remove(current_birb,ir->next);
            ir->kind = BOK_MEM_MODIFY;
            ir->mathKind = MOK_MOD_WRITE;
            ir->attr.memop.pushModifyResult = true;
            didWork = true;
        } else if (ir->kind == BOK_MEM_MODIFY && ir->next && ir->next->kind == BOK_MEM_READ &&
        ModOperatorPushesTrueResult(ir->mathKind) && !ir->attr.memop.pushModifyResult &&
        ir->attr.memop.memSize == MEMOP_SIZE_LONG && BCIR_IsEqualMemOpTarget(ir,ir->next))  {
            BCIR_Remove(current_birb,ir->next);
            ir->attr.memop.pushModifyResult = true;
            didWork = true;
        }
    }
    return didWork;
}

void BCIR_Optimize(BCIRBuffer *irbuf) {
    current_birb = irbuf;
    int flags = curfunc->optimize_flags;
    bool didWork;
    do {
        didWork = false;
        if (flags & OPT_DEADCODE) didWork |= BCIR_OptDeadCode();
        if (flags & OPT_DEADCODE) didWork |= BCIR_OptPointlessJump();
        if (flags & OPT_DEADCODE) didWork |= BCIR_OptUnusedLabel();
        if (flags & OPT_PEEPHOLE) didWork |= BCIR_OptContractWriteRead();
    } while (didWork);
    current_birb = NULL;
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
    current_birb = NULL;
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
