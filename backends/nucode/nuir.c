#include "common.h"
#include "nuir.h"
#include <stdio.h>
#include "sys/nuinterp.spin.h"

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

void NuIrInit() {
    int i;
    for (i = 0; i < NU_OP_DUMMY; i++) {
        opusage[i].used = 0;
        opusage[i].ircode = i;
        opusage[i].bytecode = -1;
    }
}

NuIrLabel *NuCreateLabel() {
    NuIrLabel *r;
    r = calloc(sizeof(*r), 1);
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

NuIr *NuEmitConst(NuIrList *irl, int32_t val) {
    NuIr *r;

    if (val >= -128 && val <= 127) {
        r = NuEmitOp(irl, NU_OP_PUSHI8);
    } else if (val >= -32768 && val <= 32767) {
        r = NuEmitOp(irl, NU_OP_PUSHI16);
    } else {
        r = NuEmitOp(irl, NU_OP_PUSHI32);
    }
    r->val = val;
    return r;
}

NuIr *NuEmitAddress(NuIrList *irl, NuIrLabel *label) {
    NuIr *r = NuEmitOp(irl, NU_OP_PUSHA);
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


void NuAssignOpcodes()
{
    size_t elemsize = sizeof(opusage[0]);
    qsort(&opusage, sizeof(opusage) / elemsize, elemsize, usage_sortfunc);
    //printf("Most used opcode: %s\n", NuOpName[opusage[0].ircode]);
    //printf("Least used opcode: %s\n", NuOpName[opusage[lastelem].ircode]);
}

void NuOutputInterpreter(Flexbuf *fb)
{
    const char *ptr = (char *)sys_nuinterp_spin;
    const char *linestart;
    int c;
    int i;
    
    // copy until ^L
    for(;;) {
        c = *ptr++;
        if (c == 0 || c == '\014') break;
        flexbuf_addchar(fb, c);
    }
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

    // now add the jump table
    for (i = 0; opusage[i].used > 0 && i < (int)NU_OP_DUMMY; i++) {
        flexbuf_printf(fb, "\tword\t@@@impl_%s\n", NuOpName[opusage[i].ircode]);
    }
    // end of jump table
    flexbuf_printf(fb, "\talignl\nOPC_TABLE_END\n\norgh\n");

    // now emit opcode implementations
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
