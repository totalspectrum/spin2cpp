//
// Bytecode IR processing for spin2cpp
//
// Copyright 2021 Ada Gottensträter and Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include "bcir.h"
#include "bc_spin1.h"
#include "bc_bedata.h"
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

ByteOpIR *BIRB_MakeCopy(ByteOpIR *ir) {
    ByteOpIR *newIR = (ByteOpIR *)malloc(sizeof(ByteOpIR));
    memcpy(newIR,ir,sizeof(ByteOpIR));
    return newIR;
}

ByteOpIR *BIRB_PushCopy(BCIRBuffer *buf,ByteOpIR *ir) {
    ByteOpIR *newIR = BIRB_MakeCopy(ir);
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

void BIRB_ReplaceInplace(ByteOpIR *target,ByteOpIR *ir) {
    ByteOpIR *next = target->next,*prev = target->prev;
    *target = *ir;
    target->next = next,target->prev = prev;
}

void BIRB_InsertBefore(BCIRBuffer *buf,ByteOpIR *target,ByteOpIR *ir) {
    ir->next = target;
    if (target->prev) target->prev->next = ir;
    else buf->head = ir;
    target->prev = ir;
}

void BIRB_RemoveBlock(BCIRBuffer *buf,ByteOpIR *first,ByteOpIR *last) {
    if (last->next) last->next->prev = first->prev;
    else buf->tail = first->prev;

    if (first->prev) first->prev->next = last->next;
    else buf->head = last->next;
}

void BIRB_Remove(BCIRBuffer *buf,ByteOpIR *ir) {
    BIRB_RemoveBlock(buf,ir,ir);
}


void BIRB_MoveBlock(BCIRBuffer *buf,ByteOpIR *target,ByteOpIR *first,ByteOpIR *last) {
    
    BIRB_RemoveBlock(buf,first,last);

    if (target == NULL) { // Insert as head
        if (buf->head) buf->head->prev = last;
        buf->head = first;
        first->prev = NULL;
    } else {
        if (target->next) target->next->prev = last;
        else buf->tail = last;
        last->next = target->next; // Null if at tail
        first->prev = target;
        target->next = first;
    }
}

void BIRB_AppendPending(BCIRBuffer *buf) {
    if (!buf->pending || buf->pending->opCount == 0) return;
    buf->tail->next = buf->pending->head;
    buf->pending->head->prev = buf->tail;
    buf->tail = buf->pending->tail;

    buf->pending->head = NULL;
    buf->pending->tail = NULL;
    buf->pending->opCount = 0;
}

static bool BCIR_SizeDetermined(ByteOpIR *ir) {
    return ir->fixedSize >= 0;
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
    if (min!=max) ERROR(NULL,"Internal error, GetJumpOffset called (on a %s), got indeterminate offset (%d..%d)",byteOpKindNames[jump->kind],min,max);
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
    case BOK_FUNDATA_LOOKUPJUMP:
    case BOK_FUNDATA_JUMPENTRY:
    case BOK_CASE:
    case BOK_CASE_RANGE:
        return true;
    case BOK_MEM_MODIFY:
    case BOK_REG_MODIFY:
        return ir->mathKind == MOK_MOD_REPEATSTEP;
    default: return false;
    }
}

bool BCIR_isJump(ByteOpIR *ir) {
    switch(ir->kind) {
    case BOK_JUMP:
    case BOK_JUMP_TJZ:
    case BOK_JUMP_DJNZ:
    case BOK_JUMP_IF_Z:
    case BOK_JUMP_IF_NZ:
    case BOK_CASE:
    case BOK_CASE_RANGE:
        return true;
    case BOK_MEM_MODIFY:
    case BOK_REG_MODIFY:
        return ir->mathKind == MOK_MOD_REPEATSTEP;
    case BOK_FUNDATA_PUSHADDRESS:
        return ir->attr.pushaddress.forJump;
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
    case BOK_JUMP_IF_NZ:
    case BOK_JUMP_IF_Z:
        return ir->attr.condjump.logicallyTerminal;
    default: return false;
    }
}

// Currently same as BCIR_IsTerminalOp, but if any large terminal ops are added, this may come in handy
bool BCIR_CanReplaceJumpToOpWithItself(ByteOpIR *ir,bool stackDirty) {
    switch(ir->kind) {
    //case BOK_JUMP: // Handled seperately...
    case BOK_CASE_DONE:
    case BOK_LOOKEND:
    case BOK_ABORT_POP:
        return !stackDirty;
    case BOK_ABORT_PLAIN:
        return true;
    case BOK_RETURN_PLAIN:
        return (ir->attr.returninfo.numResults == 1);
    case BOK_RETURN_POP:
        return (ir->attr.returninfo.numResults == 1) && !stackDirty;
    default: return false;
    }
}

bool BCIR_CanRemoveBeforeReturn(ByteOpIR *ir) {
    switch(ir->kind) {
    case BOK_CONSTANT:
    case BOK_POP:
        return true;
    default: return false;
    }
}

bool BCIR_CanBeOversized(ByteOpIR *ir) {
    switch (ir->kind) {
    case BOK_ALIGN: return false;
    default: return true;
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
    if (memcmp(&ir1->attr.memop,&ir2->attr.memop,sizeof(ir1->attr.memop))) {
        return false;
    }
    return true;
}

bool BCIR_IsResultMemop(ByteOpIR *ir) {
    if (interp_can_multireturn()) return false; // There is no one RESULT in Spin2

    return BCIR_IsConstMemOp(ir) && ir->attr.memop.base == MEMOP_BASE_DBASE && ir->data.int32 == 0 && ir->attr.memop.memSize == MEMOP_SIZE_LONG;
}

unsigned BCIR_GetRefCount(BCIRBuffer *irbuf,ByteOpIR *label) {
    unsigned refs = 0;
    for (ByteOpIR *ir=irbuf->head;ir;ir=ir->next) {
        if (ir->jumpTo == label && BCIR_UsesLabel(ir)) refs++;
    }
    return refs;
}

ByteOpIR *BCIR_AnyRef(BCIRBuffer *irbuf,ByteOpIR *label) {
    for (ByteOpIR *ir=irbuf->head;ir;ir=ir->next) {
        if (ir->jumpTo == label && BCIR_UsesLabel(ir)) return ir;
    }
    return false;
}

static bool BCIR_OptDeadCode() {
    // Any code between a terminal op and a label can be deleted safely
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        if(BCIR_IsTerminalOp(ir)) {
            while (ir->next && !(BCIR_IsLabel(ir->next) || (ir->next->kind == BOK_ALIGN && ir->next->next && BCIR_IsLabel(ir->next->next)))) {
                BIRB_Remove(current_birb,ir->next);
                didWork = true;
            }
        }
    }
    return didWork;
}

static bool BCIR_OptRemoveBeforeReturn() {
    // Since RETURN_PLAIN and ABORT_PLAIN discard the stack frame, some ops can be deleted if preceding one
    // This usually happens as a result of BCIR_OptReplaceJumpToOpWithItself
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        if((ir->kind == BOK_RETURN_PLAIN && ir->attr.returninfo.numResults == 1) || ir->kind == BOK_ABORT_PLAIN) {
            while (ir->prev && BCIR_CanRemoveBeforeReturn(ir->prev)) {
                BIRB_Remove(current_birb,ir->prev);
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
            BIRB_Remove(current_birb,ir);
            didWork = true;
        }
    }
    return didWork;
}

static bool BCIR_OptUnusedLabel() {
    // An unreferenced label can be removed, consecutive labels can be combined
    // This is an O(x^2) operation, but it's only per-function, so it's ok(tm)
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        if (ir->kind == BOK_LABEL) {
            if(!BCIR_AnyRef(current_birb,ir)) {
                BIRB_Remove(current_birb,ir);
                didWork = true;
            } else {
                while (ir->next && ir->next->kind == BOK_LABEL) {
                    for (ByteOpIR *pj=current_birb->head;pj;pj=pj->next) {
                        if (BCIR_UsesLabel(pj) && pj->jumpTo == ir->next) pj->jumpTo = ir;
                    }
                    BIRB_Remove(current_birb,ir->next);
                    didWork = true;
                }
            }
        }
    }
    return didWork;
}

static bool BCIR_OptJumpOverJump() {
    // A conditional jump that jumps over a single unconditional jump can be combined with it
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        if ((ir->kind == BOK_JUMP_IF_Z || ir->kind == BOK_JUMP_IF_NZ) && ir->next && ir->next->kind == BOK_JUMP && ir->next->next == ir->jumpTo) {
            ir->jumpTo = ir->next->jumpTo;
            ir->kind = ir->kind == BOK_JUMP_IF_Z ? BOK_JUMP_IF_NZ : BOK_JUMP_IF_Z;
            BIRB_Remove(current_birb,ir->next);
            didWork = true;
        }
    }
    return didWork;
}

static bool BCIR_OptJumpToJump() {
    // Any kind of jump that jumps to an unconditional jump can jump to its target instead
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        if (BCIR_isJump(ir) && ir->jumpTo->next && ir->jumpTo->next != ir && ir->jumpTo->next->kind == BOK_JUMP) {
            ir->jumpTo = ir->jumpTo->next->jumpTo;
            didWork = true;
        }
    }
    return didWork;
}

static bool BCIR_OptReplaceJumpToOpWithItself() {
    // An unconditional jump to a small terminal op can be replaced with that op
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        bool stackDirty, tryReplace = false;
        switch (ir->kind) {
        case BOK_JUMP:
            tryReplace = true;
            stackDirty = false;
            break;
        case BOK_JUMP_IF_Z:
        case BOK_JUMP_IF_NZ:
            tryReplace = ir->attr.condjump.logicallyTerminal;
            stackDirty = true;
            break;
        default: break;
        }
        if (tryReplace) {
            ByteOpIR *next = ir->jumpTo->next;
            if (BCIR_CanReplaceJumpToOpWithItself(next,stackDirty)) {
                BIRB_ReplaceInplace(ir,next);
                didWork = true;
            }
        }
    }
    return didWork;
}

#define MOVE_SINGLE_JUMP_THRESHOLD 18 // This could probably be tuned better

static bool BCIR_OptMoveSingleJumpLabel() {
    // A block following a terminal op referenced by a single unconditional jump can be moved after it
    // This i.e. makes OTHER cases move to the top if possible
    bool didWork = false;
    for(ByteOpIR *ir = current_birb->head;ir;ir=ir->next) {
        if (ir->kind == BOK_LABEL && ir->prev && BCIR_IsTerminalOp(ir->prev) && BCIR_GetRefCount(current_birb,ir) == 1) {
            ByteOpIR *jump = BCIR_AnyRef(current_birb,ir);
            if (!jump || jump->kind != BOK_JUMP) continue;
            ByteOpIR *first = ir->next;
            ByteOpIR *last = first;
            if (!first) continue;
            unsigned blockops = 1;
            // Find the block
            while (last && !BCIR_IsTerminalOp(last)) {
                last = last->next;
                blockops++;
            }
            if (!last || last == jump) continue;

            // Simple heuristic to avoid pessimisation:
            // If there are more than some thereshold of IR ops to move, don't do it
            if (blockops > MOVE_SINGLE_JUMP_THRESHOLD) continue;

            // Remove the label first
            BIRB_Remove(current_birb,ir); 
            // ... and set ir so we can continue iterating after the block
            ir = last->next;
            // Now move the block
            BIRB_MoveBlock(current_birb,jump,first,last);
            // ... and then the jump
            BIRB_Remove(current_birb,jump);

            didWork = true;

            // Fixup the iteration
            if (!ir) break;
            ir = ir->prev;
            if (!ir) break;
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
            BIRB_Remove(current_birb,ir->next);
            ir->kind = BOK_MEM_MODIFY;
            ir->mathKind = MOK_MOD_WRITE;
            ir->attr.memop.pushModifyResult = true;
            didWork = true;
        } else if (ir->kind == BOK_MEM_MODIFY && ir->next && ir->next->kind == BOK_MEM_READ &&
        ModOperatorPushesTrueResult(ir->mathKind) && !ir->attr.memop.pushModifyResult &&
        ir->attr.memop.memSize == MEMOP_SIZE_LONG && BCIR_IsEqualMemOpTarget(ir,ir->next))  {
            BIRB_Remove(current_birb,ir->next);
            ir->attr.memop.pushModifyResult = true;
            didWork = true;
        }
    }
    return didWork;
}

static bool BCIR_OptContractReturn() {
    // write result + return_plain -> return_pop
    // read_result + return_pop -> return_plain
    bool didWork = false;
    for (ByteOpIR *ir=current_birb->head;ir;ir=ir->next) {
        if (ir->kind == BOK_RETURN_PLAIN && ir->attr.returninfo.numResults == 1 && ir->prev && ir->prev->kind == BOK_MEM_WRITE && BCIR_IsResultMemop(ir->prev)) {
            BIRB_Remove(current_birb,ir->prev);
            ir->kind = BOK_RETURN_POP;
            didWork = true;
        } else if (ir->kind == BOK_RETURN_POP && ir->attr.returninfo.numResults == 1 && ir->prev && ir->prev->kind == BOK_MEM_READ && BCIR_IsResultMemop(ir->prev)) {
            BIRB_Remove(current_birb,ir->prev);
            ir->kind = BOK_RETURN_PLAIN;
            didWork = true;
        }
    }
    return didWork;
}

#define BCOPT_MAX_ITERATIONS 50

void BCIR_Optimize(BCIRBuffer *irbuf) {
    current_birb = irbuf;
    int flags = curfunc->optimize_flags;
    bool didWork;
    int iterations = 0;
    do {
        didWork = false;
        if (flags & OPT_DEADCODE) didWork |= BCIR_OptDeadCode();
        if (flags & OPT_DEADCODE) didWork |= BCIR_OptPointlessJump();
        if (flags & OPT_DEADCODE) didWork |= BCIR_OptUnusedLabel();
        if (flags & OPT_PEEPHOLE) didWork |= BCIR_OptMoveSingleJumpLabel();
        if (flags & OPT_PEEPHOLE) didWork |= BCIR_OptContractWriteRead();
        if (flags & OPT_PEEPHOLE) didWork |= BCIR_OptContractReturn();
        if (flags & OPT_PEEPHOLE) didWork |= BCIR_OptJumpOverJump();
        if (flags & OPT_PEEPHOLE) didWork |= BCIR_OptJumpToJump();
        if (flags & OPT_PEEPHOLE) didWork |= BCIR_OptReplaceJumpToOpWithItself();
        if (flags & OPT_DEADCODE) didWork |= BCIR_OptRemoveBeforeReturn();
    } while (didWork && ++iterations < BCOPT_MAX_ITERATIONS);
    if (iterations >= BCOPT_MAX_ITERATIONS) WARNING(curfunc->body,"Optimization pass limit (%d) exceeded on function %s",BCOPT_MAX_ITERATIONS,curfunc->name);
    DEBUG(NULL,"took %d passes to optimize function %s",iterations,curfunc->name);
    current_birb = NULL;
}

// Kindof duplicates BCCompilePopN...
static void BCIR_InsertPopNBefore(BCIRBuffer *irbuf, ByteOpIR *jump, int popcount) {
    if (popcount < 0) ERROR(NULL,"Internal Error: negative pop count");
    else if (popcount > 0) {
        ByteOpIR popAmount = {.kind = BOK_CONSTANT,.data.int32 = popcount*4};
        BIRB_InsertBefore(irbuf,jump,BIRB_MakeCopy(&popAmount));
        ByteOpIR popOp = {.kind = BOK_POP};
        BIRB_InsertBefore(irbuf,jump,BIRB_MakeCopy(&popOp));
    }
}

void BCIR_ResolveNamedLabels(BCIRBuffer *irbuf) {
    for (ByteOpIR *lbl=irbuf->head;lbl;lbl=lbl->next) {
        if (lbl->kind == BOK_NAMEDLABEL) {
            const char *name = lbl->data.stringPtr;
            lbl->kind = BOK_LABEL;
            for (ByteOpIR *jr=irbuf->head;jr;jr=jr->next) {
                if (BCIR_UsesLabel(jr) && jr->jumpTo->kind == BOK_NAMEDLABEL && !strcmp(name,jr->jumpTo->data.stringPtr)) {
                    int stackdiff = jr->jumpTo->attr.labelHiddenVars - lbl->attr.labelHiddenVars;
                    if (stackdiff > 0) {
                        if (jr->kind == BOK_JUMP) BCIR_InsertPopNBefore(irbuf,jr,stackdiff);
                        else ERROR(NULL,"Named label reference to %s with uneven stack depth",name);
                    } else if (stackdiff < 0) ERROR(NULL,"Jump to named label %s with deeper stack",name);
                    jr->jumpTo = lbl;
                }
            }
        }
    }
    // Check for any unresolved labels
    for (ByteOpIR *jr=irbuf->head;jr;jr=jr->next) {
        if (BCIR_UsesLabel(jr)) {
            if (jr->jumpTo) {
                if (jr->jumpTo->kind == BOK_NAMEDLABEL) ERROR(NULL,"unresolved label %s in function %s",jr->jumpTo->data.stringPtr,curfunc->name);
            } else ERROR(NULL,"Internal Error: Missing label in function %s",curfunc->name);
        }
    }
}

static bool
BCIR_DetermineSizes(BCIRBuffer *irbuf,bool force,int maxRecursion) {
    DEBUG(NULL,"In BCIR_DetermineSizes with maxRecursion = %d and force = %c",maxRecursion,force?'Y':'N');
    bool didSomething = false;
    for (ByteOpIR *ir=irbuf->head;ir;ir=ir->next) {
        if (BCIR_SizeDetermined(ir)) continue;
        int min=-1,max=-1;
        GetSizeBound_Func(ir,&min,&max,maxRecursion);
        if (min<0||max<0) ERROR(NULL,"Internal error, size bounds negative");
        if (min==max) {
            ir->fixedSize = max;
            didSomething = true;
        } else if (force && BCIR_CanBeOversized(ir)) {
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
        DEBUG(NULL,"An IR of kind %s is not determined",byteOpKindNames[ir->kind]);
        return false;
    }
    return true;
}

static void
BCIR_Compact(BCIRBuffer *irbuf,int maxRecursion) {
    for(ByteOpIR *ir=irbuf->head;ir;ir=ir->next) ir->fixedSize = -1; // Initialize all sizes to -1
    for(;;) {
        // Fix sizes until we cant anymore
        while (BCIR_DetermineSizes(irbuf,false,maxRecursion));
        if (BCIR_AllDetermined(irbuf)) return; // All good
        else BCIR_DetermineSizes(irbuf,true,maxRecursion); // Do an oversized encoding
    }
}

void BCIR_to_BOB(BCIRBuffer *irbuf,ByteOutputBuffer *bob,int pbase_funoffset) {
    pbase_offset = pbase_funoffset;
    current_birb = irbuf;
    if (!irbuf->opCount) return;
    BCIR_Compact(irbuf,2);
    for(ByteOpIR *ir=irbuf->head;ir;ir=ir->next) {
        OutputSpan *instrSpan = 0;
        if (ir->fixedSize<0) {
            ERROR(NULL,"Internal Errror: IR with negative size");
            continue;
        }
        instrSpan = (OutputSpan *)calloc(sizeof(OutputSpan)+ir->fixedSize,1);
        if (!instrSpan) {
            ERROR(NULL,"Out of memory (while allocating instruction span)");
        }
        instrSpan->size = ir->fixedSize;
        const char *comment = CompileIROP_Func(instrSpan->data,instrSpan->size,ir);
        if (!comment) comment = "(MISSING COMMENT)";
        instrSpan->comment = comment;
        BOB_PushSpan(bob,instrSpan);
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
