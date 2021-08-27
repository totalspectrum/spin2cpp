#include "common.h"
#include "nuir.h"
#include <stdio.h>
#include "sys/nuinterp.spin.h"

#define DIRECT_BYTECODE 0
//#define PUSHI_BYTECODE  1
//#define PUSHA_BYTECODE  2
#define FIRST_BYTECODE  1

static const char *NuOpName[] = {
    #define X(m) #m,
    NU_OP_XMACRO
    #undef X
};

static const char *impl_ptrs[NU_OP_DUMMY];

static int usage_sortfunc(const void *Av, const void *Bv) {
    NuBytecode **A = (NuBytecode **)Av;
    NuBytecode **B = (NuBytecode **)Bv;
    /* sort with larger used coming first */
    return B[0]->usage - A[0]->usage;
}

void NuIrInit(NuContext *ctxt) {
    const char *ptr = (char *)sys_nuinterp_spin;
    const char *linestart;
    int c;
    int i;
    
    memset(ctxt, 0, sizeof(*ctxt));

    // scan for implementation of primitives
    // first, skip the initial interpreter
    for(;;) {
        c = *ptr++;
        if (!c || c == '\014') break;
    }
    // some functions are always built in to the interpreter
    impl_ptrs[NU_OP_DROP] = "";
    impl_ptrs[NU_OP_DROP2] = "";
    impl_ptrs[NU_OP_DUP] = "";
    impl_ptrs[NU_OP_SWAP] = "";
    impl_ptrs[NU_OP_OVER] = "";
    impl_ptrs[NU_OP_CALL] = "";
    impl_ptrs[NU_OP_CALLM] = "";
    impl_ptrs[NU_OP_GOSUB] = "";
    impl_ptrs[NU_OP_ENTER] = "";
    impl_ptrs[NU_OP_RET] = "";
    impl_ptrs[NU_OP_INLINEASM] = "";
    impl_ptrs[NU_OP_PUSHI] = "";
    impl_ptrs[NU_OP_PUSHA] = "";
    
    // find the other implementations that we may need
    while (c) {
        linestart = ptr;
        if (!strncmp(linestart, "impl_", 5)) {
            // find the opcode this is an implementation of
            ptr = linestart + 5;
            for (i = 0; i < NU_OP_DUMMY; i++) {
                int n = strlen(NuOpName[i]);
                if (!strncmp(ptr, NuOpName[i], n) && ptr[n] == '\n') {
                    if (impl_ptrs[i]) {
                        ERROR(NULL, "Duplicate definition for %s\n", NuOpName[i]);
                    }
                    impl_ptrs[i] = linestart;
                    break;
                }
            }
        }
        do {
            c = *ptr++;
        } while (c != '\n' && c != 0);
    }
}

NuIrLabel *NuCreateLabel() {
    NuIrLabel *r;
    static int labelnum = 0;
    r = calloc(sizeof(*r), 1);
    r->num = labelnum++;
    snprintf(r->name, sizeof(r->name), "__Label_%05u", r->num);
    return r;
}

static NuIr *NuCreateIr() {
    NuIr *r;
    r = calloc(sizeof(*r), 1);
    return r;
}

NuIr *NuEmitOp(NuIrList *irl, NuIrOpcode op) {
    NuIr *r;
    NuIr *last;
    
    r = NuCreateIr();
    r->op = op;
    last = irl->tail;
    irl->tail = r;
    r->prev = last;
    if (last) {
        last->next = r;
    }
    if (!irl->head) {
        irl->head = r;
    }
    return r;
}

NuIr *NuEmitCommentedOp(NuIrList *irl, NuIrOpcode op, const char *comment) {
    NuIr *r = NuEmitOp(irl, op);
    if (r) {
        r->comment = comment;
    }
    return r;
}

NuIr *NuEmitConst(NuIrList *irl, int32_t val) {
    NuIr *r;

    r = NuEmitOp(irl, NU_OP_PUSHI);
    r->val = val;
    return r;
}

NuIr *NuEmitAddress(NuIrList *irl, NuIrLabel *label) {
    NuIr *r = NuEmitOp(irl, NU_OP_PUSHA);
    r->label = label;
    return r;
}

NuIr *NuEmitBranch(NuIrList *irl, NuIrOpcode op, NuIrLabel *label) {
    NuIr *r = NuEmitOp(irl, op);
    r->label = label;
    return r;
}

NuIr *NuEmitLabel(NuIrList *irl, NuIrLabel *label) {
    NuIr *r = NuEmitOp(irl, NU_OP_LABEL);
    r->label = label;
    return r;
}

NuIr *NuEmitNamedOpcode(NuIrList *irl, const char *name) {
    NuIrOpcode op;
    for (op = NU_OP_ILLEGAL; op < NU_OP_DUMMY; op++) {
        if (!strcasecmp(NuOpName[op], name)) {
            break;
        }
    }
    if (op == NU_OP_DUMMY) {
        ERROR(NULL, "Unknown opcode %s", name);
        return NULL;
    }
    return NuEmitOp(irl, op);
}

#define MAX_BYTECODE 0x4000
static int num_bytecodes = 0;
static NuBytecode *globalBytecodes[MAX_BYTECODE];
static NuBytecode *staticOps[NU_OP_DUMMY];

static NuBytecode *
AllocBytecode()
{
    NuBytecode *b;
    if (num_bytecodes == MAX_BYTECODE) {
        return NULL;
    }
    b = calloc(sizeof(*b), 1);
    b->usage = 1;
    globalBytecodes[num_bytecodes] = b;
    num_bytecodes++;
    return b;
}

static NuBytecode *
GetBytecodeFor(NuIr *ir)
{
    NuBytecode *b;
    if (ir->op >= NU_OP_DUMMY) {
        return NULL;
    }
    b = staticOps[ir->op];
    if (b) {
        b->usage++;
        return b;
    }
    b = AllocBytecode();
    if (b) {
        b->name = NuOpName[ir->op];
        b->impl_ptr = impl_ptrs[ir->op];
        staticOps[ir->op] = b;
        if (ir->op >= NU_OP_BRA && ir->op < NU_OP_DUMMY) {
            b->is_branch = 1;
        }
    } else {
        ERROR(NULL, "Internal error, too many bytecodes\n");
        return NULL;
    }
    return b;
}

#define NuBcIsBranch(bc) ((bc)->is_branch)

void NuCreateBytecodes(NuIrList *irl)
{
    NuIr *ir;
    int i;
    int code;
    
    // create an initial set of bytecodes
    while (irl) {
        for (ir = irl->head; ir; ir = ir->next) {
            ir->bytecode = GetBytecodeFor(ir);
        }
        irl = irl->nextList;
    }

    // sort the bytecodes in order of usage
    size_t elemsize = sizeof(NuBytecode *);
    qsort(&globalBytecodes, num_bytecodes, elemsize, usage_sortfunc);

    // assign bytecodes based on order of usage
    code = FIRST_BYTECODE;
    for (i = 0; i < num_bytecodes; i++) {
        if (code < 0xf0 && (globalBytecodes[i]->usage > 1 || NuBcIsBranch(globalBytecodes[i]))) {
            globalBytecodes[i]->code = code++;
        } else {
            globalBytecodes[i]->code = DIRECT_BYTECODE;
        }
    }
}

void NuOutputLabel(Flexbuf *fb, NuIrLabel *label) {
    if (!label) {
        flexbuf_printf(fb, "0");
        return;
    }
    if (label->offset) {
        flexbuf_printf(fb, "(%s + %d)", label->name, label->offset);
    } else {
        flexbuf_printf(fb, "%s", label->name);
    }
}
void NuOutputLabelNL(Flexbuf *fb, NuIrLabel *label) {
    NuOutputLabel(fb, label);
    flexbuf_addchar(fb, '\n');
}

static uint32_t nu_heap_size;

static void
OutputEscapedChar(Flexbuf *fb, int c, NuContext *ctxt)
{
    switch(c) {
    case '0':
        flexbuf_printf(fb, "%u", ctxt->clockFreq);
        break;
    case '1':
        flexbuf_printf(fb, "$%x", ctxt->clockMode);
        break;
    case '2':
        NuOutputLabel(fb, ctxt->entryPt);
        break;
    case '3':
        NuOutputLabel(fb, ctxt->initObj);
        break;
    case '4':
        NuOutputLabel(fb, ctxt->initFrame);
        break;
    case '5':
        NuOutputLabel(fb, ctxt->initSp);
        break;
    case '6':
        flexbuf_printf(fb, "%u", nu_heap_size / 4);
        break;
    default:
        ERROR(NULL, "Unknown escape char %c", c);
        break;
    }
}

static long GetHeapSize() {
    Symbol *sym;
    if (! (gl_features_used & FEATURE_NEED_HEAP)) return 0;
    sym = LookupSymbolInTable(&systemModule->objsyms, "__real_heapsize__");
    if (!sym || sym->kind != SYM_CONSTANT) return 0;
    uint32_t heapsize = EvalPasmExpr((AST *)sym->val) * LONG_SIZE;

    heapsize += 4*LONG_SIZE; // reserve a slot at the end
    return heapsize;
}

void NuOutputInterpreter(Flexbuf *fb, NuContext *ctxt)
{
    const char *ptr = (char *)sys_nuinterp_spin;
    int c;
    int i;
    uint32_t heapsize = GetHeapSize() + 4;
    
    heapsize = (heapsize+3)&~3; // long align
    nu_heap_size = heapsize;
    
    // copy until ^L
    for(;;) {
        c = *ptr++;
        if (c == 0 || c == '\014') break;
        if (c == '\001') {
            c = *ptr++;
            OutputEscapedChar(fb, c, ctxt);
        } else {
            flexbuf_addchar(fb, c);
        }
    }

    // add the predefined entries
    flexbuf_printf(fb, "\tword\timpl_DIRECT\n");
    //flexbuf_printf(fb, "\tword\timpl_PUSHI\n");
    //flexbuf_printf(fb, "\tword\timpl_PUSHA\n");
    
    // now add the jump table
    for (i = 0; i < num_bytecodes; i++) {
        NuBytecode *bc = globalBytecodes[i];
        int code = bc->code;
        if (code >= FIRST_BYTECODE) {
            flexbuf_printf(fb, "\tword\timpl_%s\n", bc->name);
        }
    }
    // end of jump table
    flexbuf_printf(fb, "\talignl\nOPC_TABLE_END\n");

    // emit constants for everything
    flexbuf_printf(fb, "\ncon\n");
    // predefined
    flexbuf_printf(fb, "\tNU_OP_DIRECT = %d\n", DIRECT_BYTECODE);
    // others
    for (i = 0; i < num_bytecodes; i++) {
        NuBytecode *bc = globalBytecodes[i];
        int code = bc->code;
        if (code >= FIRST_BYTECODE) {
            flexbuf_printf(fb, "\tNU_OP_%s = %d  ' (used %d times)\n", bc->name, code, bc->usage);
        }
    }
    
    // now emit opcode implementations
    flexbuf_printf(fb, "dat\n\torgh ($ < $400) ? $400 : $\n");
    for (i = 0; i < num_bytecodes; i++) {
        NuBytecode *bc = globalBytecodes[i];
        const char *ptr = bc->impl_ptr;
        if (!ptr) {
            WARNING(NULL, "no implementation for %s", bc->name);
            continue;
        }
        for(;;) {
            c = *ptr++;
            if (!c) break;
            flexbuf_addchar(fb, c);
            // empty line indicates end of code
            if (c == '\n' && *ptr == '\n') {
                flexbuf_addchar(fb, c);
                break;
            }
        }
    }
}

void NuOutputFinish(Flexbuf *fb, NuContext *ctxt)
{
    int c;
    // find tail of interpreter
    const char *ptr = (char *)sys_nuinterp_spin;
    ptr += sys_nuinterp_spin_len-1;
    // go back to last ^L
    while (ptr && *ptr != '\014') {
        --ptr;
    }
    // output tail
    ptr++;
    for(;;) {
        c = *ptr++;
        if (c == 0) break;
        if (c == '\001') {
            c = *ptr++;
            OutputEscapedChar(fb, c, ctxt);
        } else {
            flexbuf_addchar(fb, c);
        }
    }
}

static const char *
NuBytecodeString(NuBytecode *bc) {
    static char dummy[1024];
    if (bc->code == DIRECT_BYTECODE) {
        sprintf(dummy, "NU_OP_DIRECT, word impl_%s", bc->name);
    } else {
        sprintf(dummy, "NU_OP_%s", bc->name);
    }
    return dummy;
}

void
NuOutputIrList(Flexbuf *fb, NuIrList *irl)
{
    NuIr *ir;
    NuIrOpcode op;
    NuBytecode *bc;
    if (!irl || !irl->head) {
        return;
    }
    for (ir = irl->head; ir; ir = ir->next) {
        op = ir->op;
        bc = ir->bytecode;
        switch(op) {
        case NU_OP_LABEL:
            NuOutputLabel(fb, ir->label);
            break;
        case NU_OP_ALIGN:
            flexbuf_printf(fb, "\talignl");
            break;
        case NU_OP_PUSHI:
            flexbuf_printf(fb, "\tbyte\tNU_OP_PUSHI, long %d", ir->val);
            break;
        case NU_OP_PUSHA:
            flexbuf_printf(fb, "\tbyte\t long NU_OP_PUSHA | (");
            NuOutputLabel(fb, ir->label);
            flexbuf_printf(fb, " << 8)");
            break;
        case NU_OP_BRA:
        case NU_OP_CBEQ:
        case NU_OP_CBNE:
        case NU_OP_CBLTS:
        case NU_OP_CBLES:
        case NU_OP_CBLTU:
        case NU_OP_CBLEU:
        case NU_OP_CBGTS:
        case NU_OP_CBGES:
        case NU_OP_CBGTU:
        case NU_OP_CBGEU:
            flexbuf_printf(fb, "\tbyte\t%s, word (", NuBytecodeString(bc));
            NuOutputLabel(fb, ir->label);
            flexbuf_printf(fb, " - ($+2))");
            break;
        default:
            if (bc) {
                flexbuf_printf(fb, "\tbyte\t%s", NuBytecodeString(bc));
            }
            break;
        }
        if (ir->comment) {
            flexbuf_printf(fb, "\t' %s", ir->comment);
        }
        flexbuf_addchar(fb, '\n');
    }
}
