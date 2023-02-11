//
// Bytecode (nucode) compiler for spin2cpp
//
// Copyright 2021-2023 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include "common.h"
#include "nuir.h"
#include <stdio.h>
#include <ctype.h>
#include "sys/nuinterp.spin.h"
#include "becommon.h"

#define DIRECT_BYTECODE 0
#define PUSHI_BYTECODE  1
#define PUSHA_BYTECODE  2
#define CALLA_BYTECODE  3
#define FIRST_BYTECODE  4
#define MAX_BYTECODE 0xf8

static const char *NuOpName[] = {
#define X(m) #m,
    NU_OP_XMACRO
#undef X
};

static const char *impl_ptrs[NU_OP_DUMMY];
static int impl_sizes[NU_OP_DUMMY];

static int usage_sortfunc(const void *Av, const void *Bv) {
    NuBytecode **A = (NuBytecode **)Av;
    NuBytecode **B = (NuBytecode **)Bv;
    /* sort with larger used coming first */
    return B[0]->usage - A[0]->usage;
}

static int codenum_sortfunc(const void *Av, const void *Bv) {
    NuBytecode **A = (NuBytecode **)Av;
    NuBytecode **B = (NuBytecode **)Bv;
    /* sort with smaller codes coming first */
    return A[0]->code - B[0]->code;
}

static int NuImplSize(const char *lineptr) {
    int c;
    int size = 0;
    if (!strncmp(lineptr, "impl_", 5)) {
        --size; // ignore label
    }
    for(;;) {
        c = *lineptr++;
        if (c == 0) break;
        if (c == '\n') {
            size++;
            while (*lineptr == ' ' || *lineptr == '\t')
                lineptr++;
            if (*lineptr == '\'') {
                // commented line, ignore
                --size;
            }
            if (*lineptr == '\n') break;
        }
        if (c == '#' && lineptr[0] == '#') {
            size++;
        }
    }
    return size;
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
    impl_ptrs[NU_OP_DUP2] = "";
    impl_ptrs[NU_OP_SWAP2] = "";
    impl_ptrs[NU_OP_OVER] = "";
    impl_ptrs[NU_OP_CALL] = "";
    impl_ptrs[NU_OP_CALLM] = "";
    impl_ptrs[NU_OP_ENTER] = "";
    impl_ptrs[NU_OP_RET] = "";
    impl_ptrs[NU_OP_PUSHI] = "";
    impl_ptrs[NU_OP_PUSHA] = "";
    impl_ptrs[NU_OP_CALLA] = "";
    impl_ptrs[NU_OP_BREAK] = "";
    impl_ptrs[NU_OP_GETHEAP] = "";
    
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
                    impl_sizes[i] = NuImplSize(impl_ptrs[i]);
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
    r = (NuIrLabel *)calloc(sizeof(*r), 1);
    r->num = labelnum++;
    snprintf(r->name, sizeof(r->name), "__Label_%05u", r->num);
    return r;
}

static NuIr *NuCreateIr() {
    NuIr *r;
    r = (NuIr *)calloc(sizeof(*r), 1);
    return r;
}

NuIr *NuCreateIrOp(NuIrOpcode op) {
    NuIr *r = NuCreateIr();
    r->op = op;
    return r;
}

NuIr *NuEmitOp(NuIrList *irl, NuIrOpcode op) {
    NuIr *r;
    NuIr *last;

    r = NuCreateIrOp(op);
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

NuIr *NuEmitComment(NuIrList *irl, const char *comment) {
    return NuEmitCommentedOp(irl, NU_OP_COMMENT, comment);
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

NuIr *NuEmitCommentedAddress(NuIrList *irl, NuIrLabel *label, const char *comment) {
    NuIr *r = NuEmitOp(irl, NU_OP_PUSHA);
    r->label = label;
    r->comment = comment;
    return r;
}

NuIr *NuEmitCall(NuIrList *irl, NuIrOpcode op, NuIrLabel *label, const char *comment) {
    NuIr *r = NuEmitOp(irl, op);
    r->label = label;
    r->comment = comment;
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
    int i;
    for (i = (int)NU_OP_ILLEGAL; i < (int)NU_OP_DUMMY; i++) {
        op = (NuIrOpcode)i;
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

#define MAX_NUM_BYTECODE 0x8000
static int num_bytecodes = 0;
static NuBytecode *globalBytecodes[MAX_NUM_BYTECODE];
static NuBytecode *staticOps[NU_OP_DUMMY];
#define MAX_CONST_OPS 0xffff
static NuBytecode *constOps[MAX_CONST_OPS+1];

static NuBytecode *
AllocBytecode()
{
    NuBytecode *b;
    if (num_bytecodes == MAX_NUM_BYTECODE) {
        return NULL;
    }
    b = (NuBytecode *)calloc(sizeof(*b), 1);
    b->usage = 1;
    globalBytecodes[num_bytecodes] = b;
    num_bytecodes++;
    return b;
}

static NuBytecode *
GetBytecodeForConst(intptr_t val, int is_label, int bytecode)
{
    int hash;
    NuBytecode *b;

    hash = val & MAX_CONST_OPS;
    for (b = constOps[hash]; b; b = b->link) {
        if (b->value == val && b->code == bytecode) {
            b->usage++;
            return b;
        }
    }
    b = AllocBytecode();
    if (!b) {
        ERROR(NULL, "ran out of bytecode space");
        return NULL;
    }
    b->code = bytecode;
    b->value = val;
    b->link = constOps[hash];
    b->is_const = 1;
    b->is_label = is_label;
    constOps[hash] = b;
    return b;
}

static NuBytecode *
GetBytecodeFor(NuIr *ir)
{
    NuBytecode *b;
    if (ir->op >= NU_OP_DUMMY) {
        return NULL;
    }
    if (ir->op == NU_OP_PUSHI) {
        return GetBytecodeForConst(ir->val, 0, PUSHI_BYTECODE);
    }
    if (ir->op == NU_OP_PUSHA) {
        return GetBytecodeForConst( (intptr_t)(ir->label), 1, PUSHA_BYTECODE);
    }
    if (ir->op == NU_OP_CALLA) {
        b = GetBytecodeForConst( (intptr_t)(ir->label), 1, CALLA_BYTECODE);
        return b;
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
        if (b->impl_ptr && !b->impl_ptr[0]) {
            // builtin: access it via jumping to it
            b->impl_ptr = auto_printf(64, "\tjmp\t#\\impl_%s\n\n", b->name);
            b->impl_size = 1;
        } else {
            b->impl_size = impl_sizes[ir->op];
        }
        staticOps[ir->op] = b;
        if (ir->op >= NU_OP_JMP && ir->op < NU_OP_DUMMY) {
            b->is_any_branch = 1;
            if (ir->op >= NU_OP_BRA) {
                b->is_rel_branch = 1;
            }
        }
        if (ir->op >= NU_OP_ADD && ir->op <= NU_OP_MOVBYTS) {
            b->is_binary_op = 1;
        }
        if (ir->op == NU_OP_INLINEASM) {
            b->is_inline_asm = 1;
        }
    } else {
        ERROR(NULL, "Internal error, too many bytecodes\n");
        return NULL;
    }
    return b;
}
//
// scan list of instructions for pairs that may be combined into a macro
// only opcodes that have been assigned single bytecodes may be merged
//
typedef struct NuMacro {
    NuBytecode *firstCode;
    NuBytecode *secondCode;
    int count;
    int depth;
} NuMacro;

static NuMacro macros[256][256];

#define MAX_MACRO_DEPTH 4

// scan for potential macro pairs
static NuMacro *NuScanForMacros(NuIrList *lists, int *savings) {
    NuIrList *irl = lists;
    NuIr *ir;
    int maxCount = 0;
    NuBytecode *prevCode, *curCode;
    NuMacro *where = 0;
    int savedBytes;
    static int found_macro = 0;

//    if (found_macro > 48) {
//        return NULL;
//    }
    memset(macros, 0, sizeof(macros));
    while (irl) {
        prevCode = NULL;
        for (ir = irl->head; ir; ir = ir->next) {
            curCode = ir->bytecode;
            if (curCode && (curCode->is_inline_asm || curCode->is_rel_branch)) {
                // no macros involving inline asm or relative branches
                curCode = NULL;
            }
            if (curCode && prevCode && curCode->macro_depth < MAX_MACRO_DEPTH && prevCode->macro_depth < MAX_MACRO_DEPTH) {
                int bc1, bc2;
                bc1 = prevCode->code;
                bc2 = curCode->code;
                if (bc1 >= FIRST_BYTECODE && bc2 >= FIRST_BYTECODE) {
                    macros[bc1][bc2].count++;
                    if (macros[bc1][bc2].count > maxCount) {
                        maxCount = macros[bc1][bc2].count;
                        where = &macros[bc1][bc2];
                        where->firstCode = prevCode;
                        where->secondCode = curCode;
                    }
                }
            }
            if (curCode && !curCode->is_any_branch) {
                prevCode = curCode;
            } else {
                prevCode = NULL;
            }
        }
        irl = irl->nextList;
    }
    // figure out the benefit of doing this replacement
    // the new macro requires at least 8 bytes implementation
    // it will save 1 byte per invocation
    savedBytes = maxCount - 7;
    if (savedBytes < 0) {
        return NULL;
    }
    *savings = savedBytes;
    found_macro++;
    where->depth = where->firstCode->macro_depth;
    if (where->secondCode->macro_depth > where->depth) {
        where->depth = where->secondCode->macro_depth;
    }
    where->depth += 1;
    return where;
}

//
// recalculate usage of bytescodes
//
static void NuRecalcUsage(NuIrList *lists) {
    NuIrList *irl = lists;
    NuIr *ir;
    NuBytecode *curCode;
    int i;

    for (i = 0; i < num_bytecodes; i++) {
        globalBytecodes[i]->usage = 0;
    }
    while (irl) {
        for (ir = irl->head; ir; ir = ir->next) {
            curCode = ir->bytecode;
            if (curCode) {
                curCode->usage++;
            }
        }
        irl = irl->nextList;
    }
    qsort(&globalBytecodes, num_bytecodes, sizeof(globalBytecodes[0]), usage_sortfunc);
}

#define MAX_INSTR_SEQ_LEN 4

// find bytecode to compress
// this may be either a single PUSHI/PUSHA (which is compressed by creating a single opcode immediate version)
// or a macro pair (which is compressed by creating a new bytecode which does both macros)

static NuBytecode *NuFindCompressBytecode(NuIrList *irl, int *savings) {
    int i;
    NuBytecode *bc;
    int savedBytes;

    // globalBytecodes are assumed sorted by usage...
    for (i = 0; i < num_bytecodes; i++) {
        bc = globalBytecodes[i];
        if ( (bc->code == PUSHI_BYTECODE || bc->code == PUSHA_BYTECODE) && bc->usage > 1) {
            // cost of implementation is 8 bytes for small, 12 for large
            int impl_cost = (MAX_INSTR_SEQ_LEN+1)*4;
            // cost of each invocation is 5 bytes normally (PUSHI + data)
            // so we save 4 bytes by replacing with a singleton
            int invoke_cost = 4;
            if (bc->value >= 0 && bc->value <= 0xff) {
                invoke_cost = 1;
            } else if (bc->value >= 0 && bc->value <= 0xffff) {
                invoke_cost = 2;
            }
            if (bc->value >= -511 && bc->value <= 511) {
                impl_cost -= 4; // saves a prefix
            }
            savedBytes = (invoke_cost * bc->usage) - impl_cost;
            if (savedBytes < 0) {
                return NULL;
            }
            *savings = savedBytes;
            return bc;
        }
    }
    return NULL;
}

static void
NuCopyImpl(Flexbuf *fb, const char *lineptr, int skipRet) {
    int c;
    if (!strncmp(lineptr, "impl_", 5)) {
        // skip label
        while (*lineptr && *lineptr != '\n') lineptr++;
        if (*lineptr) lineptr++;
    }
    for(;;) {
        c = *lineptr++;
        if (!c) break;
        flexbuf_addchar(fb, c);
        if (c == '\n' && (lineptr[0] == 0 || lineptr[0] == '\n')) {
            break;
        }
        if (c == ' ' || c == '\t') {
            if (skipRet) {
                if (!strncmp(lineptr, "_ret_\t", 6) || !strncmp(lineptr, "_ret_ ", 6)) {
                    flexbuf_addstr(fb, "     ");
                    lineptr += 5;
                } else if (!strncmp(lineptr, "jmp\t", 4) || !strncmp(lineptr, "jmp ", 4)) {
                    flexbuf_addstr(fb, "call");
                    lineptr += 3;
                }
            }
        }
    }
}

// check for special pattern like PUSH_n_ADD_DBASE_LDL; if found return n
static int
isPushAddDbaseLdl(const char *bcname) {
    int n = 0;
    const char *ptr = bcname;
    
    if (strncmp(ptr, "PUSH_", 5) != 0)
        return -1;
    ptr += 5;
    while (*ptr && isdigit(*ptr)) {
        n = 10*n + (*ptr) - '0';
        ptr++;
    }
    if (strcmp(ptr, "_ADD_DBASE_LDL") != 0) {
        return -1;
    }
    return n;
}

// check for special pattern like PUSH_n_ADD_DBASE_LDL; if found return n
static int
isPushAddDbaseStl(const char *bcname) {
    int n = 0;
    const char *ptr = bcname;
    
    if (strncmp(ptr, "PUSH_", 5) != 0)
        return -1;
    ptr += 5;
    while (*ptr && isdigit(*ptr)) {
        n = 10*n + (*ptr) - '0';
        ptr++;
    }
    if (strcmp(ptr, "_ADD_DBASE_STL") != 0) {
        return -1;
    }
    return n;
}

static
const char *NuMergeBytecodes(const char *bcname, NuBytecode *first, NuBytecode *second) {
    Flexbuf *fb;
    Flexbuf fb_s;

    fb = &fb_s;
    flexbuf_init(fb, 256);
    flexbuf_printf(fb, "impl_%s\n", bcname);

    if (first->is_small_const && second->is_binary_op && first->value >= 0 && first->value <= 511) {
        char *opname = strdup(second->name);
        char *s;

        if (!strcmp(opname, "MINS")) {
            strcpy(opname, "fges");
        } else if (!strcmp(opname, "MAXS")) {
            strcpy(opname, "fles");
        } else if (!strcmp(opname, "MINU")) {
            strcpy(opname, "fge");
        } else if (!strcmp(opname, "MAXU")) {
            strcpy(opname, "fle");
        } else if (!strcmp(opname, "IOR")) {
            strcpy(opname, "or");
        } else {
            for (s = opname; *s; s++) {
                *s = tolower(*s);
            }
        }
        flexbuf_printf(fb, " _ret_\t%s\ttos, #%d\n", opname, first->value);
        free(opname);
    } else {
        int n;
        /* special case a few things */
        if ( (n = isPushAddDbaseLdl(bcname)) >= 0 && n < 128 && 0 == (n & 3) ) {
            flexbuf_printf(fb, "\tcall\t#\\impl_DUP\n _ret_\trdlong\ttos, ptrb[%d]\n", n/4);
        } else if ( (n = isPushAddDbaseStl(bcname)) >= 0 && n < 128 && 0 == (n & 3) ) {
            flexbuf_printf(fb, "\twrlong\ttos, ptrb[%d]\n\tjmp\t#\\impl_DROP", n/4);            
        } else if (!strcmp(bcname, "PUSH_0_ADD_DBASE")) {
            flexbuf_printf(fb, "\tcall\t#\\impl_DUP\n  _ret_\tmov\ttos, dbase\n");
        } else if (!strcmp(bcname, "PUSH_0_ADD_VBASE")) {
            flexbuf_printf(fb, "\tcall\t#\\impl_DUP\n  _ret_\tmov\ttos, vbase\n");
        } else if (!strcmp(bcname, "PUSH_0_ADD_VBASE_LDL")) {
            flexbuf_printf(fb, "\tcall\t#\\impl_DUP\n  _ret_\trdlong\ttos, vbase\n");
        } else if (first->impl_size + second->impl_size <= MAX_INSTR_SEQ_LEN) {
            NuCopyImpl(fb, first->impl_ptr, 1);
            NuCopyImpl(fb, second->impl_ptr, 0);
        } else if (first->impl_size + 1 <= MAX_INSTR_SEQ_LEN) {
            NuCopyImpl(fb, first->impl_ptr, 1);
            flexbuf_printf(fb, "\tjmp\t#\\impl_%s\n", second->name);
        } else if (second->impl_size + 1 <= MAX_INSTR_SEQ_LEN) {
            flexbuf_printf(fb, "\tcall\t#\\impl_%s\n", first->name);
            NuCopyImpl(fb, second->impl_ptr, 0);
        } else {
            flexbuf_printf(fb, "\tcall\t#\\impl_%s\n", first->name);
            flexbuf_printf(fb, "\tjmp\t#\\impl_%s\n", second->name);
        }
    }
    flexbuf_addchar(fb, '\n');
    flexbuf_addchar(fb, 0);
    return flexbuf_get(fb);
}

static NuBytecode *NuReplaceMacro(NuIrList *lists, NuMacro *macro) {
    NuIrList *irl = lists;
    NuIr *ir;
    NuBytecode *bc, *first, *second;

    bc = AllocBytecode();
    if (!bc) {
        return NULL;
    }
    bc->usage = 0;
    bc->macro_depth = macro->depth;
    first = macro->firstCode;
    second = macro->secondCode;
    bc->is_any_branch = first->is_any_branch | second->is_any_branch;
    bc->name = auto_printf(128, "%s_%s", first->name, second->name);
    bc->impl_ptr = NuMergeBytecodes(bc->name, first, second);
    bc->impl_size = NuImplSize(bc->impl_ptr);
    while (irl) {
        for (ir = irl->head; ir; ir = ir->next) {
            NuIr *delir = ir->next;
            if (ir->bytecode == first && delir && delir->bytecode == second) {
                ir->bytecode = bc;
                bc->usage++;
                ir->next = delir->next;
                if (ir->next) {
                    ir->next->prev = ir;
                } else {
                    irl->tail = ir;
                }
                first->usage--;
                second->usage--;
            }
        }

        irl = irl->nextList;
    }
    return bc;
}

#define NuBcIsRelBranch(bc) ((bc)->is_rel_branch)

void NuCreateBytecodes(NuIrList *lists)
{
    NuIr *ir;
    NuIrList *irl;
    int i;
    int code;
    NuBytecode *bc;

    int lut_size = 0x300;
    
    // create an initial set of bytecodes
    irl = lists;
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
        bc = globalBytecodes[i];
        if (bc->is_const) {
            // already assigned bytecode
            //if (bc->is_label) {
            //    globalBytecodes[i]->code = PUSHA_BYTECODE;
            //} else {
            //    globalBytecodes[i]->code = PUSHI_BYTECODE;
            //}
        } else if (NuBcIsRelBranch(globalBytecodes[i])) {
            globalBytecodes[i]->code = code++;
        } else if (code >= MAX_BYTECODE) {
            // out of bytecode space
            globalBytecodes[i]->code = DIRECT_BYTECODE;
        } else {
            globalBytecodes[i]->code = code++;
        }
    }

    // while there's room for more bytecodes, find ways to compress the code
    while (code < (MAX_BYTECODE-1) && (gl_optimize_flags & OPT_MAKE_MACROS)) {
        int32_t val;
        int cost;
        int compressValue, macroValue;
        const char *instr = "mov";
        const char *opname;
        NuMacro *macro;

        compressValue = 0;
        NuRecalcUsage(lists);
        bc = NuFindCompressBytecode(lists, &compressValue);
        macro = NuScanForMacros(lists, &macroValue);
        if (bc) {
            if (macro) {
                // pick which is better
                if (compressValue >= macroValue) {
                    cost = compressValue;
                    macro = NULL;
                } else {
                    cost = macroValue;
                    bc = NULL;
                }
            } else {
                cost = compressValue;
            }
        } else {
            cost = macroValue;
        }
        if (cost <= 0) {
            // no space savings, do we get performance savings?
            if (lut_size > 0x3f8) {
                // nope, bail
                bc = NULL;
                macro = NULL;
            }
        }
        if (bc) {
            const char *immflag = "#";
            const char *valstr;
            const char *namestr;
            NuIrLabel *label = NULL;

            opname = "PUSH_";
            if (bc->is_label) {
                label = (NuIrLabel *)bc->value;
                if (label->offset) {
                    valstr = auto_printf(128, "(%s+%u)", label->name, label->offset);
                    namestr = auto_printf(128, "%s_%u", label->name, label->offset);
                } else {
                    valstr = namestr = label->name;
                }
            } else {
                val = (int32_t)bc->value;
                if (val < 0) {
                    val = -val;
                    instr = "neg";
                    opname = "PUSH_M";
                }
                if (val >= 0 && val < 512) {
                    immflag = "";
                }
                valstr = namestr = auto_printf(32, "%u", val);
                bc->is_small_const = 1;
            }
            bc->name = auto_printf(32, "%s%s", opname, namestr);
            // impl_PUSH_0
            //       call #\impl_DUP
            // _ret_ mov  tos, #0
            bc->impl_ptr = auto_printf(128, "impl_%s\n\tcall\t#\\impl_DUP\n _ret_\t%s\ttos, #%s%s\n\n", bc->name, instr, immflag, valstr);
            bc->impl_size = (immflag[0]) ? 3 : 2;
            bc->is_const = 0; // don't need to emit PUSHI for this one
        } else if (macro) {
            bc = NuReplaceMacro(lists, macro);
        } else {
            break;
        }
        bc->code = code++;
        lut_size += bc->impl_size;
    }
    // finally, sort byte bytecode
    qsort(&globalBytecodes, num_bytecodes, elemsize, codenum_sortfunc);
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
    case '7':
        flexbuf_printf(fb, "%u", ctxt->varSize / 4);
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
    uint32_t heapsize = EvalPasmExpr((AST *)sym->v.ptr) * LONG_SIZE;

    heapsize += 4*LONG_SIZE; // reserve a slot at the end
    return heapsize;
}

void NuOutputInterpreter(Flexbuf *fb, NuContext *ctxt)
{
    const char *ptr = (char *)sys_nuinterp_spin;
    int c;
    int i;
    uint32_t heapsize = GetHeapSize() + 4;
    int impl_pc = 0x300;
    int impl_max = 0x3f8; // really 0x3ff, but give a bit of slop just in case
    int saw_orgh = 0;
    heapsize = (heapsize+3)&~3; // long align
    nu_heap_size = heapsize;

    // output lead in
    flexbuf_printf(fb, "con\n");
    flexbuf_printf(fb, "  _clkfreq = %d\n", ctxt->clockFreq);
    flexbuf_printf(fb, "  clock_freq_addr = $14\n");
    flexbuf_printf(fb, "  clock_mode_addr = $18\n\n");

    flexbuf_printf(fb, "dat\n");
    if (!gl_no_coginit && gl_output != OUTPUT_COGSPIN) {
        flexbuf_addstr(fb, "\torg 0\n");
        flexbuf_addstr(fb, "\tnop\n");
        flexbuf_addstr(fb, "\tcogid\tpa\n");
        flexbuf_addstr(fb, "\tcoginit\tpa, ##@real_init\n");
        flexbuf_printf(fb, "\torgh\t$%x\n", P2_CONFIG_BASE);
        flexbuf_addstr(fb, "\tlong\t0\t' reserved (crystal frequency on Taqoz)\n");
        flexbuf_addstr(fb, "\tlong\t0\t' clock frequency ($14)\n");
        flexbuf_addstr(fb, "\tlong\t0\t' clock mode      ($18)\n");
        flexbuf_addstr(fb, "\tlong\t0\t' reserved for baud ($1c)\n");
        flexbuf_addstr(fb, "\torgh\t$80\t' $40-$80 reserved\n");
    }

    // copy interpreter source until ^L
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

    // emit opcode implementations
    // these start in LUT from $300 - $400
    flexbuf_printf(fb, "\ndat\n\torg\t$300\n");
    flexbuf_printf(fb, "IMPL_LUT\n");
    for (i = 0; i < num_bytecodes; i++) {
        NuBytecode *bc = globalBytecodes[i];
        const char *ptr = bc->impl_ptr;
        if (!ptr) {
            if ( ! (bc->is_const) ) {
                WARNING(NULL, "no implementation for %s", bc->name);
            }
            continue;
        }
        if (strncmp(ptr, "impl_", 5) != 0) {
            // does not start with impl_, so not really needed
            continue;
        }
        impl_pc += bc->impl_size;
        if (impl_pc >= impl_max && !saw_orgh) {
            saw_orgh = 1;
            flexbuf_printf(fb, "\torgh\n");
        }
        if (saw_orgh) {
            bc->in_hub = 1;
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
        if (!saw_orgh) {
            flexbuf_printf(fb, "' pc= 0x%x\n", impl_pc);
        }
    }

    if (!saw_orgh) {
        flexbuf_printf(fb, "\n\torgh ($ < $400) ? $400 : $\n");
    }
    // now add the jump table
    flexbuf_printf(fb, "\nOPC_TABLE\n");
    // add the predefined entries
    flexbuf_printf(fb, "\tlong\timpl_DIRECT\n");
    flexbuf_printf(fb, "\tlong\timpl_PUSHI\n");
    flexbuf_printf(fb, "\tlong\timpl_PUSHA\n");
    flexbuf_printf(fb, "\tlong\timpl_CALLA\n");

    for (i = 0; i < num_bytecodes; i++) {
        NuBytecode *bc = globalBytecodes[i];
        int code = bc->code;
        if (code >= FIRST_BYTECODE) {
            if (bc->in_hub) {
                flexbuf_printf(fb, "\tlong\t(impl_%s<<16)|trampoline  ' in HUB\n", bc->name);
            } else {
                flexbuf_printf(fb, "\tlong\timpl_%s\n", bc->name);
            }
        }
    }
    // end of jump table
    flexbuf_printf(fb, "\talignl\nOPC_TABLE_END\n");

    // emit constants for everything
    flexbuf_printf(fb, "\ncon\n");
    // predefined
    flexbuf_printf(fb, "\tNU_OP_DIRECT = %d\n", DIRECT_BYTECODE);
    flexbuf_printf(fb, "\tNU_OP_PUSHI = %d\n", PUSHI_BYTECODE);
    flexbuf_printf(fb, "\tNU_OP_PUSHA = %d\n", PUSHA_BYTECODE);
    flexbuf_printf(fb, "\tNU_OP_CALLA = %d\n", CALLA_BYTECODE);
    // others
    for (i = 0; i < num_bytecodes; i++) {
        NuBytecode *bc = globalBytecodes[i];
        int code = bc->code;
        if (code >= FIRST_BYTECODE) {
            flexbuf_printf(fb, "\tNU_OP_%s = %d  ' (used %d times)\n", bc->name, code, bc->usage);
        }
    }

    // after this comes the actual bytecode
    flexbuf_printf(fb, "\ndat\n\torgh\n");
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
    switch (bc->code) {
    case DIRECT_BYTECODE:
        sprintf(dummy, "NU_OP_DIRECT, word impl_%s", bc->name);
        break;
    case PUSHI_BYTECODE:
        strcpy(dummy, "NU_OP_PUSHI");
        break;
    case PUSHA_BYTECODE:
        strcpy(dummy, "NU_OP_PUSHA");
        break;
    case CALLA_BYTECODE:
        strcpy(dummy, "NU_OP_CALLA");
        break;
    default:
        sprintf(dummy, "NU_OP_%s", bc->name);
        break;
    }
    return dummy;
}

void
NuOutputIrList(Flexbuf *fb, NuIrList *irl)
{
    NuIr *ir;
    NuIrOpcode op;
    NuBytecode *bc;
    const char *comment;
    static int labelNum = 0;
    
    if (!irl || !irl->head) {
        return;
    }
    for (ir = irl->head; ir; ir = ir->next) {
        op = ir->op;
        bc = ir->bytecode;
        comment = ir->comment;
        switch(op) {
        case NU_OP_LABEL:
            NuOutputLabel(fb, ir->label);
            break;
        case NU_OP_ALIGN:
            flexbuf_printf(fb, "\talignl");
            break;
        case NU_OP_BRA3:
            /* must always take 3 bytes because of its use with JMPREL */
            ++labelNum;
            flexbuf_printf(fb, "\tbyte\t%s, word (", NuBytecodeString(bc));
            NuOutputLabel(fb, ir->label);
            flexbuf_printf(fb, " - __L_relbranch_%05u)", labelNum);
            if (comment) {
                flexbuf_printf(fb, "\t' %s", comment);
                comment = NULL;
            }
            flexbuf_printf(fb, "\n__L_relbranch_%05u", labelNum);
            break;
        case NU_OP_BRA:
        case NU_OP_BZ:
        case NU_OP_BNZ:
        case NU_OP_DJNZ:
        case NU_OP_DJNZ_FAST:
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
            ++labelNum;
            flexbuf_printf(fb, "\tbyte\t%s, fvars (", NuBytecodeString(bc));
            NuOutputLabel(fb, ir->label);
            flexbuf_printf(fb, " - __L_relbranch_%05u)", labelNum);
            if (comment) {
                flexbuf_printf(fb, "\t' %s", comment);
                comment = NULL;
            }
            flexbuf_printf(fb, "\n__L_relbranch_%05u", labelNum);
            break;
        default:
            if (bc) {
                if (bc->is_const) {
                    const char *name = NuBytecodeString(bc);
                    if (bc->is_label) {
                        //flexbuf_printf(fb, "\tbyte\tlong %s | (", name);
                        //NuOutputLabel(fb, ir->label);
                        //flexbuf_printf(fb, "<< 8)");
                        flexbuf_printf(fb, "\tbyte\t%s, fvar ", name);
                        NuOutputLabel(fb, ir->label);
                    } else if (ir->val >= 0 && ir->val <= 0xffffff) {
                        name = "NU_OP_PUSHA";
                        flexbuf_printf(fb, "\tbyte\t%s, fvar %d", name, ir->val);
                    } else {
                        flexbuf_printf(fb, "\tbyte\t%s, long %d", name, ir->val);
                    }
                } else {
                    flexbuf_printf(fb, "\tbyte\t%s", NuBytecodeString(bc));
                }
            }
            break;
        }
        if (comment) {
            flexbuf_printf(fb, "\t' %s", comment);
        }
        flexbuf_addchar(fb, '\n');
    }
}

void
DumpNuIr(NuIrList *irl)
{
    NuIr *ir = irl->head;
    while (ir) {
        switch (ir->op) {
        default:
            printf("\t%s\n", NuOpName[ir->op]);
        }
        ir = ir->next;
    }
}
