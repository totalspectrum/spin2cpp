#include "common.h"
#include "nuir.h"
#include <stdio.h>
#include "sys/nuinterp.spin.h"

#define DIRECT_BYTECODE 0
#define PUSHI_BYTECODE  1
#define PUSHA_BYTECODE  2
#define FIRST_BYTECODE  3

static const char *NuOpName[] = {
    #define X(m) #m,
    NU_OP_XMACRO
    #undef X
};

static struct NuOpUsage {
    int used;
    int ircode;
    int bytecode;
} opusage[NU_OP_DUMMY];

static const char *impl_ptrs[NU_OP_DUMMY];

static int usage_sortfunc(const void *Av, const void *Bv) {
    const struct NuOpUsage *A = Av;
    const struct NuOpUsage *B = Bv;
    /* sort with larger used coming first */
    return B->used - A->used;
}

void NuIrInit(NuContext *ctxt) {
    int i;

    memset(ctxt, 0, sizeof(*ctxt));
    for (i = 0; i < NU_OP_DUMMY; i++) {
        opusage[i].used = 0;
        opusage[i].ircode = i;
        opusage[i].bytecode = -1;
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

    if (op < NU_OP_DUMMY) {
        opusage[op].used++;
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


void NuCreateBytecodes(NuIrList *irl)
{
    size_t elemsize = sizeof(opusage[0]);
    qsort(&opusage, sizeof(opusage) / elemsize, elemsize, usage_sortfunc);
    //printf("Most used opcode: %s\n", NuOpName[opusage[0].ircode]);
    //printf("Least used opcode: %s\n", NuOpName[opusage[lastelem].ircode]);
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
    const char *linestart;
    int c;
    int i;
    uint32_t heapsize = GetHeapSize() + 4;
    int bytecode;
    
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

    // add the predefined entries
    flexbuf_printf(fb, "\tword\timpl_DIRECT\n");
    flexbuf_printf(fb, "\tword\timpl_PUSHI\n");
    flexbuf_printf(fb, "\tword\timpl_PUSHA\n");
    
    // now add the jump table
    bytecode = FIRST_BYTECODE;
    for (i = 0; opusage[i].used > 0 && i < (int)NU_OP_DUMMY; i++) {
        if (opusage[i].ircode == NU_OP_PUSHI) {
            opusage[i].bytecode = PUSHI_BYTECODE;
            continue;
        }
        if (opusage[i].ircode == NU_OP_PUSHA) {
            opusage[i].bytecode = PUSHA_BYTECODE;
            continue;
        }
        flexbuf_printf(fb, "\tword\timpl_%s\n", NuOpName[opusage[i].ircode]);
        opusage[i].bytecode = bytecode++;
    }
    // end of jump table
    flexbuf_printf(fb, "\talignl\nOPC_TABLE_END\n");

    // emit constants for everything
    flexbuf_printf(fb, "\ncon\n");
    // predefined
    flexbuf_printf(fb, "\tNU_OP_DIRECT = %d\n", DIRECT_BYTECODE);
    flexbuf_printf(fb, "\tNU_OP_PUSHI = %d\n", PUSHI_BYTECODE);
    flexbuf_printf(fb, "\tNU_OP_PUSHA = %d\n", PUSHA_BYTECODE);
    // others
    for (i = 0; opusage[i].used > 0 && i < NU_OP_DUMMY; i++) {
        int bc = opusage[i].bytecode;
        if (bc >= FIRST_BYTECODE) {
            flexbuf_printf(fb, "\tNU_OP_%s = %d  ' (used %d times)\n", NuOpName[opusage[i].ircode], opusage[i].bytecode, opusage[i].used);
        }
    }
    
    // now emit opcode implementations
    flexbuf_printf(fb, "dat\n\torgh ($ < $400) ? $400 : $\n");
    for (i = 0; opusage[i].used > 0 && i < (int)NU_OP_DUMMY; i++) {
        int op = opusage[i].ircode;
        const char *ptr = impl_ptrs[op];
        if (!ptr) {
            WARNING(NULL, "no implementation for %s", NuOpName[op]);
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

void
NuOutputIrList(Flexbuf *fb, NuIrList *irl)
{
    NuIr *ir;
    NuIrOpcode op;
    if (!irl || !irl->head) {
        return;
    }
    for (ir = irl->head; ir; ir = ir->next) {
        op = ir->op;
        switch(op) {
        case NU_OP_LABEL:
            NuOutputLabel(fb, ir->label);
            break;
        case NU_OP_ALIGN:
            flexbuf_printf(fb, "\talignl");
            break;
        case NU_OP_PUSHI:
            flexbuf_printf(fb, "\tbyte\tNU_OP_%s, long %d", NuOpName[op], ir->val);
            break;
        case NU_OP_PUSHA:
            flexbuf_printf(fb, "\tbyte\t long NU_OP_%s | (", NuOpName[op]);
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
            flexbuf_printf(fb, "\tbyte\tNU_OP_%s, word (", NuOpName[op]);
            NuOutputLabel(fb, ir->label);
            flexbuf_printf(fb, " - ($+2))");
            break;
        default:
            if (op < NU_OP_DUMMY) {
                flexbuf_printf(fb, "\tbyte\tNU_OP_%s", NuOpName[op]);
            }
            break;
        }
        if (ir->comment) {
            flexbuf_printf(fb, "\t' %s", ir->comment);
        }
        flexbuf_addchar(fb, '\n');
    }
}
