//
// Pasm data output for spin2cpp
//
// Copyright 2016-2021 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "spinc.h"
#include "becommon.h"
#include "outasm.h"

#define MAX_COGSPIN_ARGS 8
#define MAX_ARG_REGISTER 32
#define MAX_LOCAL_REGISTER 150

#define SETJMP_BUF_SIZE (8*LONG_SIZE)

#define ENTRYNAME "entry"
#define COG_CODE (gl_outputflags & OUTFLAG_COG_CODE)
#define HUB_CODE (!COG_CODE)
#define COG_DATA (gl_outputflags & OUTFLAG_COG_DATA)
#define HUB_DATA (!COG_DATA)

#define IS_FAST_CALL(f) ( (f == 0) || !FuncData(f) || FuncData(f)->convention == FAST_CALL)
#define IS_STACK_CALL(f) ( (f != 0) && FuncData(f)->convention == STACK_CALL)

#define ALL_VARS_ON_STACK(f) ( IS_STACK_CALL(f) || f->local_address_taken || f->closure)
#define ANY_VARS_ON_STACK(f) ( ALL_VARS_ON_STACK(f) || f->stack_local )

#define IS_LEAF(func) ((gl_compress == 0) && (func)->is_leaf)

/* lists of instructions in hub and cog */
static IRList cogcode;
static IRList hubcode;
static IRList lutcode;
static IRList cogdata;
static IRList cogbss;
static IRList hubdata;

/* max size of arguments passed to coginit/cognew */
/* will default to 4 if coginit/cognew is never used */
/* FIXME: this is inaccurate if coginit/cognew is called with a
 * function pointer :(
 */
static int max_coginit_args = 4;

/* operands for multiply/divide */
Operand *mulfunc, *unsmulfunc, *muldiva, *muldivb;
Operand *divfunc, *unsdivfunc;

Operand *putcogreg;

static Operand *allocfunc;
static Operand *copyfunc;
static Operand *longjmpfunc;
static Operand *setjmpfunc;
static Operand *abortresult;
static Operand *abortcaught;
static Operand *abortchain;

Operand *resultreg[MAX_TUPLE];
Operand *argreg[MAX_ARG_REGISTER];
Operand *localreg[MAX_LOCAL_REGISTER];
static Operand *leafreg[MAX_LOCAL_REGISTER];
static Operand *debugreg[MAX_LOCAL_REGISTER];
static int debugaddr[MAX_LOCAL_REGISTER];
static Operand *sendreg, *recvreg;
Operand *lockreg, *lockreg_addr_ptr;

static Operand *nextlabel;
static Operand *quitlabel;

Operand *objbase;
static Operand *objlabel;

Operand *stackptr;

#define OPERAND_DUMMY GetArgReg(0)

static Operand *stacklabel;
static Operand *stacktop;  // indirect reference through stackptr
static Operand *frameptr;
static Operand *linkreg;

Operand *heapptr;
static Operand *heaplabel;

static Operand *hubexit;
static Operand *cogexit;

static Operand *kernelptr;

static Operand *CompileExpression(IRList *irl, AST *expr, Operand *dest);
static Operand* CompileMul(IRList *irl, AST *expr, int gethi, Operand *dest);
static Operand* CompileDiv(IRList *irl, AST *expr, int getmod, Operand *dest);
static Operand *Dereference(IRList *irl, Operand *op);
static Operand *CompileIdentifierForFunc(IRList *irl, AST *expr, Function *func);
static Operand *CompileFunccallFirstResult(IRList *irl, AST *expr);
static OperandList *CompileFunccall(IRList *irl, AST *expr);
static void CompileStatement(IRList *irl, AST *ast); /* forward declaration */

static Operand *GetAddressOf(IRList *irl, AST *expr);
static IR *EmitMove(IRList *irl, Operand *dst, Operand *src);
static void EmitBuiltins(IRList *irl);
static void CompileConsts(IRList *irl, AST *consts);
static Operand *EmitAddSub(IRList *irl, Operand *dst, int off);
static Operand *SizedHubMemRef(int size, Operand *addr, int offset);
Operand *CogMemRef(Operand *addr, int offset);
static Operand *ApplyArrayIndex(IRList *irl, Operand *base, Operand *offset, int size);

static void AssignOneFuncName(Function *f);
static void ValidatePushregs(void);
static void ValidateGosub(void);

static bool IsCogMem(Operand *addr);
static int InCog(Function *f) {
    if (f->code_placement == CODE_PLACE_DEFAULT) {
        return COG_CODE;
    }
    return f->code_placement != CODE_PLACE_HUB;
}

typedef struct AsmVariable {
    Operand *op;
    intptr_t val;
    size_t count; // number of reps of "value"
} AsmVariable;

// global variables in COG memory (really holds struct AsmVariables)
static struct flexbuf cogGlobalVars;

// global variables in hub memory (really holds struct AsmVariables)
static struct flexbuf hubGlobalVars;

static int sym_offset(Function *func, Symbol *s)
{
    int offset = -1;
    if (s->kind == SYM_RESULT) {
        offset = s->offset;
    } else if (s->kind == SYM_PARAMETER) {
        offset = LONG_SIZE*func->numresults + s->offset;
    } else if (s->kind == SYM_LOCALVAR || s->kind == SYM_TEMPVAR) {
        int realparams = func->numparams;
        if (realparams < 0) realparams = -realparams; /* varargs function */
        offset = LONG_SIZE*(func->numresults + realparams) + s->offset;
    }
    return offset;
}

static char *cleanname(const char *name)
{
    static char temp[1024];
    int i, c;
    i = 0;
    while (i < 1020) {
        c = *name++;
        if (c == 0) break;
        if (isalnum(c) || c == '_') {
            temp[i++] = c;
        } else {
            temp[i++] = '_';
            switch (c) {
            case '$':
                temp[i++] = 'S';
                break;
            case '#':
                temp[i++] = 'R';
                break;
            case '%':
                temp[i++] = 'I';
                break;
            default:
                temp[i++] = 'A' + (c & 0xF);
                temp[i++] = 'A' + (c & 0xF);
                break;
            }
        }
    }
    temp[i++] = 0;
    return temp;
}

char *
OffsetName(const char *basename, unsigned long offset)
{
    size_t len = strlen(basename) + 5;
    char *tempname = (char *)calloc(1, len);
    sprintf(tempname, "%s_%02lu", basename, offset);
    return tempname;
}

Operand *
SubRegister(Operand *reg, unsigned long offset)
{
    Operand *sub = NewOperand(REG_SUBREG, (char *)reg, 0);
    if (reg->size < offset) {
        reg->size = offset;
    }
    reg->used++;
    sub->val = offset / LONG_SIZE;
    return sub;
}

static Operand *
EmptyOperand(void)
{
    return NewImmediate(0);
}

static bool IsEmptyOperand(Operand *op)
{
    return op->kind == IMM_INT && op->val == 0;
}

const char *
IdentifierLocalName(Function *func, const char *name)
{
    char temp[1024];
    Module *P = func->module;
    if (IsTopLevel(P)) {
        snprintf(temp, sizeof(temp)-1, "_%s_", cleanname(func->name));
    } else {
        snprintf(temp, sizeof(temp)-1, "_%s_%s_", P->classname, cleanname(func->name));
    }
    strncat(temp, cleanname(name), sizeof(temp)-1);
    return strdup(temp);
}

const char *
IdentifierModuleName(Module *P, const char *name)
{
    char temp[1024];
    if (IsTopLevel(P)) {
        // avoid conflict with built-in assembler names by prepending "_"
        snprintf(temp, sizeof(temp)-1, "_%s", cleanname(name));
    } else {
        snprintf(temp, sizeof(temp)-1, "_%s_%s", P->classname, cleanname(name));
    }
    return strdup(temp);
}

Operand *GetLocalReg(int n, int isLeaf); // forward declaration

static bool
PutVarOnStack(Function *func, Symbol *sym, int size)
{
    if (ALL_VARS_ON_STACK(func)) {
        return true;
    }
    if (!ANY_VARS_ON_STACK(func)) {
        return false;
    }
    if (sym->kind == SYM_LOCALVAR && TypeGoesOnStack((AST *)sym->val)) {
        return true;
    }
    return false;
}

void
ValidateObjbase(void)
{
    if (!objbase) {
        objlabel = NewOperand(IMM_HUB_LABEL, "objmem", 0);
        objbase = NewImmediatePtr("objptr", objlabel);
    }
}

void
ValidateHeapptr(void)
{
    if (heapptr) {
        return;
    }
    if (HUB_DATA) {
        heaplabel = NewOperand(IMM_HUB_LABEL, "__heap_base", 0);
    } else {
        heaplabel = NewOperand(IMM_COG_LABEL, "__heap_base", 0);
    }
    heapptr = NewImmediatePtr("__heap_ptr", heaplabel);
}

void
ValidateStackptr(void)
{
    if (!stackptr) {
        if (HUB_DATA || gl_p2) {
            if (gl_optimize_flags & OPT_REMOVE_HUB_BSS) {
                stacklabel = NULL;
            } else {
                stacklabel = NewOperand(IMM_HUB_LABEL, "stackspace", 0);
            }
            if (gl_p2) {
                stackptr = GetOneGlobal(REG_HW, "ptra", 0);
            } else if (gl_optimize_flags & OPT_REMOVE_HUB_BSS) {
                Module *P;
                // get the top level module
                P = allparse;
                stackptr = GetOneGlobal(REG_REG, "sp", P->varsize);
            } else {
                stackptr = NewImmediatePtr("sp", stacklabel);
            }
            stacktop = SizedHubMemRef(LONG_SIZE, stackptr, 0);
        } else {
            stacklabel = NewOperand(IMM_COG_LABEL, "stackspace", 0);
            stackptr = NewImmediatePtr("sp", stacklabel);
            stacktop = CogMemRef(stackptr, 0);            
        }
    }
}
static void
ValidateFrameptr(void)
{
    if (!frameptr) {
        if (gl_p2) {
            // fp is defined with the pushregs
            ValidatePushregs();
        } else {
            frameptr = GetOneGlobal(REG_REG, "fp", 0);
        }
    }
    ValidateStackptr();
}

static void
ValidateAbortFuncs(void)
{
    if (0 == (gl_features_used & FEATURE_LONGJMP_USED)) {
        ERROR(NULL, "Internal error, using longjmp/setjmp but feature not marked\n");
    }
    if (!longjmpfunc) {
        longjmpfunc = NewOperand(IMM_COG_LABEL, "__longjmp", 0);
        setjmpfunc = NewOperand(IMM_COG_LABEL, "__setjmp", 0);
        abortchain = GetOneGlobal(REG_REG, "abortchain", 0);
        abortcaught = GetResultReg(1);
        abortresult = GetResultReg(0);
        ValidateFrameptr();
        ValidateObjbase();
    }
}

static Operand *pushregs_, *popregs_, *count_, *gosub_;

static void
ValidatePushregs(void)
{
    if (!pushregs_) {
        // we will need local01
        Operand *local1 = GetLocalReg(0, 0);
        local1->used++;
        pushregs_ = NewOperand(IMM_COG_LABEL, "pushregs_", 0);
        popregs_ = NewOperand(IMM_COG_LABEL, "popregs_", 0);
        count_ = NewOperand(REG_REG, "COUNT_", 0);
        if (gl_p2) {
            frameptr = NewOperand(REG_REG, "fp", 0);
        }
    }
}
static void
ValidateGosub(void)
{
    if (0 == (gl_features_used & FEATURE_GOSUB_USED)) {
        ERROR(NULL, "Internal error, using longjmp/setjmp but feature not marked\n");
    }    
    ValidatePushregs();
    gosub_ = NewOperand(IMM_COG_LABEL, "gosub_", 0);
}

static int IsMemRef(Operand *op)
{
    return op && (op->kind >= HUBMEM_REF) && (op->kind <= COGMEM_REF);
}

static Operand *
GetSizedVar(struct flexbuf *fb, Operandkind kind, const char *name, intptr_t value, int count)
{
  size_t siz = flexbuf_curlen(fb) / sizeof(AsmVariable);
  size_t i;
  AsmVariable tmp;
  AsmVariable *g = (AsmVariable *)flexbuf_peek(fb);
  for (i = 0; i < siz; i++) {
    if (strcmp(name, g[i].op->name) == 0) {
        if (g[i].val != value) {
            if ( (kind == REG_HUBPTR || kind == REG_COGPTR)
                 && kind == g[i].op->kind
                 && !strcmp( ((Operand *)g[i].val)->name, ((Operand *)value)->name )
                )
            {
                /* OK, pretend this is a match */
            } else {
                ERROR(NULL, "Internal error, redefining value of %s", name);
            }
        }
        if (g[i].count < count) {
            g[i].count = count;
        }
      return g[i].op;
    }
  }
  tmp.op = NewOperand(kind, name, value);
  tmp.val = value;
  tmp.count = count;
  flexbuf_addmem(fb, (const char *)&tmp, sizeof(tmp));
  return tmp.op;
}

Operand *GetOneGlobal(Operandkind kind, const char *name, intptr_t value)
{
    return GetSizedVar(&cogGlobalVars, kind, name, value, 1);
}
Operand *GetSizedGlobal(Operandkind kind, const char *name, intptr_t value, int size)
{
    return GetSizedVar(&cogGlobalVars, kind, name, value, size);
}

Operand *GetOneHub(Operandkind kind, const char *name, intptr_t value)
{
    return GetSizedVar(&hubGlobalVars, kind, name, value, 1);
}

Operand *GetResultReg(int n)
{
    static char rvalname[32];
    if (n < 0 || n >= MAX_TUPLE) {
        ERROR(NULL, "Internal error bad return value");
        return NULL;
    }
    if (!resultreg[n]) {
        sprintf(rvalname, "result%d", n+1);
        resultreg[n] = GetOneGlobal(REG_REG, strdup(rvalname), 0);
    }
    return resultreg[n];
}

Operand *GetArgReg(int n)
{
    static char rvalname[32];
    if (n < 0 || n >= MAX_ARG_REGISTER) {
        ERROR(NULL, "Internal error exceeded arg register limit; too many parameters to function");
        return NULL;
    }
    if (!argreg[n]) {
        sprintf(rvalname, "arg%02d", n+1);
        argreg[n] = GetOneGlobal(REG_ARG, strdup(rvalname), 0);
    }
    return argreg[n];
}

Operand *GetDebugReg(int n) {
    static char rvalname[32];
    if (n < 0 || n >= MAX_LOCAL_REGISTER) {
        ERROR(NULL, "Internal error exceeded arg register limit; too many debug registers needed");
        return NULL;
    }
    if (!debugreg[n]) {
        sprintf(rvalname, "%d-0", n);
        debugreg[n] = GetOneGlobal(REG_HW, strdup(rvalname), 0);
        debugaddr[n] = n;
    }
    return debugreg[n];
}

static Operand *GetGeneralLocalReg(int n)
{
    static char rvalname[32];
    if (n < 0 || n >= MAX_LOCAL_REGISTER) {
        ERROR(NULL, "Internal error exceeded local register limit, possibly due to -O2 optimization needing additional registers or code being too complicated");
        return NULL;
    }
    if (!localreg[n]) {
        // tricky stuff to make sure local100 sorts after local99
        if (n < 99) {
            sprintf(rvalname, "local%02d", n+1);
        } else {
            sprintf(rvalname, "local_%03d", n+1);
        }            
        /* do not use REG_LOCAL here, that will break optimization of recursive
           functions */
        localreg[n] = GetOneGlobal(REG_ARG, strdup(rvalname), 0);
    }
    return localreg[n];
}
static Operand *GetLeafLocalReg(int n)
{
    static char rvalname[32];
    if (n < 0 || n >= MAX_LOCAL_REGISTER) {
        ERROR(NULL, "Internal error exceeded local register limit");
        return NULL;
    }
    if (!leafreg[n]) {
        sprintf(rvalname, "_var%02d", n+1);
        /* do not use REG_LOCAL here, that will break optimization of recursive
           functions */
        leafreg[n] = GetOneGlobal(REG_ARG, strdup(rvalname), 0);
    }
    return leafreg[n];
}

Operand *GetLocalReg(int n, int isLeaf)
{
    return isLeaf ? GetLeafLocalReg(n) : GetGeneralLocalReg(n);
}

Instruction *
FindInstrForOpc(IROpcode kind)
{
    static Instruction **lookup_table;
    extern Instruction *instr; // in lexer.c
    Instruction *r;
    
    if ((unsigned int)kind >= OPC_GENERIC) {
        return NULL;
    }
    if (!lookup_table) {
        // set up the lookup table
        int i = 0;
        lookup_table = (Instruction **)calloc(1, sizeof(Instruction *) * (unsigned int)OPC_GENERIC);
        do {
            if ((unsigned int)instr[i].opc < OPC_GENERIC) {
                lookup_table[(unsigned int)instr[i].opc] = &instr[i];
            }
            i++;
        } while (instr[i].name != 0);
    }
    r = lookup_table[(unsigned int)kind];
    if (!r) {
        ERROR(NULL, "Internal error, unknown instruction");
    }
    return r;
}

IR *NewIR(IROpcode kind)
{
    IR *ir = (IR *)malloc(sizeof(*ir));
    memset(ir, 0, sizeof(*ir));
    ir->opc = kind;
    ir->instr = FindInstrForOpc(kind);
    return ir;
}

IR *DupIR(IR *old)
{
    IR *ir = NewIR(OPC_DUMMY);
    memcpy(ir, old, sizeof(*ir));
    ir->prev = ir->next = NULL;
    return ir;
}

static void
ReplaceJumpTarget(IR *jmpir, Operand *dst)
{
    switch(jmpir->opc) {
    case OPC_REPEAT:
    case OPC_REPEAT_END:
    case OPC_JUMP:
        jmpir->dst = dst;
        break;
    case OPC_DJNZ:
    case OPC_GENERIC_BRCOND:
        jmpir->src = dst;
        break;
    default:
        ERROR(NULL, "Unable to replace jump target");
        break;
    }
}

//
// replace the individual IR "ir" in list "irl" with a list of
// instructions from function f; used for e.g. expanding inline functions
//
void ReplaceIRWithInline(IRList *irl, IR *origir, Function *func)
{
    IR *newir;
    IR *dest = origir->prev;
    IRList *insert = FuncIRL(func);
    IR *ir;

    DeleteIR(irl, origir);
    ir = insert->head;
    while (ir) {
        newir = DupIR(ir);
        newir->flags |= FLAG_INSTR_NEW;
        if (newir->opc == OPC_LABEL) {
        // leave off the asm return name, if it's there
        // FIXME: this is probably an obsolete test now
            if (newir->dst == FuncData(func)->asmretname) {
                /* do nothing */
            } else {
                // make the label kind match the appropriate type
                // of label for this function
                if (InCog(curfunc)) {
                    newir->dst->kind = IMM_COG_LABEL;
                } else {
                    newir->dst->kind = IMM_HUB_LABEL;
                }
                InsertAfterIR(irl, dest, newir);
                dest = newir;
            }
        } else if (newir->opc == OPC_COMMENT && !gl_srccomments) {
            /* leave out comments, they will be out of place */
        } else {
            InsertAfterIR(irl, dest, newir);
            dest = newir;
        }
        ir = ir->next;
    }

    // replace all labels in the original inline code
    for (ir = insert->head; ir; ir = ir->next) {
        if (ir->opc == OPC_LABEL) {
            if (ir->dst == FuncData(func)->asmretname) {
                // do nothing
            } else {
                IR *jmpir;
                ir->dst = NewCodeLabel();
                jmpir = (IR *)ir->aux;
                if (jmpir) {
                    ReplaceJumpTarget(jmpir, ir->dst);
                } else {
                    // we used to print an error here, but in fact it is legal
                    // (albeit unusual) for a label to have nothing explicitly jumping to it,
                    // e.g. the label at the end of a repeat block
                }
            }
        }
    }
}

Operand *NewOperand(enum Operandkind k, const char *name, intptr_t value)
{
    Operand *R = (Operand *)malloc(sizeof(*R));
    memset(R, 0, sizeof(*R));
    R->kind = k;
    R->name = name;
    R->val = value;
    if (k == IMM_COG_LABEL || k == IMM_HUB_LABEL) {
      R->used = 1;
    }
    return R;
}

void
AppendOperand(OperandList **listptr, Operand *op)
{
  OperandList *next = (OperandList *)malloc(sizeof(OperandList));
  OperandList *x;
  next->op = op;
  next->next = NULL;
  for(;;) {
    x = *listptr;
    if (!x) {
      *listptr = next;
      return;
    }
    listptr = &x->next;
  }
}

void
AppendOperandList(OperandList **listptr, OperandList *next)
{
  OperandList *x;
  for(;;) {
    x = *listptr;
    if (!x) {
      *listptr = next;
      return;
    }
    listptr = &x->next;
  }
}

void
AppendOperandUnique(OperandList **listptr, Operand *op)
{
  OperandList *next;
  OperandList *x;
  for(;;) {
    x = *listptr;
    if (!x) {
        next = (OperandList *)malloc(sizeof(OperandList));
        next->op = op;
        next->next = NULL;
        *listptr = next;
        return;
    }
    if (x->op == op) {
        return;
    }
    listptr = &x->next;
  }
}

/*
 * append some IR to the end of a list
 */
void
AppendIR(IRList *irl, IR *ir)
{
    IR *last = irl->tail;
    InsertAfterIR(irl, last, ir);
}

/*
 * insert a new IR element after element orig
 * if orig == NULL then place orig in the
 * beginning of the list
 */

void
InsertAfterIR(IRList *irl, IR *orig, IR *ir)
{
    IR *o_next;

    if (!ir) return;
    
    if (!orig) {
        /* place at the beginning of the list */
        o_next = irl->head;
        irl->head = ir;
        ir->prev = NULL;
    } else {
        /* place it after orig */
        o_next = orig->next;
        orig->next = ir;
        ir->prev = orig;
    }
    /* now skip to the end of the ir list */
    while (ir->next) {
        ir = ir->next;
    }
    /* now fix up o_next to point back to ir */
    if (!o_next) {
        irl->tail = ir;
        ir->next = NULL;
    } else {
        ir->next = o_next;
        o_next->prev = ir;
    }
}

/*
 * Append a whole list
 */
void
AppendIRList(IRList *irl, IRList *sub)
{
  IR *last = irl->tail;
  if (!last) {
    irl->head = sub->head;
    irl->tail = sub->tail;
    return;
  }
  AppendIR(irl, sub->head);
}

/* Delete one IR from a list */
void
DeleteIR(IRList *irl, IR *ir)
{
  IR *prev = ir->prev;
  IR *next = ir->next;

  if (prev) {
    prev->next = next;
  } else {
    irl->head = next;
  }
  if (next) {
    next->prev = prev;
  } else {
    irl->tail = prev;
  }
}

// emit a machine instruction with no operands
static IR *EmitOp0(IRList *irl, IROpcode code)
{
  IR *ir = NewIR(code);
  AppendIR(irl, ir);
  return ir;
}

// emit a machine instruction with one operand
IR *EmitOp1(IRList *irl, IROpcode code, Operand *op)
{
  IR *ir = NewIR(code);
  ir->dst = op;
  AppendIR(irl, ir);
  return ir;
}

// emit a machine instruction with two operands
IR *EmitOp2(IRList *irl, IROpcode code, Operand *d, Operand *s)
{
  IR *ir = NewIR(code);
  ir->dst = d;
  ir->src = s;
  AppendIR(irl, ir);
  return ir;
}

// emit an assembler label
IR *EmitLabel(IRList *irl, Operand *op)
{
    IR *ir = NULL;
    if (irl) {
        ir = EmitOp1(irl, OPC_LABEL, op);
    }
    return ir;
}

void EmitNamedCogLabel(IRList *irl, const char *name)
{
    if (irl) {
        EmitLabel(irl, NewOperand(IMM_COG_LABEL, name, 0));
    }
}

// create a new temporary label name
char *
NewTempLabelName()
{
    return NewTemporaryVariable("L_", NULL);
}

Operand *
NewHubLabel()
{
  Operand *label;
  label = NewOperand(IMM_HUB_LABEL, NewTempLabelName(), 0);
  label->used = 0;
  return label;
}

//
// NewCodeLabel() returns a new temporary label
//
Operand *
NewCodeLabel()
{
  Operand *label;
  const char *id = NewTempLabelName();
  if (curfunc && !InCog(curfunc)) {
      label = NewOperand(IMM_HUB_LABEL, id, 0);
  } else {
      label = NewOperand(IMM_COG_LABEL, id, 0);
  }
  label->used = 0;
  return label;
}

// new code label should be persistent
// and unique to the function
Operand *
NewNamedCodeLabel(const char *id)
{
  Operand *label;
  char temp[1024];
  if (curfunc) {
      snprintf(temp, sizeof(temp)-1, "Label_%s_%s", curfunc->name, cleanname(id));
      id = temp;
  }
  id = strdup(id);
  if (curfunc && !InCog(curfunc)) {
      label = NewOperand(IMM_HUB_LABEL, id, 0);
  } else {
      label = NewOperand(IMM_COG_LABEL, id, 0);
  }
  label->used = 1;
  return label;
}

Operand *
NewDataLabel()
{
  Operand *label;
  label = NewOperand(IMM_HUB_LABEL, NewTempLabelName(), 0);
  label->used = 0;
  return label;
}

int EmitLong(IRList *irl, int val)
{
  Operand *op;

  if (irl) {
      op = NewOperand(IMM_INT, "", val);
      EmitOp1(irl, OPC_LONG, op);
  }
  return 4;
}

#define COG_RESERVE 0
#define HUB_RESERVE 1

void EmitReserve(IRList *irl, int val, int isHub)
{
  Operand *op;

  if (irl) {
      op = NewOperand(IMM_INT, "", val);
      if (isHub) {
          EmitOp1(irl, OPC_RESERVEH, op);
      } else {
          EmitOp1(irl, OPC_RESERVE, op);
      }
  }
}

static int EmitLongPtr(IRList *irl, Operand *op)
{
    if (irl) {
        EmitOp1(irl, OPC_LONG, op);
    }
    return 4;
}

static int EmitStringNoTrailingZero(IRList *irl, AST *ast)
{
  Operand *op;
  switch (ast->kind) {
  case AST_STRING:
      if (irl) {
          op = NewOperand(IMM_STRING, ast->d.string, 0);
          EmitOp1(irl, OPC_STRING, op);
      }
      return strlen(ast->d.string);
  case AST_INTEGER:
      if (irl) {
          op = NewOperand(IMM_INT, "", (int)ast->d.ival);
          EmitOp1(irl, OPC_BYTE, op);
      }
      return 1;
  default:
      if (IsConstExpr(ast)) {
          if (irl) {
              op = NewOperand(IMM_INT, "", EvalConstExpr(ast));
              EmitOp1(irl, OPC_BYTE, op);
          }
      } else {
          ERROR(ast, "Unable to emit string");
      }
      return 1;
  }
}

int EmitString(IRList *irl, AST *ast)
{
    int count = 0;
    while (ast && ast->kind == AST_EXPRLIST) {
        count += EmitStringNoTrailingZero(irl, ast->left);
        ast = ast->right;
    }
    if (ast) {
        count += EmitStringNoTrailingZero(irl, ast);
    }
    // add a trailing 0
    if (irl) {
        EmitOp1(irl, OPC_BYTE, NewImmediate(0));
        count++;
    }
    return count;
}

IR *EmitJump(IRList *irl, IRCond cond, Operand *label)
{
  IR *ir;

  if (cond == COND_FALSE) return NULL;
  ir = NewIR(OPC_JUMP);
  ir->dst = label;
  ir->cond = cond;
  AppendIR(irl, ir);
  return ir;
}

Operand *
NewPcRelative(int32_t val)
{
    return NewOperand(IMM_PCRELATIVE, "", (int32_t)val);
}

Operand *
NewImmediate(int32_t val)
{
  char temp[1024];
  if ( gl_p2 || (val >= 0 && val < 512) ) {
    return NewOperand(IMM_INT, "", (int32_t)val);
  }
  sprintf(temp, "imm_%u_", (unsigned)val);
  return GetOneGlobal(IMM_INT, strdup(temp), (int32_t)val);
}

Operand *
NewImmediatePtr(const char *name, Operand *val)
{
    char temp[1024];
    if (!name) {
        sprintf(temp, "ptr_%s_", cleanname(val->name));
        name = strdup(temp);
    }
    if (val->kind == IMM_COG_LABEL) {
        return GetOneGlobal(REG_COGPTR, name, (intptr_t)val);
    } else {
        return GetOneGlobal(REG_HUBPTR, name, (intptr_t)val);
    }
}

static Operand *
GetFunctionTempRegister(Function *f, int n)
{
  Module *P = f->module;
  char buf[1024];
  if (IS_LEAF(f)) {
      // leaf functions can share a set of temporary registers
      snprintf(buf, sizeof(buf)-1, "_tmp%03d_", n);
  } else if (IsTopLevel(P)) {
      snprintf(buf, sizeof(buf)-1, "%s_tmp%03d_", cleanname(f->name), n);
  } else {
      snprintf(buf, sizeof(buf)-1, "%s_%s_tmp%03d_", P->classname, cleanname(f->name), n);
  }
  return GetOneGlobal(REG_TEMP, strdup(buf), 0);
}

Operand *
NewFunctionTempRegister()
{
    Function *f = curfunc;
    IRFuncData *fdata = FuncData(f);

    fdata->curtempreg++;
    if (fdata->curtempreg > fdata->maxtempreg) {
        fdata->maxtempreg = fdata->curtempreg;
    }
    return GetFunctionTempRegister(f, fdata->curtempreg);
}

void
FreeTempRegisters(IRList *irl, int starttempreg)
{
    /* release temporaries we used */
    FuncData(curfunc)->curtempreg = starttempreg;
}


static Operand *
SizedHubMemRef(int size, Operand *addr, int offset)
{
    Operand *temp;

    // NOTE: a size of 0 is perfectly OK (it can happen for a struct
    // which only has function members) but should never be dereferenced
    temp = NewOperand(HUBMEM_REF, (char *)addr, offset);
    temp->size = size;
    return temp;
}

//
// FIXME: should this use TypeSize()? It seems to have slightly different
// requirements
//
static Operand *
TypedHubMemRef(AST *type, Operand *addr, int offset)
{
    int size;
    
    while (type && (type->kind == AST_ARRAYTYPE || type->kind == AST_MODIFIER_CONST || type->kind == AST_MODIFIER_VOLATILE)) {
        type = type->left;
    }
    if (!type) {
        size = 4;
    } else if (type->kind == AST_TUPLE_TYPE) {
        size = AstListLen(type) * 4;
    } else if (type->kind == AST_PTRTYPE || type->kind == AST_REFTYPE || type->kind == AST_COPYREFTYPE ) {
        size = 4;
    } else if (type->kind == AST_OBJECT) {
        Module *Q = (Module *)type->d.ptr;
        size = Q->varsize;
    } else if (type->kind == AST_FUNCTYPE) {
        size = 4;
    } else {
        size = EvalConstExpr(type->left);
    }
    return SizedHubMemRef(size, addr, offset);
}

Operand *
CogMemRef(Operand *addr, int offset)
{
    Operand *ref = NewOperand(COGMEM_REF, (char *)addr, offset);
    ref->size = 4;
    return ref;
}

static Operand *
FrameRef(int offset, int typesize)
{
    ValidateFrameptr();
    if (COG_DATA) {
        return CogMemRef(frameptr, offset / LONG_SIZE);
    }
    return SizedHubMemRef(typesize, frameptr, offset);
}
static Operand *
StackRef(int offset)
{
    ValidateStackptr();
    if (COG_DATA) {
        return CogMemRef(stackptr, offset / LONG_SIZE);
    }
    return SizedHubMemRef(LONG_SIZE, stackptr, offset);
}

static Operand *
ValidateDatBase(Module *P)
{
    AsmModData *PD = ModData(P);
    if (!PD->datbase) {
        PD->datlabel = NewOperand(IMM_HUB_LABEL, IdentifierModuleName(P, "dat_"), 0);
        PD->datbase = NewImmediatePtr(NULL, PD->datlabel);
    }
    return PD->datbase;
}

Operand *
LabelRef(IRList *irl, Symbol *sym)
{
    Operand *temp;
    Label *lab = (Label *)sym->val;
    Module *P = sym->module ? (Module *)sym->module : current;
    Operand *datbase = ValidateDatBase(P);

#if 0    
    if (! (lab->flags & (LABEL_IN_HUB|LABEL_USED_IN_SPIN) ) ) {
        WARNING(NULL, "Internal error, unexpected COG label");
    }
#endif    
    temp = TypedHubMemRef(lab->type, datbase, (int)lab->hubval);
    return temp;
}

static Operand *
CompileSymbolForFunc(IRList *irl, Symbol *sym, Function *func, AST *ast)
{
  int stype;
  int size;
  Module *P = func->module;
  if (sym) {
      AST *exprtype;
      AST *expr;
      stype = sym->kind;
      switch (stype) {
      case SYM_VARIABLE:
          if (sym->flags & SYMF_GLOBAL) {
              int off = sym->offset;
              Operand *addr;
              Operand *ref;
              if (off == -1) {
                  // this is a special internal COG variable
                  if (!sendreg) {
                      sendreg = GetOneGlobal(REG_REG, "__sendreg", P2_HUB_BASE);
                  }
                  return sendreg;
              } else if (off == -2) {
                  // this is a special internal COG variable
                  if (!recvreg) {
                      recvreg = GetOneGlobal(REG_REG, "__recvreg", P2_HUB_BASE);
                  }
                  return recvreg;
              } else if (off == -3) {
                  // this is a special internal COG variable
                  if (!lockreg) {
                      lockreg = GetOneGlobal(REG_REG, "__lockreg", 0);
                  }
                  return lockreg;
              }
              addr = NewImmediate(off);
              ref = NewOperand(HUBMEM_REF, (char *)addr, 0);
              ref->size = LONG_SIZE;
              return ref;
          }
          ValidateObjbase();
          exprtype = (AST *)sym->val;
          if (COG_DATA) {
              // COG memory
              size = TypeSize(exprtype);
              return GetSizedGlobal(REG_REG, IdentifierModuleName(P, sym->our_name), 0, size);
          } else {
              // HUB memory
              return TypedHubMemRef(exprtype, objbase, (int)sym->offset);
          }
      case SYM_FUNCTION:
      {
          Function *calledf = (Function *)sym->val;
          return FuncData(calledf)->asmname;
      }
      case SYM_HWREG:
      {
          HwReg *hw = (HwReg *)sym->val;
          return GetOneGlobal(REG_HW, hw->name, 0);
      }
      case  SYM_CLOSURE:
      {
          return FrameRef(0, LONG_SIZE);
      }
      default:
          if (sym->kind == SYM_RESERVED && !strcmp(sym->our_name, "result")) {
              /* do nothing, this is OK */
              stype = SYM_RESULT;
          } else {
              ERROR(ast, "Undefined symbol %s", sym->user_name);
              return EmptyOperand();
          }
          /* fall through */
      case SYM_LOCALVAR:
      case SYM_PARAMETER:
      case SYM_RESULT:
          if (stype == SYM_RESULT) {
              size = LONG_SIZE;
          } else {
              size = TypeSize((AST *)sym->val);
          }
          if (PutVarOnStack(func, sym, size)) {
              int offset = sym_offset(func, sym);
              if (offset >= 0) {
                  AST *typ = (AST *)sym->val;
                  if (IsArrayType(typ)) {
                      size = TypeSize(BaseType(typ));
                  }
                  return FrameRef(offset, size);
              }
          }
          return GetSizedGlobal(REG_LOCAL, IdentifierLocalName(func, sym->our_name), 0, size);
      case SYM_TEMPVAR:
          size = TypeSize((AST *)sym->val);
          return GetSizedGlobal(REG_TEMP, IdentifierLocalName(func, sym->our_name), 0, size);
      case SYM_LABEL:
          return LabelRef(irl, sym);
      case SYM_ALIAS:
          expr = (AST *)sym->val;
          return CompileExpression(irl, expr, NULL);
      }
  }
  return NULL;
}

static Operand *
CompileIdentifierForFunc(IRList *irl, AST *expr, Function *func)
{
  Symbol *sym;
  const char *name;
  AST *resultexpr;
  
  if (expr->kind == AST_RESULT) {
      resultexpr = func->resultexpr;
      if (resultexpr && resultexpr->kind == AST_DECLARE_VAR) {
          resultexpr = resultexpr->right;
      }
      if (resultexpr && resultexpr->kind == AST_IDENTIFIER) {
          name = resultexpr->d.string;
      } else {
          name = "result";
      }
  } else if (expr->kind == AST_VARARGS || expr->kind == AST_VA_START) {
      name = "__varargs";
  } else {
      name = VarName(expr);
      if (!name) {
          ERROR(expr, "Internal error, unexpected expression type for identifier");
          return EmptyOperand();
      }
  }
  sym = LookupSymbol(name);
  if (sym) {
      return CompileSymbolForFunc(irl, sym, func, expr);
  } else {
      ERROR_UNKNOWN_SYMBOL(expr);
  }
  return GetOneGlobal(REG_LOCAL, IdentifierLocalName(func, name), 0);
}

static Operand *
GetSystemFunction(const char *name)
{
    Symbol *sym;
    Function *calledf;
    sym = FindSymbol(&systemModule->objsyms, name);
    if (!sym) {
        ERROR(NULL, "Internal error could not find %s", name);
        return mulfunc;
    }
    if (sym->kind != SYM_FUNCTION) {
        ERROR(NULL, "Internal error, %s is not a function", name);
        return mulfunc;
    }
    calledf = (Function *)sym->val;
    if (!calledf || !FuncData(calledf)) {
        ERROR(NULL, "Internal error, %s not set up", name);
        return mulfunc;
    }
    return FuncData(calledf)->asmname;
}

const char *
VarName(AST *ast)
{
    if (ast->kind == AST_DECLARE_VAR) {
        // left is type, right is name
        ast = ast->right;
    }
    if (ast->kind == AST_ARRAYDECL) {
        ast = ast->left;
    }
    if (ast->kind == AST_LOCAL_IDENTIFIER) {
        ast = ast->left;
    }
    if (ast->kind != AST_IDENTIFIER) {
        ERROR(ast, "Internal error, expected identifier");
        return "<null>";
    }
    return ast->d.string;
}

//
// utility function: get a pointer to where we should put the n'th argument
// for function f when we call it
//
static Operand *GetFunctionParameterForCall(IRList *irl, Function *func, AST *functype, int n)
{
    if (IS_FAST_CALL(func)) {
        // check for varargs functions
        int maxparam;
        if (func) {
            maxparam = func->numparams;
        } else {
            maxparam = FuncNumParams(functype);
        }
        if (maxparam >= 0 || n < -maxparam) {
            return GetArgReg(n);
        }
        return NULL; // signals we have to push the parameter
    } else {
        AST *astlist;
        AST *ast;
        int numresults;
        int offset;
        
        if (!IsFunctionType(functype)) {
            ERROR(functype, "Expected function type");
            return EmptyOperand();
        }
        numresults = FuncNumResults(functype);
        astlist = functype->right;
        offset = 0;
        while (astlist != NULL) {
            const char *name;
            ast = astlist->left;
            astlist = astlist->right;
            if (n == 0) {
                name = VarName(ast);
                //size = TypeSize(ExprType(ast)); // FIXME we will need this
                if (func) {
                    Symbol *sym;
                    sym = FindSymbol(&func->localsyms, name);
                    if (!sym) {
                        ERROR(NULL, "Internal error, symbol %s not found", name);
                        return EmptyOperand();
                    }
                    if (sym->offset != offset) {
                        ERROR(NULL, "Internal error, offset %d is not sym offset %d", offset, sym->offset);
                    }
                }
                // we have to leave space for:
                // return address
                // old frame pointer
                // result value
                return StackRef((2 + numresults)*LONG_SIZE + offset);
            }
            --n;
            offset += 4;
        }
        ERROR(NULL, "Too many parameters to function %s", func ? func->name : "");
        return GetOneGlobal(REG_ARG, "dummyArg", 0);
    }
}

static void EmitPush(IRList *irl, Operand *src)
{
    IR *ir;
    ValidateStackptr();
    ir = EmitMove(irl, stacktop, src);
    if (gl_p2) {
        ir->srceffect = OPEFFECT_POSTINC;
    } else if (COG_DATA) {
        EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(1));
    } else {
        EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(4));
    }
}
static void EmitPop(IRList *irl, Operand *src)
{
    ValidateStackptr();
    IR *ir;
    if (!gl_p2) {
        if (COG_DATA) {
            EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(1));
        } else {
            EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(4));
        }
    }
    ir = EmitMove(irl, src, stacktop);
    if (gl_p2) {
        ir->srceffect = OPEFFECT_PREDEC;
    }
}

//
// the function Prolog and Epilog are always output, and do things
// like stack management and variable setup
//

static void EmitFunctionProlog(IRList *irl, Function *func)
{
    int n = 0;
    int i, size;
    AST *astlist;
    AST *ast;
    AST *type;
    Operand *src;
    Operand *dst;
    Operand *basedst;

    if (FuncData(func)->asmentername) {
        EmitLabel(irl, FuncData(func)->asmentername);
    }
    if (!IS_STACK_CALL(func)) {
        //
        // move parameters into local registers
        //
        astlist = func->params;
        while (astlist != NULL) {
            ast = astlist->left;
            astlist = astlist->right;
            type = NULL;
            size = 1;
            if (ast->kind == AST_DECLARE_VAR) {
                type = ast->left;
                ast = ast->right;
            }
            if (ast && ast->kind == AST_ARRAYDECL) {
                size = EvalConstExpr(ast->right);
            }
            // how many longs will we need?
            if (type) {
                int tsize = size * TypeSize(type);
                tsize = (tsize + LONG_SIZE - 1) & ~(LONG_SIZE-1);
                size = tsize / LONG_SIZE;
            }
            if (ast && ast->kind != AST_INTTYPE && ast->kind != AST_UNSIGNEDTYPE) {
                basedst = CompileIdentifierForFunc(irl, ast, func);
            } else {
                basedst = NULL;
                size = 0;
            }
            for (i = 0; i < size; i++) {
                src = GetFunctionParameterForCall(irl, func, func->overalltype, n++);
                if (!src) {
                    int maxArg = func->numparams;
                    if (maxArg < 0) {
                        src = GetArgReg(-maxArg);
                    } else {
                        ERROR(NULL, "internal error, bad varargs parsing");
                        src = stackptr;
                    }
                }
                if (size > 1) {
                    dst = ApplyArrayIndex(irl, basedst, NewImmediate(i), LONG_SIZE);
                } else {
                    dst = basedst;
                }
                EmitMove(irl, dst, src);
            }
        }
    }
    if (func->closure) {
        // need to copy the stack frame into the heap
        Operand *framesize = NewImmediate(FuncLocalSize(func));
        Operand *tmp;
        if (!allocfunc) {
            allocfunc = GetSystemFunction("_gc_alloc_managed");
        }
        if (!copyfunc) {
            copyfunc = GetSystemFunction("bytemove");
        }
        ValidateStackptr();
        tmp = GetResultReg(5); // we know this won't be used in the system functions
        // tmp = _gc_alloc_managed(framesize)
        EmitMove(irl, GetArgReg(0), framesize);
        EmitOp1(irl, OPC_CALL, allocfunc);
        EmitMove(irl, tmp, GetResultReg(0));
        // bytemove(tmp, frameptr, framesize)
        EmitMove(irl, GetArgReg(0), tmp);
        EmitMove(irl, GetArgReg(1), frameptr);
        EmitMove(irl, GetArgReg(2), framesize);
        EmitOp1(irl, OPC_CALL, copyfunc);
        // frameptr = tmp
        EmitMove(irl, frameptr, tmp);

        // adjust stack pointer back
        EmitOp2(irl, OPC_SUB, stackptr, framesize);
    }
}

//
// WARNING: most of the stuff placed in the epilog will be skipped over
// by any return statement!!
//
static void EmitFunctionEpilog(IRList *irl, Function *func)
{
    if (FuncData(func)->asmreturnlabel != FuncData(func)->asmretname) {
        EmitLabel(irl, FuncData(func)->asmreturnlabel);
    }
    FreeTempRegisters(irl, 0);
}

/*
 * rename one operand locally
 */
static void
RenameOneReg(IR *ir, Operand *old, Operand *update)
{
    while (ir) {
        if (ir->dst) {
            if (ir->dst == old) {
                ir->dst = update;
            } else if (ir->dst && ir->dst->kind == COGMEM_REF && ir->dst->name == (char *)old) {
                ir->dst->name = (char *)update;
            } else if (old->kind == REG_SUBREG || ir->dst->kind == REG_SUBREG) {
                if (old->kind == ir->dst->kind && old->name == ir->dst->name && old->val == ir->dst->val) {
                    ir->dst = update;
                }
            } else if (IsCogMem(ir->dst) && !strcmp(ir->dst->name, old->name)) {
                ir->dst->name = update->name;
            }
        }
        if (ir->src) {
            if (ir->src == old) {
                ir->src = update;
            } else if (ir->src->kind == COGMEM_REF && ir->src->name == (char *)old) {
                ir->src->name = (char *)update;
            } else if (old->kind == REG_SUBREG || ir->src->kind == REG_SUBREG) {
                if (old->kind == ir->src->kind && old->name == ir->src->name && old->val == ir->src->val) {
                    ir->src = update;
                }
            } else if (IsCogMem(ir->src) && !strcmp(ir->src->name, old->name)) {
                ir->src->name = update->name;
            }
        }
        ir = ir->next;
    }
    // now we can mark "old" as unused in the global variable table
    if (old->used) {
        --old->used;
    }
}

static int
RenameSubregs(IRList *irl, Operand *base, int numlocals, int isLeaf)
{
    int num = (base->size + LONG_SIZE) / LONG_SIZE;
    IR *ir;
    Operand *replace;
    
    replace = GetLocalReg(numlocals, isLeaf);
    RenameOneReg(irl->head, base, replace);
    for (ir = irl->head; ir; ir = ir->next) {
        if (ir->dst && ir->dst->kind == REG_SUBREG && base == (Operand *)ir->dst->name) {
            replace = GetLocalReg(numlocals + ir->dst->val, isLeaf);
            RenameOneReg(ir, ir->dst, replace);
            if (base->used > 0) {
                base->used--;
            }
        }
        if (ir->src && ir->src->kind == REG_SUBREG && base == (Operand *)ir->src->name) {
            replace = GetLocalReg(numlocals + ir->src->val, isLeaf);
            RenameOneReg(ir, ir->src, replace);
            if (base->used > 0) {
                base->used--;
            }
        }
    }
        
    return num;
}

/*
 * rename locals so that we can re-use registers
 * returns a count of how many unique local registers we needed
 */

static int
RenameLocalRegs(IRList *irl, int isLeaf)
{
    IR *ir;
    Operand *replace = NULL;
    int numlocals = 0;

    /* first, look for any subregisters; if found, rename those
     * in a way that guarantees the arrays stay in order
     */
    for (ir = irl->head; ir; ir = ir->next) {
        if (ir->dst && ir->dst->kind == REG_SUBREG && IsLocal(ir->dst)) {
            numlocals += RenameSubregs(irl, (Operand *)ir->dst->name, numlocals, isLeaf);
        }
        if (ir->src && ir->src->kind == REG_SUBREG && IsLocal(ir->src)) {
            numlocals += RenameSubregs(irl, (Operand *)ir->src->name, numlocals, isLeaf);
        }
    }
    for (ir = irl->head; ir; ir = ir->next) {
        if (IsDummy(ir)) continue;
        if (ir->dst && IsLocal(ir->dst)) {
            replace = GetLocalReg(numlocals++, isLeaf);
            RenameOneReg(ir, ir->dst, replace);
        }
        if (ir->src && IsLocal(ir->src)) {
            replace = GetLocalReg(numlocals++, isLeaf);
            RenameOneReg(ir, ir->src, replace);
        }
    }
    return numlocals;
}

static bool
NeedToSaveLocals(Function *func)
{
    if (IS_LEAF(func)) {
        return false;
    }
    if (func->is_recursive) {
        return true;
    }
    if (gl_output == OUTPUT_COGSPIN) {
        return false;
    }
    if (InCog(func)) {
        return false;
    }
    return true;
}

// see if a function needs a frame pointer
#define FRAME_NO 0
#define FRAME_MAYBE 1
#define FRAME_YES 2

static int
NeedFramePointer(Function *func)
{
    if (ANY_VARS_ON_STACK(func)) {
        return FRAME_YES;
    }
    if (func->uses_alloca) {
        return FRAME_YES;
    }
    if (func->is_recursive) {
        return FRAME_YES;
    }
    if (NeedToSaveLocals(func)) {
        return FRAME_MAYBE;
    }
    return FRAME_NO;
}

static void
MarkUsedOneReg(Operand *reg)
{
    if (reg && reg->kind == REG_SUBREG) {
        reg = (Operand *)reg->name;
    }
    if (reg) {
        reg->used = 1;
    }
}
static void
MarkUsedSubregs(IRList *irl)
{
    IR *ir;
    for (ir = irl->head; ir; ir = ir->next) {
        MarkUsedOneReg(ir->dst);
        MarkUsedOneReg(ir->src);
    }
}

//
// the header/footer are code that should only be emitted for
// non-inline function invocations, at the beginning and
// end of the function; they contain the labels and
// ret instruction, for example
//
static void EmitFunctionHeader(IRList *irl, Function *func)
{
    int i, n;
    int needFrame = 0;
    
    // earlier we put the appropriate comments into func->irheader
    // copy them out now
    AppendIRList(irl, &FuncData(func)->irheader);

    if (gl_output == OUTPUT_COGSPIN && FuncData(func)->asmaltname) {
        // insert a dummy function header that just changes the return address
        EmitLabel(irl, FuncData(func)->asmaltname);
        if (!gl_p2) {
            EmitOp2(irl, OPC_MOVS, FuncData(func)->asmretname, linkreg);
        }
    }

    // now the function label
    EmitLabel(irl, FuncData(func)->asmname);

    // push return address, if we are in cog mode
    if (func->is_recursive && InCog(func) && !gl_p2) {
        EmitPush(irl, FuncData(func)->asmretname);
    }
    
    // if SEND is set in function, save it
    if (func->sets_send) {
        EmitPush(irl, sendreg);
    }
    if (func->sets_recv) {
        EmitPush(irl, recvreg);
    }
    //
    // if recursive function, push all local registers
    // we also have to push any local temporary registers that
    // are used in the body
    // FIXME: can probably optimize this to skip temporaries that
    // are used only after the first call
    //

    n = 0;
    needFrame = NeedFramePointer(func);
    if (needFrame == FRAME_YES || needFrame == FRAME_MAYBE) {
        if (NeedToSaveLocals(func)) {
            n = RenameLocalRegs(FuncIRL(func), IS_LEAF(func));
        } else {
            MarkUsedSubregs(FuncIRL(func));
        }
    } else {
        MarkUsedSubregs(FuncIRL(func));
    }
    if (needFrame == FRAME_MAYBE) {
        needFrame = (n > 0) ? FRAME_YES : FRAME_NO;
    }
    FuncData(func)->numsavedregs = n;
    if (needFrame) {
        ValidateFrameptr();
        if (HUB_CODE) {
            ValidatePushregs();
            EmitMove(irl, count_, NewImmediate(n));
            EmitOp1(irl, OPC_CALL, pushregs_);
        } else {
            for (i = 0; i < n; i++) {
                EmitPush(irl, GetLocalReg(i, 0));
            }
            EmitPush(irl, frameptr);
            EmitMove(irl, frameptr, stackptr);
        }
    } else if (IS_LEAF(func)) {
        RenameLocalRegs(FuncIRL(func), 1);
    }
    if (ANY_VARS_ON_STACK(func)) {
        int localsize;
        //
        // adjust stack as necessary
        //
        localsize = FuncLocalSize(func);
        if (COG_DATA) {
            EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(localsize / LONG_SIZE));
        } else {
            EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(localsize));
        }
    }

}

static void EmitFunctionFooter(IRList *irl, Function *func)
{
    int i, n;
    int needFrame;

    needFrame = NeedFramePointer(func);
    n = FuncData(func)->numsavedregs;
    if (needFrame == FRAME_MAYBE) {
        needFrame = (n > 0) ? FRAME_YES : FRAME_NO;
    }
    
    if (needFrame) {
        ValidateFrameptr();
        if (!func->closure) {
            EmitMove(irl, stackptr, frameptr);
        }
        if (HUB_CODE) {
            // pop registers off stack
            ValidatePushregs();
            EmitOp1(irl, OPC_CALL, popregs_);
        } else {
            EmitPop(irl, frameptr);
            for (i = n-1; i >= 0; --i) {
                EmitPop(irl, GetLocalReg(i, 0));
            }
        }
    }
    // if SEND/RECV is set in function, restore it
    if (func->sets_recv) {
        EmitPop(irl, recvreg);
    }
    if (func->sets_send) {
        EmitPop(irl, sendreg);
    }
    if (func->is_recursive && InCog(func) && !gl_p2) {
        IR *ir;
        EmitPop(irl, FuncData(func)->asmretname);
        // WARNING: 
        // we need at least 1 instruction between the pop
        // and the actual return
        ir = EmitOp1(irl, OPC_NOP, NULL);
        ir->flags |= FLAG_KEEP_INSTR;
    }
    EmitLabel(irl, FuncData(func)->asmretname);
    EmitOp0(irl, OPC_RET);
}

Operand *
CompileIdentifier(IRList *irl, AST *expr)
{
    Symbol *sym = NULL;

    if (expr->kind == AST_IDENTIFIER) {
        sym = LookupSymbol(expr->d.string);
        if (sym && (sym->kind == SYM_CONSTANT || sym->kind == SYM_FLOAT_CONSTANT)) {
            AST *symexpr = (AST *)sym->val;
            int val = EvalConstExpr(symexpr);
            if (val >= 0 && val < 512) {
                // it would be nice to use sym->name as a symbolic
                // name for the constant, but this causes problems
                // with visibility in subobjects
                return NewOperand(IMM_INT, "" /*sym->name*/, val);
            } else {
                return NewImmediate(val);
            }
        }
    }
    return CompileIdentifierForFunc(irl, expr, curfunc);
}

Operand *
CompileHWReg(IRList *irl, AST *expr)
{
  HwReg *hw = (HwReg *)expr->d.ptr;
  return GetOneGlobal(REG_HW, hw->name, 0);
}

static int isPowerOf2(unsigned x)
{
    return (x & (x-1)) == 0;
}

//
// Decompose val into a sequence of a shift, add/sub
// returns 0 if failure, 1 if success
// sets shifts[0] to final shift
// shifts[1] to +1 for add, -1 for sub, 0 if done
// shifts[2] to initial shift
//
static int DecomposeBits(unsigned val, int *shifts)
{
    int shift = 0;
    
    while (val != 0) {
        if (val & 1) {
            break;
        }
        shift++;
        val = val >> 1;
    }
    shifts[0] = shift;
    if (val == 1) {
        // we had a power of 2
        shifts[1] = 0;
        return 1;
    }
    // OK, can the new val itself be decomposed?
    if (isPowerOf2(val-1)) {
        shifts[1] = +1;
        return DecomposeBits(val-1, &shifts[2]);
    } else if (isPowerOf2(val+1)) {
        shifts[1] = -1;
        return DecomposeBits(val+1, &shifts[2]);
    } else {
        return 0;
    }
}

static int g_NeedMulHi = 0;

static Operand *
doCompileMul(IRList *irl, Operand *lhs, Operand *rhs, int gethi, Operand *dest)
{    
    Operand *temp = NewFunctionTempRegister();
    int isUnsigned = (gethi & 2);
    g_NeedMulHi |= (gethi & 1);
    
    // if lhs is constant, swap left and right
    if (lhs->kind == IMM_INT) {
        Operand *swap = lhs;
        lhs = rhs;
        rhs = swap;
    }
    // check for multiply by constants
    if (rhs->kind == IMM_INT && rhs->val >= 0 && gethi == 0) {
        int shifts[4];
        int32_t val = rhs->val;

        if (val == 0) {
            EmitMove(irl, temp, rhs); // rhs == 0
            return temp;
        }
        if (val == 1) {
            EmitMove(irl, temp, lhs);
            return temp;
        }
        lhs = Dereference(irl, lhs);
        // see if we can emit a sequence of shift and add/sub
        if (DecomposeBits(val, shifts)) {
            EmitMove(irl, temp, lhs);
            if (shifts[1] == 0) {
                EmitOp2(irl, OPC_SHL, temp, NewImmediate(shifts[0]));
                return temp;
            } else {
                EmitOp2(irl, OPC_SHL, temp, NewImmediate(shifts[2]));
            }
            if (shifts[1] > 0) {              
                EmitOp2(irl, OPC_ADD, temp, lhs);
            } else {
                EmitOp2(irl, OPC_SUB, temp, lhs);
            }
            EmitOp2(irl, OPC_SHL, temp, NewImmediate(shifts[0]));
            return temp;
        }
    }
    if (rhs->kind == IMM_INT && rhs->val == -1 && gethi == 0) {
        lhs = Dereference(irl, lhs);
        EmitOp2(irl, OPC_NEG, temp, lhs);
        return temp;
    }
    if (gl_p2) {
        if (gethi == 0) {
            lhs = Dereference(irl, lhs);
            rhs = Dereference(irl, rhs);
            EmitOp2(irl, OPC_QMUL, lhs, rhs);
            EmitOp1(irl, OPC_GETQX, temp);
            return temp;
        }
        if (isUnsigned && gethi) {
            lhs = Dereference(irl, lhs);
            rhs = Dereference(irl, rhs);
            EmitOp2(irl, OPC_QMUL, lhs, rhs);
            EmitOp1(irl, OPC_GETQY, temp);
            return temp;
        }
    }
    
    if (!mulfunc) {
        mulfunc = NewOperand(IMM_COG_LABEL, "multiply_", 0);
        unsmulfunc = NewOperand(IMM_COG_LABEL, "unsmultiply_", 0);
        muldiva = GetOneGlobal(REG_REG, "muldiva_", 0);
        muldivb = GetOneGlobal(REG_REG, "muldivb_", 0);
    }
    EmitMove(irl, muldiva, lhs);
    EmitMove(irl, muldivb, rhs);
    if (isUnsigned || !gethi) {
        EmitOp1(irl, OPC_CALL, unsmulfunc);
    } else {
        EmitOp1(irl, OPC_CALL, mulfunc);
    }
    if (gethi) {
        EmitMove(irl, temp, muldivb);
    } else {
        EmitMove(irl, temp, muldiva);
    }
    return temp;
}

static Operand *
CompileMul(IRList *irl, AST *expr, int gethi, Operand *dest)
{
    Operand *lhs = CompileExpression(irl, expr->left, NULL);
    Operand *rhs = CompileExpression(irl, expr->right, NULL);
    if (gl_p2 && gethi == 0) {
        AST *lefttype = ExprType(expr->left);
        if (IsIntType(lefttype) && TypeSize(lefttype) == 2) {
            AST *righttype = ExprType(expr->right);
            if (righttype == lefttype) {
                Operand *temp = NewFunctionTempRegister();
                EmitMove(irl, temp, lhs);
                rhs = Dereference(irl, rhs);
                if (IsUnsignedType(lefttype)) {
                    EmitOp2(irl, OPC_MULU, temp, rhs);
                } else {
                    EmitOp2(irl, OPC_MULS, temp, rhs);
                }
                return temp;
            }
        }
    }
    return doCompileMul(irl, lhs, rhs, gethi, dest);
}

#define FAST_IMMEDIATE_DIVIDES

// getmod is: 0 for divide, 1 for remainder, 2 for unsigned divide, 3 for unsigned remainder
//  special case: 4 is for unsigned 64 bit by 32 bit division
static Operand *
CompileDiv(IRList *irl, AST *expr, int getmod, Operand *dest)
{
  Operand *lhs = CompileExpression(irl, expr->left, NULL);
  Operand *rhs = CompileExpression(irl, expr->right, NULL);
  Operand *temp = dest ? dest : NewFunctionTempRegister();
  int isSigned = (getmod & 2) == 0;
  int isfrac64 = getmod >= 4;
  
  getmod &= 1;
  
#ifdef FAST_IMMEDIATE_DIVIDES
  if (rhs->kind == IMM_INT  && !isfrac64 && ((rhs->val > 0 && isPowerOf2(rhs->val)) || (rhs->val < 0 && isPowerOf2(0-rhs->val)))) {
      IR *ir;
      int aval = abs(rhs->val);
      
      if (rhs->val == 1 || (rhs->val == -1 && getmod)) {
          EmitMove(irl, temp, lhs);
          return temp;
      } else if (rhs->val == -1) {
          EmitMove(irl, temp, lhs);
          EmitOp2(irl, OPC_NEG, temp, temp);
        return temp;
      }

      lhs = Dereference(irl, lhs);
      if (isSigned) {
          ir = EmitOp2(irl, OPC_ABS, temp, lhs);
          ir->flags |= FLAG_WC; // carry will have the original sign bit
      } else {
          EmitMove(irl, temp, lhs);
      }
      if (getmod) {
          EmitOp2(irl, OPC_AND, temp, NewImmediate(aval-1));
      } else {
          EmitOp2(irl, OPC_SHR, temp, NewImmediate(__builtin_ctz(aval)));
      }
      if (isSigned) {
          bool negresult = !getmod && (rhs->val < 0); // Negate result if divisor is negative and we're not getting the remainder
          ir = EmitOp2(irl, negresult ? OPC_NEGNC : OPC_NEGC, temp, temp);
      }
      return temp;
  }
#endif
  
  if (isfrac64) {
      if (gl_p2) {
          Operand *temp2 = NewFunctionTempRegister();
          EmitMove(irl, temp2, rhs);
          EmitMove(irl, temp, lhs);
          EmitOp2(irl, OPC_QFRAC, temp, temp2);
          EmitOp1(irl, OPC_GETQX, temp);
          return temp;
      } else {
        ERROR(expr, "FRAC not supported on P1 yet");
        return temp; //???
      }
  }

  // P2 can use QDIV for unsigned divide
  if (gl_p2 && !isSigned) {
    Operand *temp2 = NewFunctionTempRegister();
    EmitMove(irl, temp2, rhs);
    EmitMove(irl, temp, lhs);
    EmitOp2(irl, OPC_QDIV, temp, temp2);
    EmitOp1(irl, getmod ? OPC_GETQY : OPC_GETQX, temp);
    return temp;
  }

  // Use native QDIV + ABS/NEGC for signed divide with known divisor
  if (gl_p2 && isSigned && rhs->kind == IMM_INT) {
    IR *ir;
    lhs = Dereference(irl,lhs);
    Operand *rhs2 = NewOperand(IMM_INT,rhs->name,abs(rhs->val));
    ir = EmitOp2(irl, OPC_ABS, temp, lhs);
    ir->flags |= FLAG_WC;
    EmitOp2(irl, OPC_QDIV, temp, rhs2);
    EmitOp1(irl, getmod ? OPC_GETQY : OPC_GETQX, temp);
    bool negresult = !getmod && (rhs->val < 0); // Negate result if divisor is negative and we're not getting the remainder
    EmitOp2(irl,negresult ? OPC_NEGNC : OPC_NEGC, temp, temp);
    //ir->cond = negresult ? COND_GE : COND_LT; // only do the negate if x < 0
    return temp;
  }
  
  // Use native QDIV + ABS/NEGC for signed divide with known dividend
  if (gl_p2 && isSigned && lhs->kind == IMM_INT) {
    IR *ir;
    rhs = Dereference(irl,rhs);
    Operand *lhs2 = NewOperand(IMM_INT,lhs->name,abs(lhs->val));
    ir = EmitOp2(irl, OPC_ABS, temp, rhs);
    if (!getmod) ir->flags |= FLAG_WC;
    EmitOp2(irl, OPC_QDIV, lhs2, temp);
    EmitOp1(irl, getmod ? OPC_GETQY : OPC_GETQX, temp);
    if (getmod) {
      EmitOp2(irl, (lhs->val < 0) ? OPC_NEG : OPC_MOV, temp, temp);
    } else {
      EmitOp2(irl, (lhs->val < 0) ? OPC_NEGNC : OPC_NEGC, temp, temp);
    }
    return temp;
  }


  // Fall back on function call
  if (!divfunc) {
    divfunc = NewOperand(IMM_COG_LABEL, "divide_", 0);
    unsdivfunc = NewOperand(IMM_COG_LABEL, "unsdivide_", 0);
    muldiva = GetOneGlobal(REG_ARG, "muldiva_", 0);
    muldivb = GetOneGlobal(REG_ARG, "muldivb_", 0);
  }
  EmitMove(irl, muldiva, lhs);
  EmitMove(irl, muldivb, rhs);
  EmitOp1(irl, OPC_CALL, (isSigned) ? divfunc : unsdivfunc);
  if (getmod) {
    EmitMove(irl, temp, muldiva);
  } else {
    EmitMove(irl, temp, muldivb);
  }
  return temp;
}

static IRCond
CondFromExpr(int kind)
{
  switch (kind) {
  case K_NE:
    return COND_NE;
  case K_EQ:
    return COND_EQ;
  case K_GE:
  case K_GEU:
    return COND_GE;
  case K_LE:
  case K_LEU:
    return COND_LE;
  case '<':
  case K_LTU:
    return COND_LT;
  case '>':
  case K_GTU:
    return COND_GT;
  default:
    break;
  }
  ERROR(NULL, "Unknown boolean operator");
  return COND_FALSE;
}

/*
 * change condition for when the two sides of a comparison
 * are flipped (e.g. a<b becomes b>a)
 */
static IRCond
FlipSides(IRCond cond)
{
  switch (cond) {
  case COND_LT:
    return COND_GT;
  case COND_GT:
    return COND_LT;
  case COND_LE:
    return COND_GE;
  case COND_GE:
    return COND_LE;
  default:
    return cond;
  }
}
/*
 * invert a condition (e.g. EQ -> NE, LT -> GE
 * note that the conditions are declared in a
 * particular order to make this easier
 */
IRCond
InvertCond(IRCond v)
{
  v = (IRCond)(15 ^ (unsigned)v);
  return v;
}

static IRCond
CompileBasicBoolExpression(IRList *irl, AST *expr)
{
  IRCond cond;
  Operand *lhs;
  Operand *rhs;
  int opkind;
  IR *ir;
  int flags;
  int isUnsigned = 0;
  
  if (expr->kind == AST_OPERATOR) {
    opkind = (int)expr->d.ival;
  } else {
    opkind = -1;
  }
  switch(opkind) {
  case K_GEU:
  case K_GTU:
  case K_LEU:
  case K_LTU:
      isUnsigned = 1;
      /* fall through */
  case K_NE:
  case K_EQ:
  case '<':
  case '>':
  case K_LE:
  case K_GE:
    cond = CondFromExpr(opkind);
    lhs = CompileExpression(irl, expr->left, NULL);
    rhs = CompileExpression(irl, expr->right, NULL);
    break;
  default:
    cond = COND_NE;
    lhs = CompileExpression(irl, expr, NULL);
    if (lhs && lhs->kind == IMM_INT) {
        return (lhs->val == 0) ? COND_FALSE : COND_TRUE;
    }
    rhs = NewOperand(IMM_INT, "", 0);
    break;
  }
  /* emit a compare operator */
  /* note that lhs cannot be a constant */
  if (lhs && lhs->kind == IMM_INT) {
    Operand *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
    cond = FlipSides(cond);
  }
  // If comparing with constant, try for a condition that only uses C
  // but only if it's correct and it won't horribly pessimize everything
  if ((cond == COND_LE||cond == COND_GT)
  && rhs && rhs->kind == IMM_INT
  && rhs->val != 511 && (int32_t)rhs->val != (isUnsigned ? UINT32_MAX : INT32_MAX)) {
    cond = (cond == COND_LE) ? COND_LT : COND_GE;
    rhs = NewImmediate(rhs->val+1);
  }
  
  switch (cond) {
  case COND_LT:
  case COND_GE:
    flags = FLAG_WC;
    break;
  case COND_NE:
  case COND_EQ:
    flags = FLAG_WZ;
    break;
  default:
    flags = FLAG_WZ|FLAG_WC;
    break;
  }
  lhs = Dereference(irl, lhs);
  rhs = Dereference(irl, rhs);
  if (isUnsigned || flags == FLAG_WZ) {
      ir = EmitOp2(irl, OPC_CMP, lhs, rhs);
  } else {
      ir = EmitOp2(irl, OPC_CMPS, lhs, rhs);
  }
  ir->flags |= flags;
  return cond;
}

void
CompileBoolBranches(IRList *irl, AST *expr, Operand *truedest, Operand *falsedest)
{
    IRCond cond;
    int opkind;
    Operand *dummylabel = NULL;
    IR *ir;
    
    if (expr->kind == AST_ISBETWEEN) {
        Operand *lo, *hi;
        Operand *val;
        Operand *tmplo, *tmphi;
        Operand *tmp2lo, *tmp2hi;
        
        val = CompileExpression(irl, expr->left, NULL);
        val = Dereference(irl, val);
        lo = CompileExpression(irl, expr->right->left, NULL);
        hi = CompileExpression(irl, expr->right->right, NULL);
        tmplo = NewFunctionTempRegister();
        tmphi = NewFunctionTempRegister();
        tmp2lo = NewFunctionTempRegister();
        tmp2hi = NewFunctionTempRegister();
        EmitMove(irl, tmplo, lo);
        EmitMove(irl, tmp2lo, tmplo);
        EmitMove(irl, tmphi, hi);
        EmitMove(irl, tmp2hi, tmphi);
        EmitOp2(irl, OPC_MAXS, tmplo, tmp2hi); // now tmplo really is the smallest
        EmitOp2(irl, OPC_MINS, tmphi, tmp2lo); // and tmphi really is highest
        ir = EmitOp2(irl, OPC_CMPS, tmplo, val);
        ir->flags |= FLAG_WZ|FLAG_WC;
        ir = EmitOp2(irl, OPC_CMPS, val, tmphi);
        ir->flags |= FLAG_WZ|FLAG_WC;
        ir->cond = COND_LE;
        if (truedest) {
            EmitJump(irl, COND_LE, truedest);
        }
        if (falsedest) {
            EmitJump(irl, COND_GT, falsedest);
        }
        return;
    }
    
    if (expr->kind == AST_OPERATOR) {
        opkind = (int)expr->d.ival;
    } else {
        opkind = -1;
    }

    switch (opkind) {
    case K_BOOL_NOT:
        CompileBoolBranches(irl, expr->right, falsedest, truedest);
        break;
    case K_BOOL_AND:
        if (!falsedest) {
            dummylabel = NewCodeLabel();
            falsedest = dummylabel;
        }
        CompileBoolBranches(irl, expr->left, NULL, falsedest);
        CompileBoolBranches(irl, expr->right, truedest, falsedest);
        if (dummylabel) {
            EmitLabel(irl, dummylabel);
        }
        break;
    case K_BOOL_OR:
        if (!truedest) {
            dummylabel = NewCodeLabel();
            truedest = dummylabel;
        }
        CompileBoolBranches(irl, expr->left, truedest, NULL);
        CompileBoolBranches(irl, expr->right, truedest, falsedest);
        if (dummylabel) {
            EmitLabel(irl, dummylabel);
        }
        break;
    default:
        if (IsConstExpr(expr)) {
            cond = EvalConstExpr(expr) == 0 ? COND_FALSE : COND_TRUE;
        } else {
            cond = CompileBasicBoolExpression(irl, expr);
        }
        if (truedest) {
            EmitJump(irl, cond, truedest);
        }
        if (falsedest) {
            EmitJump(irl, InvertCond(cond), falsedest);
        }
        break;
    }
}

static IROpcode
OpcFromOp(int op)
{
  switch(op) {
  case '+':
    return OPC_ADD;
  case '-':
    return OPC_SUB;
  case '&':
    return OPC_AND;
  case '|':
    return OPC_OR;
  case '^':
    return OPC_XOR;
  case K_SHL:
    return OPC_SHL;
  case K_SAR:
    return OPC_SAR;
  case K_SHR:
    return OPC_SHR;
  case K_NEGATE:
    return OPC_NEG;
  case K_ABS:
    return OPC_ABS;
  case K_ROTL:
      return OPC_ROL;
  case K_ROTR:
      return OPC_ROR;
//  case K_REV:
//      return gl_p2 ? OPC_REV_P2 : OPC_REV_P1;
  case K_LIMITMIN:
      return OPC_MINS;
  case K_LIMITMAX:
      return OPC_MAXS;
  case K_LIMITMIN_UNS:
      return OPC_MINU;
  case K_LIMITMAX_UNS:
      return OPC_MAXU;
  default:
    ERROR(NULL, "Unsupported operator 0x%x", op);
    return OPC_UNKNOWN;
  }
}
static Operand *
Dereference(IRList *irl, Operand *op)
{
    if (IsMemRef(op)) {
        Operand *temp = NewFunctionTempRegister();
        EmitMove(irl, temp, op);
        if (op->size > 4) {
            ERROR(NULL, "internal error, Dereference called on large object");
        }
        return temp;
    }
    return op;
}

static Operand *
CompileBasicOperator(IRList *irl, AST *expr, Operand *dest)
{
  int op = (int)expr->d.ival;
  AST *lhs = expr->left;
  AST *rhs = expr->right;
  Operand *left;
  Operand *right;
  Operand *temp;
  IR *ir;
  int specialcase = 0;
  
  // beware of things like a = b - a
  // so make sure we re-use temp for dest only
  // if it isn't already in use
  if (dest && dest->kind == REG_TEMP) {
      temp = dest;
  } else {
      temp = NewFunctionTempRegister();
  }
  switch(op) {
  case K_REV:
      // we actually want rev lhs, 32 - rhs
      rhs = AstOperator('-', AstInteger(32), rhs);
      if (gl_p2) {
          // reverse, then shift right
          left = CompileExpression(irl, lhs, NULL);
          right = CompileExpression(irl, rhs, NULL);
          EmitMove(irl, temp, left);
          EmitOp1(irl, OPC_REV_P2, temp); // reverse the bits
          EmitOp2(irl, OPC_SHR, temp, right);
          return temp;
      } else {
          left = CompileExpression(irl, lhs, temp);
          right = CompileExpression(irl, rhs, NULL);
          EmitMove(irl, temp, left);
          right = Dereference(irl, right);
          EmitOp2(irl, OPC_REV_P1, temp, right);
          return temp;
      }
  case '-':
  case K_SHL:
  case K_SHR:
  case K_SAR:
  case K_ROTL:
  case K_ROTR:
  case K_LIMITMIN:
  case K_LIMITMAX:
  case K_LIMITMIN_UNS:
  case K_LIMITMAX_UNS:
      left = CompileExpression(irl, lhs, temp);
      right = CompileExpression(irl, rhs, NULL);
      EmitMove(irl, temp, left);
      right = Dereference(irl, right);
      EmitOp2(irl, OpcFromOp(op), temp, right);
      return temp;
  case K_ZEROEXTEND:
  case K_SIGNEXTEND:
      if (gl_p2) {
          AST *rhs_temp = AstOperator('-', rhs, AstInteger(1));
          rhs_temp = FoldIfConst(rhs_temp);
          right = CompileExpression(irl, rhs_temp, NULL);
          left = CompileExpression(irl, lhs, temp);
          EmitMove(irl, temp, left);
          EmitOp2(irl, op == K_ZEROEXTEND ? OPC_ZEROX : OPC_SIGNX, temp, right);
      } else { // P1
          AST *rhs_temp = FoldIfConst(AstOperator('-',AstInteger(32),rhs));
          left = CompileExpression(irl, lhs, temp);
          EmitMove(irl, temp, left);
          if (op == K_ZEROEXTEND && rhs_temp->kind == AST_INTEGER && (rhs_temp->d.ival&31) >= 23) {
              EmitOp2(irl, OPC_AND, temp, NewImmediate(0xFFFFFFFFu>>(rhs_temp->d.ival&31)));
          } else {
              right = CompileExpression(irl, rhs_temp, NULL);
              EmitOp2(irl, OPC_SHL, temp, right);
              EmitOp2(irl, (op == K_ZEROEXTEND) ? OPC_SHR : OPC_SAR, temp, right);
          }
      }
      return temp;
    // commutative ops 
  case '+':
  case '^':
  case '&':
  case '|':
      // there might be something different we could do about
      // commutative operations, but for now handle them the same
      left = CompileExpression(irl, lhs, temp);
      right = CompileExpression(irl, rhs, NULL);
      EmitMove(irl, temp, left);
      right = Dereference(irl, right);
      EmitOp2(irl, OpcFromOp(op), temp, right);
      return temp;
  case K_NEGATE:
  case K_ABS:
      right = CompileExpression(irl, rhs, temp);
      right = Dereference(irl, right);
      EmitOp2(irl, OpcFromOp(op), temp, right);
      return temp;
  case K_FNEGATE:
      right = CompileExpression(irl, rhs, temp);
      right = Dereference(irl, right);
      EmitMove(irl, temp, NewImmediate(0x80000000));
      EmitOp2(irl, OPC_XOR, temp, right);
      return temp;
  case K_BIT_NOT:
      right = CompileExpression(irl, rhs, temp);
      if (gl_p2) {
          right = Dereference(irl, right);
          EmitOp2(irl, OPC_NOT, temp, right);
      } else {
          left = NewImmediate(-1);
          EmitMove(irl, temp, right);
          EmitOp2(irl, OPC_XOR, temp, left);
      }
      return temp;
  case K_ENCODE2:
      specialcase = 1;
      /* fall through */
  case K_ENCODE:
      right = CompileExpression(irl, rhs, temp);
      if (gl_p2) {
          IR *ir;
          ir = EmitOp2(irl, OPC_ENCOD, temp, right);
          if (!specialcase) {
              // the Spin operator returns 0-32; the Spin2 one returns
              // 0-31 with ENCOD(0) == 0
              ir->flags |= FLAG_WC;
              ir = EmitOp2(irl, OPC_ADD, temp, NewImmediate(1));
              ir->cond = COND_LT; // if c is set, src was nonzero and add 1
          }
      } else {
          left = NewFunctionTempRegister();
          EmitMove(irl, left, right);
          if (specialcase) {
              // Spin2 ENCOD only goes 0-31, not 0-32
              EmitMove(irl, temp, NewImmediate(31));
          } else {
              EmitMove(irl, temp, NewImmediate(32));
          }
          right = NewCodeLabel();
          EmitLabel(irl, right);
          ir = EmitOp2(irl, OPC_SHL, left, NewImmediate(1));
          ir->flags |= FLAG_WC;
          ir = EmitOp2(irl, OPC_DJNZ, temp, right);
          ir->cond = COND_NC;
      }
      return temp;
  case K_DECODE:
      ERROR(rhs, "Internal error, decode operators should have been handled in spin transormations");
      return EmptyOperand();

  case K_BOOL_NOT:
  case K_BOOL_AND:
  case K_BOOL_OR:
  case K_EQ:
  case K_NE:
  case K_LE:
  case K_GE:
  case '<':
  case '>':
  case K_LTU:
  case K_GTU:
  case K_LEU:
  case K_GEU:
  {
      Operand *zero = NewImmediate(0);
      Operand *skiplabel = NewCodeLabel();
      Operand *truevalue;

      if (LangBoolIsOne(curfunc->language)) {
          truevalue = NewImmediate(1);
      } else {
          truevalue = NewImmediate(-1);
      }
      EmitMove(irl, temp, zero);
      CompileBoolBranches(irl, expr, NULL, skiplabel);
      EmitOp2(irl, OPC_XOR, temp, truevalue);
      EmitLabel(irl, skiplabel);
      return temp;
  }
  case K_SQRT:
  case K_ONES_COUNT:
  case K_SCAS:
  case K_FRAC64:
  case K_QEXP:
  case K_QLOG:
  case '?':
  {
      ERROR(expr, "Internal error, Unexpected operator 0x%x found, should have been converted to function", op);
      return EmptyOperand();
  }
  default:
    ERROR(lhs, "Unsupported operator 0x%x", op);
    return EmptyOperand();
  }
}

static Operand *
CompileOperator(IRList *irl, AST *expr, Operand *dest)
{
    int op = (int)expr->d.ival;
    switch (op) {
    case K_INCREMENT:
    case K_DECREMENT:
    {
        AST *addone;
        AST *stepsize;
        AST *dest;
        AST *desttype;
        Operand *lhs;
        Operand *temp = NULL;
        int opc = (op == K_INCREMENT) ? '+' : '-';
        if (expr->left) {
            dest = expr->left;
        } else {
            dest = expr->right;
        }
        desttype = ExprType(dest);
        if (IsPointerType(desttype)) {
            stepsize = AstInteger(TypeSize(BaseType(desttype)));
        } else {
            if (IsFloatType(desttype)) {
                ERROR(expr, "Cannot handle ++ or -- for floats yet");
            }
            stepsize = AstInteger(1);
        }
        if (expr->left) {  /* x++ */
            addone = AstAssign(expr->left, AstOperator(opc, expr->left, stepsize));
            temp = NewFunctionTempRegister();
            lhs = CompileExpression(irl, expr->left, NULL);
            EmitMove(irl, temp, lhs);
            CompileExpression(irl, addone, NULL);
            return temp;
        } else {
            addone = AstAssign(expr->right, AstOperator(opc, expr->right, stepsize));
        }
        return CompileExpression(irl, addone, NULL);
    }
    case '*':
        return CompileMul(irl, expr, 0, dest);
    case K_HIGHMULT:
        return CompileMul(irl, expr, 1, dest);
    case K_UNS_HIGHMULT:
        return CompileMul(irl, expr, 3, dest);
    case '/':
        return CompileDiv(irl, expr, 0, dest);
    case K_MODULUS:
        return CompileDiv(irl, expr, 1, dest);
    case K_UNS_DIV:
        return CompileDiv(irl, expr, 2, dest);
    case K_UNS_MOD:
        return CompileDiv(irl, expr, 3, dest);
    case '&':
        if (expr->right->kind == AST_OPERATOR && expr->right->d.ival == K_BIT_NOT) {
            Operand *temp = dest ? dest : NewFunctionTempRegister();
            Operand *lhs = CompileExpression(irl, expr->left, temp);
            Operand *rhs = CompileExpression(irl, expr->right->right, NULL);
            EmitMove(irl, temp, lhs);
            rhs = Dereference(irl, rhs);
            EmitOp2(irl, OPC_ANDN, temp, rhs);
            return temp;
        }
        return CompileBasicOperator(irl, expr, dest);
    default:
        return CompileBasicOperator(irl, expr, dest);
    }
}

static OperandList quitstack;

static void
PushQuitNext(Operand *q, Operand *n)
{
  OperandList *qholder = (OperandList *)malloc(sizeof(OperandList));
  OperandList *nholder = (OperandList *)malloc(sizeof(OperandList));
  qholder->op = quitlabel;
  nholder->op = nextlabel;
  qholder->next = nholder;
  nholder->next = quitstack.next;
  quitstack.next = qholder;

  quitlabel = q;
  nextlabel = n;
}

static void
PopQuitNext()
{
  OperandList *ql, *nl;

  ql = quitstack.next;
  if (!ql || !ql->next) {
    ERROR(NULL, "Internal error, empty loop stack");
    return;
  }
  nl = ql->next;
  quitstack.next = nl->next;
  quitlabel = ql->op;
  nextlabel = nl->op;
  free(nl);
  free(ql);
}

/* compiles a list of expressions; returns
 * a list of operands
 */
static OperandList *
CompileExprList(IRList *irl, AST *fromlist)
{
  AST *from;
  Operand *opfrom = NULL;
  Operand *opto = NULL;
  OperandList *ret = NULL;
  OperandList *fresults = NULL;
  int starttempreg;
  
  while (fromlist) {
    from = fromlist->left;
    fromlist = fromlist->right;

    if (from && from->kind == AST_FUNCCALL) {
        fresults = CompileFunccall(irl, from);
        AppendOperandList(&ret, fresults);
    } else {
        opto = NewFunctionTempRegister();
        starttempreg = FuncData(curfunc)->curtempreg;
        opfrom = CompileExpression(irl, from, NULL);
        if (!opfrom) break;
        EmitMove(irl, opto, opfrom);
        AppendOperand(&ret, opto);
        FreeTempRegisters(irl, starttempreg);
    }
  }
  return ret;
}

//
// returns number of longs pushed onto the stack
//
static int
EmitParameterList(IRList *irl, OperandList *oplist, Function *func, AST *functype)
{
    Operand *op;
    Operand *dst;
    int n = 0;
    int stackadj = 0;
    
    while (oplist != NULL) {
        op = oplist->op;
        oplist = oplist->next;
        dst = GetFunctionParameterForCall(irl, func, functype, n++);
        if (dst) {
            EmitMove(irl, dst, op);
        } else {
            if (stackadj == 0) {
                ValidateStackptr();
                EmitMove(irl, GetArgReg(n-1), stackptr);
            }
            EmitPush(irl, op);
            if (COG_DATA) {
                stackadj++;
            } else {
                stackadj += 4;
            }
        }
    }
    return stackadj; // amount to adjust the stack by after the call
}

/*
 * compile a function call and give the first result
 */
static Operand *
CompileFunccallFirstResult(IRList *irl, AST *expr)
{
    OperandList *ptr = CompileFunccall(irl, expr);
    if (!ptr) {
        return NewImmediate(0);
    }
    return ptr->op;
}

static int
IsDirectMemberVariable(IRList *irl, AST *objref, Operand **offsetPtr)
{
    Operand *offset = NULL;
    AST *arrayidx = NULL;
    AST *typ;

    typ = ExprType(objref);
    if (objref->kind == AST_ARRAYREF) {
        arrayidx = objref->right;
        objref = objref->left;
    }
    if (objref->kind == AST_IDENTIFIER) {
        Symbol *sym;
        const char *name = GetIdentifierName(objref);
        sym = LookupSymbol(name);
        switch(sym->kind) {
        case SYM_VARIABLE:
            if (IsClassType(typ))
            {
                int offval = sym->offset;
                if (arrayidx) {
                    AST *expr = AstInteger(offval);
                    expr = AstOperator('+', expr,
                                       AstOperator('*', AstInteger(TypeSize(typ)), arrayidx));
                    offset = CompileExpression(irl, expr, NULL);
                } else {
                    offset = NewImmediate(sym->offset);
                }
                *offsetPtr = offset;
                return 1;
            }
            return 0;
        default:
            return 0;
        }
    }
    return 0;
}

/*
 * get operands for:
 *  object pointer to use (NULL for the default one)
 *  offset to that object pointer (NULL if non-default)
 *  function to call
 * "expr" is either an "@func" or a funccall
 * returns function, or NULL
 */
Function *
CompileGetFunctionInfo(IRList *irl, AST *expr, Operand **objptr, Operand **offsetptr, Operand **funcptr, AST **ftypeptr)
{
    Operand *objaddr, *offset;
    Symbol *sym;
    Function *func;
    AST *objref = NULL;
    int abstract = 0;
    
    if (ftypeptr) {
        AST *ftype;
        if (expr->kind == AST_FUNCCALL || expr->kind == AST_ADDROF || expr->kind == AST_ABSADDROF) {
            ftype = ExprType(expr->left);
            if (!ftype && curfunc && IsSpinLang(curfunc->language)) {
                ftype = ast_type_generic_funcptr;
            }
            if (ftype && (ftype->kind == AST_PTRTYPE || ftype->kind == AST_REFTYPE || ftype->kind == AST_COPYREFTYPE)) {
                ftype = RemoveTypeModifiers(ftype->left);
            }
            if (ftype && !IsFunctionType(ftype)) {
                ftype = NULL;
            }
        } else {
            ftype = NULL;
        }
        *ftypeptr = ftype;
    }
    sym = FindFuncSymbol(expr, &objref, 1);
    if (!sym || sym->kind != SYM_FUNCTION) {
        Operand *base;
        Operand *tempbase = NewFunctionTempRegister();
        Operand *temp1 = NewFunctionTempRegister();
        Operand *temp2 = NewFunctionTempRegister();
        Operand *ptr1, *ptr2;
        
        base = CompileExpression(irl, expr->left, tempbase);
        EmitMove(irl, tempbase, base);
        ptr1 = SizedHubMemRef(LONG_SIZE, tempbase, 0);
        ptr2 = SizedHubMemRef(LONG_SIZE, tempbase, 4);
        
        EmitMove(irl, temp1, ptr1);
        EmitMove(irl, temp2, ptr2);
        if (objptr) {
            *objptr = temp1;
        }
        if (funcptr) {
            *funcptr = temp2;
        }
        if (offsetptr) {
            *offsetptr = NULL;
        }
        return NULL;
    }
    func = (Function *)sym->val;
    if (func) {
        func->callSites |= 1; // internal functions may not have been marked earlier
    }
    offset = NULL;
    objaddr = NULL;
    
    if (objref) {
        if (IsDirectMemberVariable(irl, objref, &offset)) {
            // do nothing, offset is already set up
        } else {
            AST *objreftype = ExprType(objref);
            if (objref->kind == AST_OBJECT) {
                // direct reference to a static function
                abstract = 1;
            } else {
                objaddr = CompileExpression(irl, objref, NULL);
                if (IsClassType(objreftype)) {
                    objaddr = GetLea(irl, objaddr);
                }
            }
        }
    }
    if (func) {
        if (func->is_static) {
            offset = NULL; // no need to update object
        } else {
            if (abstract && objptr && funcptr) {
                ERROR(expr, "function call requires real object, not an abstract class");
            }
        }
    }
    if (objptr) {
        *objptr = objaddr;
    }
    if (offsetptr) {
        *offsetptr = offset;
    }
    if (funcptr) {
        if (!FuncData(func)) {
            AssignOneFuncName(func);
        }
        *funcptr = FuncData(func)->asmname;
    }
    return func;
}

/*
 * compile a function call
 * returns a list of the things the function returns
 */
static OperandList *
CompileFunccall(IRList *irl, AST *expr)
{
  Function *func = NULL;
  AST *functype = NULL;
  AST *params;
  Operand *offset = NULL;
  Operand *absobjaddr = NULL;
  Operand *funcaddr = NULL;
  OperandList *temp;
  OperandList *results = NULL;
  Operand *reg;
  int numresults;
  IR *ir;
  int i;
  int stackadj = 0;
  int starttempreg = FuncData(curfunc)->curtempreg;

      
  func = CompileGetFunctionInfo(irl, expr, &absobjaddr, &offset, &funcaddr, &functype);

  if (!func && !functype) {
      return NULL;
  }
  params = expr->right;
  temp = CompileExprList(irl, params);

  /* now copy the parameters into place (have to do this in case there are
     function calls within the parameters)
   */
  stackadj = EmitParameterList(irl, temp, func, functype);

  if (offset || absobjaddr) {
      Operand *temp = NULL;
      ValidateObjbase();
      if (offset) {
          EmitOp2(irl, OPC_ADD, objbase, offset);
      } else {
          temp = NewFunctionTempRegister();
          EmitMove(irl, temp, objbase);
          EmitMove(irl, objbase, absobjaddr);
      }
      ir = EmitOp1(irl, OPC_CALL, funcaddr);
      if (offset) {
          EmitOp2(irl, OPC_SUB, objbase, offset);
      } else if (temp) {
          EmitMove(irl, objbase, temp);
      }
  } else {
      ir = EmitOp1(irl, OPC_CALL, funcaddr);
  }
  if (func) {
      FuncData(func)->actual_callsites++;
  }
  ir->aux = (void *)func; // remember the function for optimization purposes

  FreeTempRegisters(irl, starttempreg);
  
  /* now get the results */
  /* NOTE: we cannot assume this is unchanged over future calls,
     so save it in a temp register
  */
  numresults = FuncNumResults(functype);
  if (func && func->numresults != numresults) {
      if (functype) {
          WARNING(NULL, "Internal assert failure: func numresults inconsistent for function %s", func->name);
          func->numresults = numresults;
      } else {
          numresults = func->numresults;
      }
  }
  
  for (i = 0; i < numresults; i++) {
      reg = NewFunctionTempRegister();
      EmitMove(irl, reg, GetResultReg(i));
      AppendOperand(&results, reg);
  }
  if (stackadj) {
      EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(stackadj));
  }
  return results;
}

static Operand *
CompileMultipleAssign(IRList *irl, AST *lhs, AST *rhs)
{
    OperandList *opList, *ptr ;
    Operand *r = NULL;

    // compile out statement lists
    while (rhs && rhs->kind == AST_STMTLIST) {
        // OK, this is slightly tricky, we have to chase the sequence
        // down, compiling the parts, until we get to the final
        // function call
        if (rhs->right) {
            CompileStatement(irl, rhs->left);
            rhs = rhs->right;
        } else {
            rhs = rhs->left;
        }
    }
    while (rhs && rhs->kind == AST_SEQUENCE) {
        // OK, this is slightly tricky, we have to chase the sequence
        // down, compiling the parts, until we get to the final
        // function call
        if (rhs->right) {
            CompileExpression(irl, rhs->left, NULL);
            rhs = rhs->right;
        } else if (rhs->left && rhs->left->kind == AST_SEQUENCE) {
            rhs = rhs->left;
        } else {
            rhs = NULL;
        }
    }
    if (!rhs) {
        ERROR(lhs, "Unexpected end of multiple assignment list");
        return OPERAND_DUMMY;
    }
    if (rhs->kind == AST_EXPRLIST) {
        opList = CompileExprList(irl, rhs);
    } else if (rhs->kind == AST_FUNCCALL) {
        opList = CompileFunccall(irl, rhs);
    } else {
        ERROR(rhs, "Expected multiple values");
        return OPERAND_DUMMY;
    }
    ptr = opList;
    while (lhs && ptr) {
        if (lhs->kind != AST_EXPRLIST) {
            ERROR(lhs, "Illegal left hand side for multiple assignment");
            return OPERAND_DUMMY;
        }
        r = CompileExpression(irl, lhs->left, NULL);
        EmitMove(irl, r, ptr->op);
        lhs = lhs->right;
        ptr = ptr->next;
    }
    if (ptr) {
        ERROR(rhs, "Too many elements on right hand side of assignment");
    } else if (lhs) {
        ERROR(rhs, "Not enough elements on right hand side of assignment");
    }
    return r;
}

static bool
IsCogMem(Operand *addr)
{
    switch (addr->kind) {
    case IMM_COG_LABEL:
    case COGMEM_REF:
    case REG_HW:
    case REG_REG:
    case REG_TEMP:
    case REG_LOCAL:
    case REG_ARG:
    case REG_SUBREG:
        return true;
    case IMM_HUB_LABEL:
    case IMM_STRING:
    case IMM_INT:
    case HUBMEM_REF:
        return false;
    default:
        return COG_DATA;
    }
}

static Operand *
CompileMemref(IRList *irl, AST *expr)
{
    Operand *addr = CompileExpression(irl, expr->right, NULL);
    AST *type = expr->left;
    
    addr = Dereference(irl, addr);
    if (0 && addr->kind != REG_TEMP && IsCogMem(addr)) {
        addr = CogMemRef(addr, 0);
        addr->size = TypeSize(type);
        return addr;
    }
    return TypedHubMemRef(type, addr, 0);
}

static Operand *
OffsetMemory(IRList *irl, Operand *base, Operand *offset, AST *type)
{
    Operand *basereg;
    Operand *newbase;
    Operand *temp;
    int idx;
    int shift;
    int siz;

    // check for COG memory references
    if (!IsMemRef(base) && IsCogMem(base)) {
        Operand *addr;
        long offval = offset->val;
        
        if (base->kind == REG_SUBREG) {
            offval += base->val * LONG_SIZE;
            base = (Operand *)base;
        }
        switch(base->kind) {
        case REG_REG:
        case REG_LOCAL:
        case REG_TEMP:
        case REG_ARG:
        case REG_HW:
#if 1      
            /* special case immediate offsets */
            if (offset->kind == IMM_INT) {
                base->used = 1;
                if (offval == 0) {
                    return base;
                }
                return SubRegister(base, offval);
            }
#endif            
            base->used = 1;
            addr = NewOperand(IMM_COG_LABEL, base->name, 0);
            temp = NewFunctionTempRegister();
            EmitMove(irl, temp, addr);
            base = CogMemRef(temp, 0);
            break;
        default:
            break;
        }
    }
    if (!IsMemRef(base)) {
        ERROR(NULL, "Pointer does not reference memory");
        return base;
    }
    if (base->kind == COGMEM_REF) {
        shift = 2;
    } else {
        shift = 0;
    }
    siz = TypeSize(type);
    basereg = (Operand *)base->name;
    if (!offset || offset->kind == IMM_INT) {
        if (offset) {
            idx = offset->val >> shift;
        } else {
            idx = 0;
        }
        newbase = NewOperand(base->kind, (char *)basereg, idx + base->val);
        newbase->size = siz;
        return newbase;
    }
    temp = NewFunctionTempRegister();
    EmitMove(irl, temp, offset);
    if (shift) {
        EmitOp2(irl, OPC_SHR, temp, NewImmediate(shift));
    }
    newbase = GetLea(irl, base);
    EmitOp2(irl, OPC_ADD, temp, newbase);
    newbase = NewOperand(base->kind, (char *)temp, 0);
    newbase->size = siz;
    return newbase;
}

static Operand *
ApplyArrayIndex(IRList *irl, Operand *base, Operand *offset, int siz)
{
    Operand *basereg;
    Operand *newbase;
    Operand *temp;
    int idx;

    // check for COG memory references
    if (!IsMemRef(base) && IsCogMem(base)) {
        Operand *addr;
        long offval = offset->val * siz;
        if (base->kind == REG_SUBREG) {
            offval += base->val * LONG_SIZE;
            base = (Operand *)base->name;
        }
        switch(base->kind) {
        case REG_REG:
        case REG_LOCAL:
        case REG_TEMP:
        case REG_ARG:
        case REG_HW:
#if 1      
            /* special case immediate offsets */
            if (offset->kind == IMM_INT) {
                base->used = 1;
                if (offval == 0) {
                    return base;
                }
                return SubRegister(base, offval);
            }
#endif            
            base->used = 1;
            addr = NewOperand(IMM_COG_LABEL, base->name, 0);
            temp = NewFunctionTempRegister();
            EmitMove(irl, temp, addr);
            base = CogMemRef(temp, 0);
            break;
        default:
            break;
        }
    }
    if (!IsMemRef(base)) {
        ERROR(NULL, "Array does not reference memory");
        return base;
    }

    if (siz == 0) {
        siz = base->size;
    } else {
        base->size = siz;
    }
    switch (base->kind) {
    case HUBMEM_REF:
        break;
    case COGMEM_REF:
        siz = siz / 4;
        break;
    default:
        ERROR(NULL, "Bad size in array reference");
        siz = 1;
        break;
    }
          
    basereg = (Operand *)base->name;
    if (offset->kind == IMM_INT) {
        idx = offset->val * siz;
        if (idx == 0) {
            return base;
        }
        newbase = NewOperand(base->kind, (char *)basereg, idx + base->val);
        newbase->size = siz;
        return newbase;
    }
    temp = NewFunctionTempRegister();
    EmitMove(irl, temp, offset);
    if (siz > 1) {
        if (siz == 4) {
            EmitOp2(irl, OPC_SHL, temp, NewImmediate(2));
        } else {
            Operand *temp2 = doCompileMul(irl, temp, NewImmediate(siz), 0, temp);
            temp = temp2;
        }
    }
    newbase = GetLea(irl, base);
    EmitOp2(irl, OPC_ADD, temp, newbase);
    newbase = NewOperand(base->kind, (char *)temp, 0);
    newbase->size = siz;
    return newbase;
}

static Operand *
CompileCondResult(IRList *irl, AST *expr)
{
    AST *cond = expr->left;
    AST *ifpart = expr->right->left;
    AST *elsepart = expr->right->right;
    Operand *r = NewFunctionTempRegister();
    Operand *tmp;
    Operand *label1 = NewCodeLabel();
    Operand *label2 = NewCodeLabel();

    CompileBoolBranches(irl, cond, NULL, label1);
    /* the default is the IF part */
    tmp = CompileExpression(irl, ifpart, NULL);
    EmitMove(irl, r, tmp);
    EmitJump(irl, COND_TRUE, label2);

    /* here is the ELSE part */
    EmitLabel(irl, label1);
    tmp = CompileExpression(irl, elsepart, NULL);
    EmitMove(irl, r, tmp);

    /* all done */
    EmitLabel(irl, label2);

    return r;
}

//
// compile a masked fetch:
//   (a & mask) | val
// here a is expr->left, mask = expr->right->left, val = expr->right->right
// useful for manipulating groups of bits in OUTA and DIRA
//
static Operand *
CompileMaskMove(IRList *irl, AST *expr)
{
    AST *destast = expr->left;
    AST *maskast;
    AST *valast;
    Operand *tmp = NewFunctionTempRegister();
    Operand *dest;
    Operand *val;
    Operand *mask;
    unsigned keepflag = 0;
    IR *ir;

    if (expr->right->kind == AST_SEQUENCE) {
        maskast = expr->right->left;
        valast = expr->right->right;
    } else {
        ERROR(expr, "Internal format error");
        return tmp;
    }
    
    val = CompileExpression(irl, valast, NULL);
    mask = CompileExpression(irl, maskast, NULL);

    switch(destast->kind) {
    case AST_IDENTIFIER:
        dest = CompileIdentifier(irl, destast);
        break;
    case AST_LOCAL_IDENTIFIER:
        dest = CompileIdentifier(irl, destast->left);
        break;
    case AST_HWREG:
        dest = CompileHWReg(irl, destast);
// is this really necessary? it certainly hinders optimization        
//        keepflag = FLAG_KEEP_INSTR;
        break;
    case AST_ARRAYREF:
    case AST_METHODREF:
    case AST_MEMREF:
        dest = CompileExpression(irl, destast, NULL);
        break;
    default:
        ERROR(expr, "Internal error, bad value  in MaskMove");
        return tmp;
    }
    ir = EmitMove(irl, tmp, dest);
    ir->flags |= keepflag;
    ir = EmitOp2(irl, OPC_AND, tmp, mask);
    ir->flags |= keepflag;
    ir = EmitOp2(irl, OPC_OR, tmp, val);
    ir->flags |= keepflag;
    return tmp;
}

//
// get the effective address of "src"
// this may require us to emit instructions
//
Operand *
GetLea(IRList *irl, Operand *src)
{
    // check for COG memory references
    if (!IsMemRef(src) && IsCogMem(src)) {
        Operand *addr;
        long offset = 0;
        if (src->kind == REG_SUBREG) {
            addr = (Operand *)src->name;
            offset = src->val;
            return NewOperand(IMM_COG_LABEL, OffsetName(addr->name, offset / LONG_SIZE), 0);
        }
        switch(src->kind) {
        case IMM_COG_LABEL:
            return src;
        case REG_REG:
        case REG_LOCAL:
        case REG_TEMP:
        case REG_ARG:
        case REG_HW:
            src->used = 1;
            addr = NewOperand(IMM_COG_LABEL, src->name, offset);
            return addr;

        default:
            break;
        }
    }
    if (IsMemRef(src)) {
        Operand *dst = NewFunctionTempRegister();
        int off = src->val;
        src = (Operand *)src->name;
        if (off) {
            src = EmitAddSub(irl, src, off);
        }
        EmitMove(irl, dst, src);
        if (off) {
            src = EmitAddSub(irl, src, -off);
        }
        return dst;
    } else if (src->kind == IMM_HUB_LABEL) {
        return NewImmediatePtr(NULL, src);
    } else {
        ERROR(NULL, "Load Effective Address on a non-memory reference");
        return NULL;
    }
}

//
// compile a coginit expression
// if we have a spin function call inside it then this
// is mildly complicated
//
static Operand *
CompileCoginit(IRList *irl, AST *expr)
{
    AST *funccall;
    AST *params = expr->left;
    
    if ( IsSpinCoginit(expr, NULL) )
    {
        AST *exprlist;
        AST *func;
        AST *stack;
        AST *cogid;
        AST *kernel;
        AST *functype;
        Operand *newstackptr;
        Operand *newstacktop;
        Operand *const4;
        Operand *funcptr;
        Operand *fobjptr;
        Operand *baseobjptr;
        Operand *offset;
        OperandList *plist;
        int numparams = 0;
        Function *remote;

        if (!kernelptr) {
            kernelptr = NewImmediatePtr("entryptr__", NewOperand(IMM_HUB_LABEL, ENTRYNAME, 0));
        }
        kernel = NewAST(AST_OPERAND, NULL, NULL);
        kernel->d.ptr = (void *)kernelptr;

        exprlist = expr->left;
        if (!exprlist) {
            ERROR(expr, "Missing cog parameter for coginit/cognew");
            return OPERAND_DUMMY;
        }
        cogid = exprlist->left;
        exprlist = exprlist->right;
        if (!exprlist) {
            ERROR(expr, "Missing function parameter for coginit/cognew");
            return OPERAND_DUMMY;
        }
        func = exprlist->left;
        exprlist = exprlist->right;
        if (!exprlist) {
            ERROR(expr, "Missing stack parameter for coginit/cognew");
            return OPERAND_DUMMY;
        }
        stack = exprlist->left;
        if (exprlist->right != NULL) {
            ERROR(expr, "Too many parameters to coginit/cognew");
        }

        remote = CompileGetFunctionInfo(irl, func, &baseobjptr, &offset, &funcptr, &functype);
        fobjptr = NewFunctionTempRegister();
        if (!baseobjptr) {
            ValidateObjbase();
            baseobjptr = objbase;
        }
        EmitMove(irl, fobjptr, baseobjptr);
        if (offset) {
            EmitOp2(irl, OPC_ADD, fobjptr, offset);
        }
        if (!IS_FAST_CALL(remote)) {
            ERROR(expr, "Internal error, wrong calling convention for coginit");
            return OPERAND_DUMMY;
        }
        if (gl_output == OUTPUT_COGSPIN) {
            ERROR(expr, "coginit/cognew of Spin objects is not permitted from COG code");
            return OPERAND_DUMMY;
        }
        if (remote && InCog(remote)) {
            ERROR(expr, "Coginit target must be in hub memory.");
            return OPERAND_DUMMY;
        }
        
        // we have to build the call into the new stack
        if (IsIdentifier(func)) {
            params = NULL;
        } else if (func->kind == AST_FUNCCALL) {
            params = func->right;
        } else {
            ERROR(expr, "Internal error, expected function call");
            return OPERAND_DUMMY;
        }
        ValidateObjbase();
        if (stack->kind != AST_ADDROF && stack->kind != AST_DATADDROF
            && stack->kind != AST_ABSADDROF)
        {
            if (!IsPointerType(ExprType(stack))) {
                WARNING(stack, "Normally the coginit stack parameter should be an address");
            }
        }
        newstackptr = CompileExpression(irl, stack, NULL);
        newstackptr = Dereference(irl, newstackptr);
        if (COG_DATA) {
            newstacktop = CogMemRef(newstackptr, 0);
            const4 = NewImmediate(1);
        } else {
            newstacktop = SizedHubMemRef(LONG_SIZE, newstackptr, 0);
            const4 = NewImmediate(4);
        }
        EmitMove(irl, newstacktop, fobjptr);
        EmitOp2(irl, OPC_ADD, newstackptr, const4);
        // push the function to call
        EmitMove(irl, newstacktop, funcptr);
        EmitOp2(irl, OPC_ADD, newstackptr, const4);
        // now the parameters
        plist = CompileExprList(irl, params);
        while (plist) {
            EmitMove(irl, newstacktop, plist->op);
            EmitOp2(irl, OPC_ADD, newstackptr, const4);
            plist = plist->next;
            numparams++;
        }
        // OK, new stack is all set up
        // now we need to call coginit(cogid, @entry, stack)
        params = NewAST( AST_EXPRLIST, cogid,
                         NewAST(AST_EXPRLIST, kernel,
                                NewAST(AST_EXPRLIST, stack, NULL)) );

        if (numparams > max_coginit_args) {
            ERROR(NULL, "Internal error, coginit with too many args");
            max_coginit_args = numparams;
        }
    }
    funccall = AstIdentifier("_coginit");
    funccall = NewAST(AST_FUNCCALL, funccall, params);
    return CompileFunccallFirstResult(irl, funccall);
}

//
// compile a lookup/lookdown
//
static Operand *
CompileLookupDown(IRList *irl, AST *expr)
{
    AST *funccall;
    AST *idx, *base;
    AST *params;
    AST *ev, *table;
    AST *len;
    AST *arrid;
    Operand *value;
    
    int popsize = 0;
    if (expr->kind == AST_LOOKDOWN) {
        funccall = AstIdentifier("_lookdown");
    } else {
        funccall = AstIdentifier("_lookup");
    }
    ev = expr->left;
    if (ev->kind != AST_LOOKEXPR) {
        ERROR(ev, "Internal error in lookup");
        return OPERAND_DUMMY;
    }
    base = ev->left;
    idx = ev->right;
    table = expr->right;

    if (table->kind == AST_TEMPARRAYUSE) {
        len = table->right;
        arrid = NewAST(AST_ADDROF, table->left, NULL);
    } else if (table->kind == AST_EXPRLIST) {
        // FIXME?? ASSUMES STACK GROWS UP!!!
        int n;
        Operand *basereg = NewFunctionTempRegister();
        Operand *finalidx;
        
        /* NOTE!
           we have to evaluate the index before evaluating array elements
        */
        finalidx = CompileExpression(irl, idx, NULL);
        idx = NewAST(AST_OPERAND, NULL, NULL);
        idx->d.ptr = (void *)finalidx;
        
        ValidateStackptr();
        arrid = NewAST(AST_OPERAND, NULL, NULL);
        arrid->d.ptr = (void *)basereg;
        EmitMove(irl, basereg, stackptr);
        n = 0;
        while (table) {
            Operand *src = CompileExpression(irl, table->left, NULL);
            EmitPush(irl, src);
            n++;
            table = table->right;
        }
        len = AstInteger(n);
        popsize = n*LONG_SIZE;
    } else {
        ERROR(table, "Internal error in lookup code");
        return OPERAND_DUMMY;
    }

    params = NewAST(AST_EXPRLIST, idx,
                    NewAST(AST_EXPRLIST, base,
                           NewAST(AST_EXPRLIST, arrid,
                                  NewAST(AST_EXPRLIST, len, NULL))));
    funccall = NewAST(AST_FUNCCALL, funccall, params);
    value = CompileFunccallFirstResult(irl, funccall);
    if (popsize) {
        EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(popsize));
    }
    return value;
}

//
// get the address of an expression
//
static Operand *
GetAddressOf(IRList *irl, AST *expr)
{
    Operand *res;
    Operand *tmp;
    switch (expr->kind) {
    case AST_RESULT:
    case AST_IDENTIFIER:
    case AST_LOCAL_IDENTIFIER:
        res = CompileExpression(irl, expr, NULL);
        tmp = GetLea(irl, res);
        return tmp;
    case AST_ARRAYREF:
    {
      Operand *base;
      Operand *offset;
      int siz;
      
      if (!expr->right) {
          ERROR(expr, "Array ref with no index?");
          return NewOperand(REG_REG, "???", 0);
      }
      base = CompileExpression(irl, expr->left, NULL);
      offset = CompileExpression(irl, expr->right, NULL);
      siz = TypeSize(ExprType(expr));
      res = ApplyArrayIndex(irl, base, offset, siz);
      tmp = GetLea(irl, res);
      return tmp;
    }
    case AST_METHODREF:
    {
        Symbol *sym;
        Operand *res = NULL;
        const char *name;
        Module *P;
        int off;
        
        name = GetUserIdentifierName(expr->right);
        sym = LookupMemberSymbol(expr, ExprType(expr->left), name, &P, NULL);
        if (sym) {
            if (sym->kind == SYM_VARIABLE) {
                Operand *base = CompileExpression(irl, expr->left, NULL);
                Operand *tmp;
                AST *type = ExprType(expr);
                off = sym->offset;
                tmp = OffsetMemory(irl, base, NewImmediate(off), type);
                res = GetLea(irl, tmp);
            } else if (sym->kind == SYM_FUNCTION) {
                CompileGetFunctionInfo(irl, expr, NULL, NULL, &res, NULL);
            }
        }
        if (!res) {
            ERROR(expr, "Internal error, cannot take address of %s", name);
            res = OPERAND_DUMMY;
        }
        return res;
    }
    default:
        ERROR(expr, "Cannot take address of expression");
        break;
    }
    return NewImmediate(-1);
}

static void
EmitComments(IRList *irl, AST *comment)
{
    Operand *r;
    
    while (comment) {
        if (comment->kind == AST_COMMENT) {
            /* for now ignore comments */
#if 0
            if (comment->d.string && !gl_srccomments) {
                r = NewOperand(IMM_STRING, comment->d.string, 0);
                EmitOp1(irl, OPC_COMMENT, r);
            }
#endif
        } else if (comment->kind == AST_SRCCOMMENT) {
            LineInfo *info = GetLineInfo(comment);
            if (info && info->linedata && gl_srccomments) {
                r = NewOperand(IMM_STRING, info->linedata, 0);
                EmitOp1(irl, OPC_COMMENT, r);
            }
        } else {
            ERROR(comment, "Internal error, expected comment node");
        }
        comment = comment->right;
    }
}

static void
EmitDebugComment(IRList *irl, AST *ast)
{
}

static bool
validateArrayRef(AST *ast)
{
    if (!ast) {
        return false;
    }
    switch (ast->kind) {
    case AST_MEMREF:
        return true;
    case AST_IDENTIFIER:
    case AST_LOCAL_IDENTIFIER:
    case AST_METHODREF:
        return IsArrayType(ExprType(ast));
    default:
        return validateArrayRef(ast->left) || validateArrayRef(ast->right);
    }
}

static Operand *
CompileExpression(IRList *irl, AST *expr, Operand *dest)
{
  Operand *r;
  Operand *val;

  while (expr && expr->kind == AST_COMMENTEDNODE) {
      EmitComments(irl, expr->right);          
      expr = expr->left;
  }
  if (!expr) return NULL;
  if (expr->kind == AST_COMMENT) return NULL;
  if (IsConstExpr(expr)) {
      switch (expr->kind) {
      case AST_SYMBOL:
      case AST_IDENTIFIER:
      case AST_LOCAL_IDENTIFIER:
          // leave symbolic constants alone
      case AST_ADDROF:
      case AST_ABSADDROF:
          // similarly for address expressions
          break;
      default:
          expr = FoldIfConst(expr);
          break;
      }
  }
  switch (expr->kind) {
  case AST_CONDRESULT:
      return CompileCondResult(irl, expr);
  case AST_MASKMOVE:
      return CompileMaskMove(irl, expr);
  case AST_CATCH:
      ERROR(expr, "Internal error, should not see raw CATCH");
      return OPERAND_DUMMY;
  case AST_SETJMP:
      ValidateAbortFuncs();
      if (expr->left) {
          val = CompileExpression(irl, expr->left, NULL);
          r = GetResultReg(0);
      } else {
          val = abortchain;
          r = abortcaught;
      }
      EmitMove(irl, GetArgReg(0), val);
      EmitOp1(irl, OPC_CALL, setjmpfunc);
      return r;
  case AST_CATCHRESULT:
      ValidateAbortFuncs();
      return abortresult;
  case AST_TRYENV:
  {
      Operand *tmp;
      ValidateAbortFuncs();
      EmitPush(irl, abortchain);
      EmitMove(irl, abortchain, stackptr);
      EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(SETJMP_BUF_SIZE));
      tmp = CompileExpression(irl, expr->left, NULL);
      EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(SETJMP_BUF_SIZE));
      EmitPop(irl, abortchain);
      return tmp;
  }
  case AST_ISBETWEEN:
  {
      Operand *zero = NewImmediate(0);
      Operand *skiplabel = NewCodeLabel();
      Operand *temp = NewFunctionTempRegister();
      Operand *truevalue = (LangBoolIsOne(curfunc->language)) ? NewImmediate(1) : NewImmediate(-1);
      EmitMove(irl, temp, zero);
      CompileBoolBranches(irl, expr, NULL, skiplabel);
      EmitOp2(irl, OPC_XOR, temp, truevalue);
      EmitLabel(irl, skiplabel);
      return temp;
  }
  case AST_SEQUENCE:
      r = CompileExpression(irl, expr->left, NULL);
      if (expr->right) {
          r = CompileExpression(irl, expr->right, NULL);
      }
      return r;
  case AST_INTEGER:
  case AST_FLOAT:
    r = NewImmediate((int32_t)expr->d.ival);
    return r;
  case AST_SYMBOL:
  {
      Symbol *sym = (Symbol *)expr->d.ptr;
      r = CompileSymbolForFunc(irl, sym, curfunc, expr);
      if (dest) {
          EmitMove(irl, dest, r);
          r = dest;
      }
      return r;
  }
  case AST_RESULT:
  case AST_IDENTIFIER:
  case AST_LOCAL_IDENTIFIER:
    r = CompileIdentifier(irl, expr);
    if (dest) {
        EmitMove(irl, dest, r);
        r = dest;
    }
    return r;
  case AST_EMPTY:
      r = EmptyOperand();
      return r;
  case AST_VA_START:
  {   // va_start(x, n) -> mov x, __func__varargs
      Operand *src = CompileIdentifier(irl, expr); // VA_START gets __func__varargs
      r = CompileIdentifier(irl, expr->left);
      EmitMove(irl, r, src);
      return r;
  }
  case AST_SELF:
      ValidateObjbase();
      return objbase;
  case AST_HWREG:
    return CompileHWReg(irl, expr);
  case AST_OPERATOR:
      return CompileOperator(irl, expr, dest);
  case AST_FUNCCALL:
      return CompileFunccallFirstResult(irl, expr);
  case AST_ASSIGN:
      if (expr->d.ival != K_ASSIGN) {
          ERROR(expr, "Internal error, asm code cannot handle assignment");
      }
      if (expr->left) {
          AST *typ;
          int n;
          typ = ExprType(expr->left);
          if (typ && !TypeGoesOnStack(typ)) {
              // construct a list of object members
              n = (TypeSize(typ) + (LONG_SIZE-1)) / LONG_SIZE;
              if (n > 1) {
                  expr->left = BuildExprlistFromObject(expr->left, typ);
              }
          }
          if (expr->left->kind == AST_EXPRLIST) {
              // do a series of assignments
              if (expr->right->kind != AST_EXPRLIST) {
                  typ = ExprType(expr->right);
                  if (typ && !TypeGoesOnStack(typ)) {
                      expr->right = BuildExprlistFromObject(expr->right, typ);
                  }
              }
              return CompileMultipleAssign(irl, expr->left, expr->right);
          }
      }
      if ( IsSpinLang(curfunc->language)) {
          // Spin always evaluates RHS first, then LHS
          val = CompileExpression(irl, expr->right, NULL);
          r = CompileExpression(irl, expr->left, NULL);
      } else {
          // for other languages, try to optimize by
          // compiling RHS first and if it's a register re-using it
          r = CompileExpression(irl, expr->left, NULL);
          if (IsRegister(r->kind)) {
              val = CompileExpression(irl, expr->right, r);
          } else {
              val = CompileExpression(irl, expr->right, NULL);
          }
      }
      EmitMove(irl, r, val);
      return r;
  case AST_RANGEREF:
      return CompileExpression(irl, TransformRangeUse(expr), dest);
  case AST_STRING:
      if (strlen(expr->d.string) > 1)  {
            ERROR(expr, "string too long, expected a single character");
      }
      return NewImmediate(expr->d.string[0]);
  case AST_STRINGPTR:
  {
      // evaluate any const references in our current context
      AST *stringExpr = EvalStringConst(expr->left);
      r = GetOneHub(STRING_DEF, NewTempLabelName(), (intptr_t)(stringExpr));
      if (gl_p2) {
          Operand *temp = NewFunctionTempRegister();
          EmitMove(irl, temp, r);
          return temp;
      }
      return NewImmediatePtr(NULL, r);
  }
  case AST_ARRAYREF:
  {
      Operand *base;
      Operand *offset;
      int siz;
      if (!expr->right || !expr->left) {
          ERROR(expr, "Bad array reference");
          return NewOperand(REG_REG, "???", 0);
      }
      if (!validateArrayRef(expr->left)) {
          ERROR(expr, "%s is not an array", GetVarNameForError(expr->left));
      }
      base = CompileExpression(irl, expr->left, NULL);
      offset = CompileExpression(irl, expr->right, NULL);
      if (IsFunctionType(ExprType(expr->left))) {
          return base;
      }
      siz = TypeSize(ExprType(expr));
      return ApplyArrayIndex(irl, base, offset, siz);
  }
  case AST_SPRREF:
  {
      Operand *base;

      if (!expr->left) {
          ERROR(expr, "SPR ref with no index?");
          return NewOperand(REG_REG, "???", 0);
      }
      base = CompileExpression(irl, expr->left, NULL);
      return CogMemRef(base, 0);
  }
  case AST_MEMREF:
  {
      if (IsFunctionType(ExprType(expr->right))) {
          return CompileExpression(irl, expr->right, dest);
      }
      return CompileMemref(irl, expr);
  }
  case AST_COGINIT:
    return CompileCoginit(irl, expr);
  case AST_ADDROF:
  case AST_ABSADDROF:
      return GetAddressOf(irl, expr->left);
  case AST_DATADDROF:
      /* this requires that we add an offset to the value we get */
  {
      Operand *temp = dest ? dest : NewFunctionTempRegister();
      Operand *val = CompileExpression(irl, expr->left, dest);
      Operand *datbase = ValidateDatBase(current);
      
      EmitMove(irl, temp, val);
      EmitOp2(irl, OPC_ADD, temp, datbase);
      return temp;
  }
  case AST_LOOKUP:
  case AST_LOOKDOWN:
      return CompileLookupDown(irl, expr);
  case AST_OPERAND:
      /* stashed away by other code */
      return (Operand *)expr->d.ptr;
  case AST_STMTLIST:
  {
      AST *list = expr;
      if (list && list->right) {
          CompileStatement(irl, list->left);
          return CompileExpression(irl, list->right, dest);
      }
      if (list) {
          AST *finalexpr = list->left;
          while (finalexpr && finalexpr->kind == AST_COMMENTEDNODE) {
              finalexpr = finalexpr->left;
          }
          if (finalexpr) {
              switch(finalexpr->kind) {
              case AST_IF:
              case AST_GOTO:
              case AST_GOSUB:
              case AST_WHILE:
              case AST_DOWHILE:
              case AST_FOR:
              case AST_FORATLEASTONCE:
              case AST_CASE:
              case AST_ENDCASE:
              case AST_QUITLOOP:
              case AST_LABEL:
                  CompileStatement(irl, list);
                  WARNING(expr, "statement expression does not produce a value");
                  return OPERAND_DUMMY;
              default:
                  return CompileExpression(irl, finalexpr, dest);
              }
          }
      }
      ERROR(expr, "expected expression at end of statement list");
      return OPERAND_DUMMY;
  }
  case AST_VA_ARG:
  {
      // x = va_arg(vl, type) should act like:
      //  x = *(type *)vl;
      //  vl += sizeof(type)
      int incsize = TypeSize(expr->left);
      Operand *temp = dest ? dest : NewFunctionTempRegister();
      r = CompileMemref(irl, expr);
      EmitMove(irl, temp, r);
      (void)CompileExpression(irl,
                              AstAssign(expr->right,
                                        AstOperator('+', expr->right, AstInteger(incsize))),
                              NULL);
      return temp;
  }
  case AST_METHODREF:
  {
      Symbol *sym;
      Operand *base;
      AST *finaltype;
      AST *objtype;
      int off = 0;
      Module *P;
      const char *name;
      objtype = ExprType(expr->left);
      name = GetUserIdentifierName(expr->right);
      if (!IsClassType(objtype)) {
          ERROR(expr, "Request for member %s in something that is not a class", name);
          return EmptyOperand();
      }
      base = CompileExpression(irl, expr->left, NULL);
      sym = LookupMemberSymbol(expr, objtype, name, &P, NULL);
      if (sym) {
          switch (sym->kind) {
          case SYM_VARIABLE:
              off = sym->offset;
              break;
          default:
              ERROR(expr, "Unable to dereference symbol %s in %s", name, P->classname);
              break;
          }
      }
      finaltype = ExprType(expr);
      if (off == 0 && IsCogMem(base)) {
          r = base;
      } else {
          r = OffsetMemory(irl, base, NewImmediate(off), finaltype);
      }
      return r;
  }
  case AST_CAST:
      return CompileExpression(irl, expr->right, dest);
  case AST_DECLARE_VAR:
      return CompileExpression(irl, expr->right, dest);
  case AST_ALLOCA:
  {
      Operand *temp;
      AST *alignexpr;
      ValidateStackptr();
      // we have to keep the stack long aligned
      alignexpr = AstOperator('&',
                              AstOperator('+', expr->right, AstInteger(3)),
                              AstOperator(K_BIT_NOT, NULL, AstInteger(3)));
      r = CompileExpression(irl, alignexpr, NULL);
      temp = dest ? dest : NewFunctionTempRegister();
      EmitMove(irl, temp, stackptr);
      EmitOp2(irl, OPC_ADD, stackptr, r);
      return temp;
  }
  case AST_CONSTANT:
  {
      if (!IsConstExpr(expr->left)) {
          WARNING(expr, "CONSTANT expression is not actually constant and will be evaluated at run time");
      }
      return CompileExpression(irl, expr->left, dest);
  }
  case AST_GETHIGH:
  case AST_GETLOW:
  {
      Operand *base;
      int off = expr->kind == AST_GETLOW ? 0 : 4;
      base = CompileExpression(irl, expr->left, NULL);
      r = OffsetMemory(irl, base, NewImmediate(off), ast_type_long);
      if (dest) {
          EmitMove(irl, dest, r);
          r = dest;
      }
      return r;
  }
  default:
    ERROR(expr, "Cannot handle expression yet");
    return NewOperand(REG_REG, "???", 0);
  }
}

static void
CompileStatementList(IRList *irl, AST *ast)
{
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR(ast, "Internal error, expected statement list, got %d",
                  ast->kind);
            return;
        }
        CompileStatement(irl, ast->left);
        ast = ast->right;
    }
}
static Operand *EmitAddSub(IRList *irl, Operand *dst, int off)
{
    IROpcode opc  = OPC_ADD;
    Operand *imm;
    
    if (off < 0) {
        off = -off;
        opc = OPC_SUB;
    }
    if (dst->kind == IMM_INT) {
        off += dst->val;
        return NewImmediate(off);
    }
    imm = NewImmediate(off);
    EmitOp2(irl, opc, dst, imm);
    return dst;
}

static Operand *ImmCogRef(Operand *addr)
{
    char *immname = (char *)calloc(1, 16);
    sprintf(immname, "%lu + 0", (unsigned long)addr->val);
    return NewOperand(REG_REG, immname, 0);
}

static IR *EmitCogread(IRList *irl, Operand *dst, Operand *src)
{
    Operand *dstimm;
    static Operand *zero = NULL;

    if (!zero) zero = NewImmediate(0);
    if (dst->kind != IMM_INT) {
        EmitOp1(irl, OPC_LIVE, dst); // FIXME: the optimizer should be smart enough to deduce this?
    }
    if (gl_p2) {
        IR *ir;

        if (src->kind == IMM_INT) {
            Operand *newsrc = ImmCogRef(src);
            ir = EmitOp2(irl, OPC_MOV, dst, newsrc);
            return ir;
        }
        EmitOp1(irl, OPC_LIVE, src); // FIXME: the optimizer should be smart enough to deduce this?
        ir = EmitOp2(irl, OPC_ALTS, src, zero);
        ir->flags |= FLAG_KEEP_INSTR;
        ir = EmitOp2(irl, OPC_MOV, dst, src);
        ir->flags |= FLAG_KEEP_INSTR;
        return ir;
    } else {
        if (!putcogreg) {
            putcogreg = NewOperand(IMM_COG_LABEL, "wrcog", 0);
        }
        dstimm = GetLea(irl, dst);
        EmitOp2(irl, OPC_MOVS, putcogreg, src);
        EmitOp2(irl, OPC_MOVD, putcogreg, dstimm);
        return EmitOp1(irl, OPC_CALL, putcogreg);
    }
}

static IR *EmitCogwrite(IRList *irl, Operand *src, Operand *dst)
{
    Operand *srcimm;
    static Operand *zero = NULL;
    IR *ir;

    if (!zero) zero = NewImmediate(0);
    EmitOp1(irl, OPC_LIVE, src);
    src = Dereference(irl, src);
    if (gl_p2) {
        if (dst->kind == IMM_INT) {
            Operand *newdst = ImmCogRef(dst);
            ir = EmitOp2(irl, OPC_MOV, newdst, src);
            return ir;
        }
        EmitOp1(irl, OPC_LIVE, dst); // FIXME: the optimizer should be smart enough to deduce this?
        ir = EmitOp2(irl, OPC_ALTD, dst, zero);
        ir->flags |= FLAG_KEEP_INSTR;
        ir = EmitOp2(irl, OPC_MOV, dst, src);
        ir->flags |= FLAG_KEEP_INSTR;
        return ir;
    } else {
        if (!putcogreg) {
            putcogreg = NewOperand(IMM_COG_LABEL, "wrcog", 0);
        }
        srcimm = GetLea(irl, src);
        EmitOp2(irl, OPC_MOVS, putcogreg, srcimm);
        EmitOp2(irl, OPC_MOVD, putcogreg, dst);
        return EmitOp1(irl, OPC_CALL, putcogreg);
    }
}

#define MAX_TMP_REGS 4

static IR *EmitMove(IRList *irl, Operand *origdst, Operand *origsrc)
{
    Operand *temps[MAX_TMP_REGS] = { 0 };
    unsigned i;
    Operand *dst = origdst;
    Operand *src = origsrc;
    IR *ir = NULL;
    unsigned num_tmp_regs = 1;

    if (IsEmptyOperand(origdst)) {
        return ir;
    }
    if (src->kind == IMM_HUB_LABEL) {
        src = NewImmediatePtr(NULL, src);
    }
    if (IsMemRef(src)) {
        int off = src->val;
        Operand *where = NULL;
        unsigned size = origsrc->size;
        num_tmp_regs = (size + 3) / 4;

        if (num_tmp_regs > MAX_TMP_REGS) {
            ERROR(NULL, "too many temporary registers needed");
            return ir;
        }
        // if we are reading into a register, no need for
        // a temp (unless there's an offset, in which case we
        // need to watch out for the case where src == dst)
        src = (Operand *)src->name;
        if (IsValidDstReg(origdst) && off == 0 && num_tmp_regs == 1) {
            temps[0] = where = origdst;
        } else {
            for (i = 0; i < num_tmp_regs; i++) {
                temps[i] = NewFunctionTempRegister();
            }
            where = temps[0];
        }
        if (off) {
            if (src->kind == IMM_INT) {
                src->val += off;
            } else {
                EmitAddSub(irl, src, off);
            }
        }
        if (origsrc->kind == COGMEM_REF) {
            for (i = 0; i < num_tmp_regs; i++) {
                ir = EmitCogread(irl, temps[i], src);
                if (i+1 != num_tmp_regs) {
                    src = OffsetMemory(irl, src, NewImmediate(4), NULL);
                }
            }
        } else if (origsrc->kind == HUBMEM_REF) {
            if (size == 4) {
                ir = EmitOp2(irl, OPC_RDLONG, where, src);
            } else if (size == 2) {
                ir = EmitOp2(irl, OPC_RDWORD, where, src);
            } else if (size == 1) {
                ir = EmitOp2(irl, OPC_RDBYTE, where, src);
            } else {
                Operand *inc4 = NewImmediate(4);
                for (i = 0; i < num_tmp_regs; i++) {
                    ir = EmitOp2(irl, OPC_RDLONG, temps[i], src);
                    if (i != (num_tmp_regs-1)) {
                        ir = EmitOp2(irl, OPC_ADD, src, inc4);
                    }
                }
                where = NULL; // use temps array
                EmitAddSub(irl, src, -4*(num_tmp_regs-1));
            }
        } else {
            ERROR(NULL, "Illegal memory reference");
        }

        if (off && src->kind != IMM_INT) {
            EmitAddSub(irl, src, -off);
        }
        if (where == origdst) {
            return ir;
        }
        src = temps[0];
    }
    if (IsMemRef(origdst)) {
        int off = dst->val;
        Operand *temp2;
        dst = (Operand *)dst->name;
        // the "STRING_DEF" case is missed above on p2, where
        // we can deliberately refer to memory; but mem-mem moves
        // do not work out well
        if (src->kind == IMM_INT || SrcOnlyHwReg(src) || (off && src == dst) || src->kind == STRING_DEF) {
            temp2 = NewFunctionTempRegister();
            EmitMove(irl, temp2, src);
            src = temp2;
        }
        if (off) {
            if (dst->kind == IMM_INT) {
                dst->val += off;
            } else {
                EmitAddSub(irl, dst, off);
            }
        }
        if (origdst->kind == COGMEM_REF) {
            if (num_tmp_regs != 1) {
                ERROR(NULL, "Cannot handle multiple COG writes yet");
                return ir;
            }
            ir = EmitCogwrite(irl, src, dst);
        } else if (origdst->kind == HUBMEM_REF) {
            int size = origdst->size;
            if (size == 4) {
                ir = EmitOp2(irl, OPC_WRLONG, src, dst);
            } else if (size == 2) {
                ir = EmitOp2(irl, OPC_WRWORD, src, dst);
            } else if (size == 1) {
                ir = EmitOp2(irl, OPC_WRBYTE, src, dst);
            } else {
                Operand *inc4 = NewImmediate(4);
                if (src != temps[0]) {
                    ERROR(NULL, "Cannot handle memref of size %d", size);
                    return ir;
                }
                for (i = 0; i < num_tmp_regs; i++) {
                    EmitOp2(irl, OPC_WRLONG, temps[i], dst);
                    if ( (i+1) != num_tmp_regs) {
                        EmitOp2(irl, OPC_ADD, dst, inc4);
                    }
                }
                EmitAddSub(irl, dst, -4*(num_tmp_regs-1));
            }
        } else {
            ERROR(NULL, "Illegal memory reference");
        }
        if (off && dst->kind != IMM_INT) {
            EmitAddSub(irl, dst, -off);
        }
    } else {
        if (dst != src) {
            if (dst->kind == IMM_INT) {
                ERROR(NULL, "Internal Error: emitting move to immediate");
            }
            ir = EmitOp2(irl, OPC_MOV, dst, src);
        }
    }
    return ir;
}

//
// a for loop gets a pattern like
//
//   initial code
// Ltop:
//   if (!loopcond) goto Lexit
//   loop body
// Lnext:
//   updatestmt
//   goto Ltop
// Lexit
//
// if it must execute at least once, it looks like:
//   initial code
// Ltop:
//   loop body
// Lnext
//   updatestmt
//   if (loopcond) goto Ltop
// Lexit
//

static void CompileForLoop(IRList *irl, AST *ast, int atleastonce)
{
    AST *initstmt;
    AST *loopcond;
    AST *update;
    AST *body = 0;

    Operand *toplabel, *nextlabel, *exitlabel;
    initstmt = ast->left;
    ast = ast->right;
    if (!ast || ast->kind != AST_TO) {
        ERROR(ast, "Internal Error: expected AST_TO in for loop");
        return;
    }
    loopcond = ast->left;
    ast = ast->right;
    if (!ast || ast->kind != AST_STEP) {
        ERROR(ast, "Internal Error: expected AST_TO in for loop");
        return;
    }
    update = ast->left;
    ast = ast->right;
    body = ast;
    if (initstmt) {
        EmitDebugComment(irl, initstmt);
    } else if (loopcond) {
        EmitDebugComment(irl, loopcond);
    } else if (body) {
        EmitDebugComment(irl, body);
    }
    CompileExpression(irl, initstmt, NULL);
    
    toplabel = NewCodeLabel();
    nextlabel = NewCodeLabel();
    exitlabel = NewCodeLabel();
    PushQuitNext(exitlabel, nextlabel);

    if (!loopcond) {
        loopcond = AstInteger(1);
    }
    EmitLabel(irl, toplabel);
    if (!atleastonce) {
        CompileBoolBranches(irl, loopcond, NULL, exitlabel);
    }
    CompileStatementList(irl, body);
    EmitLabel(irl, nextlabel);
    CompileStatement(irl, update);
    if (atleastonce) {
        CompileBoolBranches(irl, loopcond, toplabel, NULL);
    } else {
        EmitJump(irl, COND_TRUE, toplabel);
    }
    EmitLabel(irl, exitlabel);
    PopQuitNext();
}

static void CompileCaseStmt(IRList *irl, AST *ast)
{
    Operand *labeldone = NewCodeLabel();
    Operand *labelnext = NULL;
    AST *var;
    AST *item;
    AST *booltest;
    AST *stmts;
    int starttempreg;
    
    var = ast->left;
    if (var->kind == AST_ASSIGN) {
        CompileExpression(irl, var, NULL);
        var = var->left;
    } else if (!IsIdentifier(var)) {
        ERROR(var, "Internal error, expected identifier in case");
        return;
    }
    ast = ast->right;
    while (ast) {
        starttempreg = FuncData(curfunc)->curtempreg;
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Internal error in case list");
            return;
        }
        item = ast->left;
        ast = ast->right;
        booltest = TransformCaseExprList(var, item->left);
        stmts = item->right;
        if (labelnext) {
            EmitJump(irl, COND_TRUE, labeldone);
            EmitLabel(irl, labelnext);
        }
        labelnext = NewCodeLabel();
        CompileBoolBranches(irl, booltest, NULL, labelnext);
        FreeTempRegisters(irl, starttempreg);
        CompileStatementList(irl, stmts);
        FreeTempRegisters(irl, starttempreg);
    }
    if (labelnext) {
        EmitLabel(irl, labelnext);
    }
    EmitLabel(irl, labeldone);
}

static AST *GetResultExpr(AST *resultval)
{
    if (!resultval) return resultval;
    if (resultval->kind == AST_DECLARE_VAR) {
        resultval = resultval->right;
    }
    return resultval;
}

static void MarkUsedLabel(IRList *irl, Operand *label)
{
    label->used = 9999; // make sure label is not removed
}

static int OutAsm_DebugEval(AST *ast, int regNum, int *addr, void *ourarg) {
    IRList *irl = (IRList *)ourarg;
    Operand *srcop;
    Operand *dstop;
    if (IsConstExpr(ast)) {
        *addr = EvalConstExpr(ast);
        return PASM_EVAL_ISCONST;
    }
    srcop = CompileExpression(irl, ast, NULL);
    dstop = GetDebugReg(regNum);
    EmitMove(irl, dstop, srcop);
    *addr = debugaddr[regNum];
    return PASM_EVAL_ISREG;
}

static void CompileStatement(IRList *irl, AST *ast)
{
    AST *retval;
    Operand *op;
    Operand *botloop, *toploop;
    Operand *exitloop;
    Operand *buf;
    int starttempreg;
    AST *pendingComments = NULL;
    
    if (!ast) return;

    starttempreg = FuncData(curfunc)->curtempreg;
    switch (ast->kind) {
    case AST_COMMENTEDNODE:
        EmitComments(irl, ast->right);
        CompileStatement(irl, ast->left);
        break;
    case AST_RETURN:
        EmitDebugComment(irl, ast);
        retval = ast->left;
        if (!retval) {
            retval = GetResultExpr(curfunc->resultexpr);
        }
	if (retval) {
            // extract the return value if it's buried in a sequence
            while (retval->kind == AST_SEQUENCE) {
                if (retval->right) {
                    CompileStatement(irl, retval->left);
                    retval = retval->right;
                } else {
                    retval = retval->left;
                }
            }
            if (retval->kind == AST_FUNCCALL) {
                OperandList *oplist = CompileFunccall(irl, retval);
                int n = 0;
                while (oplist) {
                    EmitMove(irl, GetResultReg(n), oplist->op);
                    n++;
                    oplist = oplist->next;
                }
            } else if (retval) {
                int n = 0;
                int items;
                AST *retlist = NULL;
                OperandList *oplist = NULL;
                Operand *tempreg;
                if (retval->kind == AST_EXPRLIST) {
                    retlist = retval;
                }
                do {
                    if (retlist) {
                        retval = retlist->left;
                        retlist = retlist->right;
                    }
                    items = NumExprItemsOnStack(retval);
                    op = CompileExpression(irl, retval, NULL);
                    if (items <= 1) {
                        if (retlist) {
                            tempreg = NewFunctionTempRegister();
                            EmitMove(irl, tempreg, op);
                            AppendOperand(&oplist, tempreg);
                            n++;
                        } else {
                            EmitMove(irl, GetResultReg(n), op);
                            n++;
                        }
                    } else {
                        Operand *base = op;
                        Operand *derefptr;
                        int offset = 0;
                        while (items-- > 0) {
                            derefptr = ApplyArrayIndex(irl, base, NewImmediate(offset), LONG_SIZE);
                            tempreg = NewFunctionTempRegister();
                            EmitMove(irl, tempreg, derefptr);
                            offset++;
                            AppendOperand(&oplist, tempreg);
                        }
                    }
                } while(retlist);
                n = 0;
                while (oplist) {
                    EmitMove(irl, GetResultReg(n), oplist->op);
                    n++;
                    oplist = oplist->next;
                }
            }
	}
	EmitJump(irl, COND_TRUE, FuncData(curfunc)->asmreturnlabel);
	break;
    case AST_THROW:
        EmitDebugComment(irl, ast);
        retval = ast->left;
        if (!retval) {
            retval = GetResultExpr(curfunc->resultexpr);
        }
        if (!retval || retval->kind == AST_EXPRLIST) {
            // cannot abort back multiple values!
            WARNING(ast, "cannot throw multiple or no values");
            retval = AstInteger(0);
        }
        ValidateAbortFuncs();
        op = CompileExpression(irl, retval, NULL);
        // check which buffer to use
        if (ast->right) {
            buf = CompileExpression(irl, ast->right, NULL);
        } else {
            buf = abortchain;
        }
        EmitMove(irl, GetArgReg(0), buf);
        EmitMove(irl, GetArgReg(1), op);
        EmitMove(irl, GetArgReg(2), NewImmediate(ast->d.ival));
        EmitOp1(irl, OPC_CALL, longjmpfunc);
        break;
    case AST_LABEL:
        EmitDebugComment(irl, ast);
        op = GetLabelFromSymbol(ast, GetUserIdentifierName(ast->left), false);
        if (op) {
            EmitLabel(irl, op);
        }
        break;
    case AST_GOTO:
        EmitDebugComment(irl, ast);
        op = GetLabelFromSymbol(ast, GetUserIdentifierName(ast->left), false);
        if (op) {
            EmitJump(irl, COND_TRUE, op);
        }
        break;
    case AST_GOSUB:
        EmitDebugComment(irl, ast);
        ValidateGosub();
        op = GetLabelFromSymbol(ast, ast->left->d.string, false);
        if (op) {
            EmitMove(irl, GetArgReg(0), op);
            EmitOp1(irl, OPC_CALL, gosub_);
            MarkUsedLabel(irl, op);
        }
        break;
    case AST_WHILE:
        EmitDebugComment(irl, ast->left);
        toploop = NewCodeLabel();
	botloop = NewCodeLabel();
	PushQuitNext(botloop, toploop);
	EmitLabel(irl, toploop);
        CompileBoolBranches(irl, ast->left, NULL, botloop);
	FreeTempRegisters(irl, starttempreg);
        CompileStatementList(irl, ast->right);
	EmitJump(irl, COND_TRUE, toploop);
	EmitLabel(irl, botloop);
	PopQuitNext();
	break;
    case AST_DOWHILE:
        toploop = NewCodeLabel();
	botloop = NewCodeLabel();
	exitloop = NewCodeLabel();
	PushQuitNext(exitloop, botloop);
	EmitLabel(irl, toploop);
        CompileStatementList(irl, ast->right);
	EmitLabel(irl, botloop);
        CompileBoolBranches(irl, ast->left, toploop, NULL);
	FreeTempRegisters(irl, starttempreg);
	EmitLabel(irl, exitloop);
	PopQuitNext();
	break;
    case AST_TRYENV:
    {
        ValidateAbortFuncs();
        EmitPush(irl, abortchain);
        EmitMove(irl, abortchain, stackptr);
        EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(SETJMP_BUF_SIZE));
        if (ast->left && ast->left->kind != AST_STMTLIST) {
            (void)CompileExpression(irl, ast->left, NULL);
        } else {
            CompileStatementList(irl, ast->left);
        }
        EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(SETJMP_BUF_SIZE));
        EmitPop(irl, abortchain);
        break;
    }
    case AST_FORATLEASTONCE:
    case AST_FOR:
	CompileForLoop(irl, ast, ast->kind == AST_FORATLEASTONCE);
        break;
    case AST_INLINEASM:
    {
        //EmitDebugComment(irl, ast);
        unsigned flags = 0;
        if (ast->right && ast->right->kind == AST_INTEGER) {
            flags = ast->right->d.ival;
        }
        CompileInlineAsm(irl, ast->left, flags);
        break;
    }
    case AST_CASE:
        CompileCaseStmt(irl, ast);
        break;
    case AST_QUITLOOP:
    case AST_ENDCASE:
        EmitDebugComment(irl, ast);
        if (!quitlabel) {
	    ERROR(ast, "loop exit statement outside of loop");
	} else {
	    EmitJump(irl, COND_TRUE, quitlabel);
	}
	break;
    case AST_CONTINUE:
        EmitDebugComment(irl, ast);
        if (!nextlabel) {
	    ERROR(ast, "loop continue statement outside of loop");
	} else {
	    EmitJump(irl, COND_TRUE, nextlabel);
	}
	break;
    case AST_IF:
        EmitDebugComment(irl, ast->left);
        toploop = NewCodeLabel();
        CompileBoolBranches(irl, ast->left, NULL, toploop);
	FreeTempRegisters(irl, starttempreg);
	ast = ast->right;
	if (ast->kind == AST_COMMENTEDNODE) {
            pendingComments = ast->right;
            ast = ast->left;
	}
	/* ast should be an AST_THENELSE */
	CompileStatementList(irl, ast->left);
	if (ast->right) {
            EmitComments(irl, pendingComments);
            botloop = NewCodeLabel();
            EmitJump(irl, COND_TRUE, botloop);
            EmitLabel(irl, toploop);
            CompileStatementList(irl, ast->right);
            EmitLabel(irl, botloop);
	} else {
            EmitLabel(irl, toploop);
	}
	break;
    case AST_YIELD:
	/* do nothing in assembly for YIELD */
        break;
    case AST_SCOPE:
        ast = ast->left;
        /* fall through */
    case AST_STMTLIST:
        CompileStatementList(irl, ast);
        break;
    case AST_JUMPTABLE:
    {
        Operand *switchval;
        Operand *op;
        Operand *jumptab;
        Operand *jumptabptr;
        Operand *shift;
        IR *ir;
        int needAlign = 0;
        
        if (curfunc && InCog(curfunc)) {
            shift = 0;
        } else if (gl_p2) {
            shift = NewImmediate(2);
        } else {
            shift = NewImmediate(1);
            needAlign = 1;
        }
        switchval = CompileExpression(irl, ast->left, NULL);
        jumptab = NewCodeLabel();
        if (gl_p2) {
            EmitOp1(irl, OPC_JMPREL, switchval);
        } else {
            jumptabptr = NewImmediatePtr(NULL, jumptab);
            if (shift) {
                EmitOp2(irl, OPC_SHL, switchval, shift);
                EmitOp2(irl, OPC_ADD, switchval, jumptabptr);
                EmitOp2(irl, OPC_RDWORD, switchval, switchval);
                EmitJump(irl, COND_TRUE, switchval);
            } else {
                // in COG memory
                EmitOp2(irl, OPC_ADD, switchval, jumptabptr);
                EmitJump(irl, COND_TRUE, switchval);
            }
        }
        ir = EmitLabel(irl, jumptab);
        ir->flags |= FLAG_KEEP_INSTR;
        ast = ast->right;
        while (ast->kind == AST_LISTHOLDER) {
            op = GetLabelFromSymbol(ast, ast->left->d.string, false);
            if (op) {
                ir = EmitJump(irl, COND_TRUE, op);
                ir->flags |= (FLAG_KEEP_INSTR | FLAG_JMPTABLE_INSTR);
            }
            ast = ast->right;
        }
        if (needAlign) {
            EmitOp0(irl, OPC_ALIGNL);
        }
        if (ast->kind != AST_STMTLIST) {
            ERROR(ast, "Expected statement list!");
            break;
        }
        CompileStatementList(irl, ast);
        break;
    }
    case AST_BRKDEBUG: {
        if (!gl_p2) ERROR(ast, "BRK debug is only supported on P2");
        int brkCode = AsmDebug_CodeGen(ast, OutAsm_DebugEval, (void *)irl);
        if (brkCode >= 0) {
            Operand *op = NewImmediate(brkCode);
            IR *ir = EmitOp1(irl, OPC_BREAK, op);
            ir->flags |= FLAG_KEEP_INSTR;
        }
    } break;
    case AST_ASSIGN:
        /* fall through */
    default:
        /* assume an expression */
        EmitDebugComment(irl, ast);
        (void)CompileExpression(irl, ast, NULL);
        break;
    }
    FreeTempRegisters(irl, starttempreg);
}

/*
 * compile just the body of a function, and put it
 * in an IRL for that function
 */
static void
CompileFunctionBody(Function *f)
{
    IRList *irl = FuncIRL(f);
    IRList *irheader = &FuncData(f)->irheader;

    EmitComments(irheader, f->doccomment);
    
    nextlabel = quitlabel = NULL;

    // check for __fromfile
    if (f->body && f->body->kind == AST_STRING) {
        return;
    }
    EmitFunctionProlog(irl, f);
    // emit initializations if any required
    if (f->resultexpr && !IsConstExpr(f->resultexpr))
    {
        AST *init = GetResultExpr(f->resultexpr);
        if (init) {
            AST *zero = AstInteger(0);
            if (IsIdentifier(init) || init->kind == AST_RESULT) {
                AST *resinit = AstAssign(init, zero);
                CompileStatement(irl, resinit);
            } else if (init->kind == AST_EXPRLIST) {
                AST *var;
                
                while (init && init->kind == AST_EXPRLIST) {
                    var = init->left;
                    init = init->right;
                    if (var && (IsIdentifier(var) || var->kind == AST_RESULT)) {
                        CompileStatement(irl, AstAssign(var, zero));
                    }
                }
            }
        }
    }
    CompileStatementList(irl, f->body);
    EmitFunctionEpilog(irl, f);
    OptimizeIRLocal(irl, f);
}

/*
 * compile a function to IR and put it at the end of the IRList
 * called late (after all optimizations have been finished)
 */

static void
CompileWholeFunction(IRList *irl, Function *f)
{
    IRList *firl = FuncIRL(f);
    if (FuncData(f)->firl_done) {
        ERROR(NULL, "firl done");
    }
    EmitFunctionHeader(irl, f);
    AppendIRList(irl, firl);
    EmitFunctionFooter(irl, f);
    FuncData(f)->firl_done = 1;
}

static Operand *newlineOp;
static void EmitNewline(IRList *irl)
{
    IR *ir;

    if (irl) {
        ir = NewIR(OPC_LITERAL);
        ir->dst = newlineOp;
        AppendIR(irl, ir);
    }
}

static int gcmpfunc(const void *a, const void *b)
{
  const AsmVariable *ga = (const AsmVariable *)a;
  const AsmVariable *gb = (const AsmVariable *)b;
  return strcmp(ga->op->name, gb->op->name);
}

#define SORT_ALPHABETICALLY 1
#define NO_SORT 0

// returns count of bytes emitted
// if datairl or bssirl is NULL, nothing is actually output
static int EmitAsmVars(struct flexbuf *fb, IRList *datairl, IRList *bssirl, int flags)
{
    size_t siz = flexbuf_curlen(fb) / sizeof(AsmVariable);
    size_t i;
    AsmVariable *g = (AsmVariable *)flexbuf_peek(fb);
    int varsize;
    int alphaSort = flags & SORT_ALPHABETICALLY;
    int count = 0;
    
    if (siz > 0) {
      EmitNewline(datairl);
    }
    /* sort the global variables */
    if (alphaSort) {
        qsort(g, siz, sizeof(*g), gcmpfunc);
    }
    for (i = 0; i < siz; i++) {
      if (g[i].op->kind == REG_LOCAL && !g[i].op->used) {
	continue;
      }
      if (g[i].op->kind == REG_TEMP && !g[i].op->used) {
	continue;
      }
      if (g[i].op->kind == IMM_INT && !g[i].op->used) {
	continue;
      }
      if (g[i].op->kind == REG_HW) {
	continue;
      }
      switch(g[i].op->kind) {
      case STRING_DEF:
          EmitLabel(datairl, g[i].op);
          count += EmitString(datairl, (AST *)g[i].val);
          break;
      case IMM_COG_LABEL:
      case IMM_HUB_LABEL:
      case REG_HUBPTR:
      case REG_COGPTR:
          EmitLabel(datairl, g[i].op);
          count += EmitLongPtr(datairl, (Operand *)g[i].val);
          break;
      case IMM_INT:
          EmitLabel(datairl, g[i].op);
          count += EmitLong(datairl, g[i].val);
          break;
      case REG_ARG:
      case REG_LOCAL:
      case REG_TEMP:
	  if (bssirl != NULL) {
   	      EmitLabel(bssirl, g[i].op);
	      varsize = g[i].count / LONG_SIZE;
	      if (varsize == 0) varsize = 1;
              if (varsize > 1) {
                  int n;
                  char *label;
                  EmitReserve(bssirl, 1, COG_RESERVE);
                  for (n = 1; n < varsize; n++) {
                      label = OffsetName(g[i].op->name, n);
                      EmitNamedCogLabel(bssirl, label);
                      EmitReserve(bssirl, 1, COG_RESERVE);
                  }
              } else {                  
                  EmitReserve(bssirl, varsize, COG_RESERVE);
              }
              count += varsize * 4;
	      break;
	  }
	  // otherwise fall through
      default:
          EmitLabel(datairl, g[i].op);
          varsize = g[i].count / LONG_SIZE;
          if (varsize <= 1) {
              count += EmitLong(datairl, g[i].val);
          } else {
              count += varsize * 4;
              if (datairl) {
                  /* normally ir->src is NULL for OPC_LONG, but in this
                     case (an array definition) it is a count */
                  EmitOp2(datairl, OPC_LONG, NewOperand(IMM_INT, "", 0),
                          NewOperand(IMM_INT, "", varsize));
              }
          }
          break;
      }
    }
    return count;
}

static void EmitGlobals(IRList *cogdata, IRList *cogbss, IRList *hubdata)
{
    EmitAsmVars(&cogGlobalVars, cogdata, cogbss, SORT_ALPHABETICALLY);
    EmitAsmVars(&hubGlobalVars, hubdata, NULL, NO_SORT);
}

//
// return true if a function will be removed if inlined
//
bool
RemoveIfInlined(Function *f)
{
    if (f->cog_task)
        return false;
    if (f->used_as_ptr)
        return false;
    if (ShouldSkipFunction(f))
        return true;
    if (!f->is_public || (gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS))
        return true;
    return false;
}

// return true if the function was actually inlined
static bool
ActuallyInlined(Function *f)
{
    if (!FuncData(f)->isInline) {
        return false;
    }
    if (FuncData(f)->actual_callsites > 0) {
        return false;
    }
    if (!f->callSites) {
        return false;
    }
    return true;
}

// assign one function name
static void
AssignOneFuncName(Function *f)
{
    const char *fname;
    char *frname;
    char *faltname;
    char *fentername = NULL;
    Module *P = f->module;
    Function *savecur = curfunc;
    
    if (f->bedata) {
        // already set up
        return;
    }
    if (!P->bedata) {
        P->bedata = calloc(sizeof(AsmModData), 1);
    }
    curfunc = f;
    {
        // at one time we also forced the global module stuff into
        // COG memory, so we still have the capability to put code
        // into COG on a per-function basis; may be useful later
        if (f->code_placement == CODE_PLACE_DEFAULT) {
            if (COG_CODE) {
                f->code_placement = CODE_PLACE_COG;
            } else {
                f->code_placement = CODE_PLACE_HUB;
            }
        }
        fname = IdentifierModuleName(P, f->name);
	frname = (char *)malloc(strlen(fname) + 5);
	sprintf(frname, "%s_ret", fname);
        if (gl_output == OUTPUT_COGSPIN && InCog(f) && f->is_public) {
            faltname = (char *)malloc(strlen(fname) + 6);
            sprintf(faltname, "pasm%s", fname);
        } else {
            faltname = NULL;
        }
        if (f->is_recursive) {
            fentername = (char *)malloc(strlen(fname) + 16);
            sprintf(fentername, "%s_enter", fname);
        }
        f->bedata = calloc(1, sizeof(IRFuncData));

        // figure out calling convention
        if (0 && f->local_address_taken) {
            FuncData(f)->convention = STACK_CALL;
        } else if (InCog(f)) {
            FuncData(f)->convention = FAST_CALL;
        } else {
            FuncData(f)->convention = FAST_CALL; // default
        }

        if (InCog(f)) {
            FuncData(f)->asmname = NewOperand(IMM_COG_LABEL, fname, 0);
            FuncData(f)->asmretname = NewOperand(IMM_COG_LABEL, frname, 0);
            if (fentername) {
                FuncData(f)->asmentername = NewOperand(IMM_COG_LABEL, fentername, 0);
            }
        } else {
            FuncData(f)->asmname = NewOperand(IMM_HUB_LABEL, fname, 0);
            FuncData(f)->asmretname = NewOperand(IMM_HUB_LABEL, frname, 0);
            if (fentername) {
                FuncData(f)->asmentername = NewOperand(IMM_HUB_LABEL, fentername, 0);
            }
        }
        if (f->is_recursive || ANY_VARS_ON_STACK(f)) {
            f->no_inline = 1; // cannot inline these, they need special setup/shutdown
        }
        if (NeedToSaveLocals(f) || ANY_VARS_ON_STACK(f)) {
            FuncData(f)->asmreturnlabel = NewCodeLabel();
        } else {
            FuncData(f)->asmreturnlabel = FuncData(f)->asmretname;
        }
        if (faltname) {
            FuncData(f)->asmaltname = NewOperand(IMM_COG_LABEL, faltname, 0);
        }
        if (f->extradecl) {
            // this case should be handled at an earlier level
            ERROR(f->extradecl, "Unexpected data while assembling");
        }
    }
    curfunc = savecur;
}

// assign all function names so we can do forward calls
// this is also where we can allocate the back end data
static int FixupTypes(Symbol *sym, void *arg)
{
    Label *lab;
    
    switch (sym->kind) {
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
    case SYM_VARIABLE:
    case SYM_PARAMETER:
    case SYM_RESULT:
        sym->val = (void *)CleanupType((AST *)sym->val);
        break;
    case SYM_LABEL:
        lab = (Label *)sym->val;
        lab->type = CleanupType(lab->type);
        break;
    default:
        break;
    }
    return 1;
}

// we also resolve type sizes here
static int
AssignFuncNames(void *vptr, Module *P)
{
    Function *f;
    
    for(f = P->functions; f; f = f->next) {
        if (ShouldSkipFunction(f))
            continue;
        AssignOneFuncName(f);
        IterateOverSymbols(&f->localsyms, FixupTypes, (void *)0);
    }
    IterateOverSymbols(&P->objsyms, FixupTypes, (void *)0);
    return 0;
}

static int
CompileFunc_internal(void *vptr, Module *P)
{
    Function *savecurf = curfunc;
    Function *f;
    
    for(f = P->functions; f; f = f->next) {
      if (ShouldSkipFunction(f))
          continue;
      curfunc = f;
      CompileFunctionBody(f);
      FuncData(f)->isInline = ShouldBeInlined(f);
    }
    curfunc = savecurf;
    return 0;
}

static int
ExpandInline_internal(void *vptr, Module *P)
{
    Function *f;
    int change;
    int newInlines;
    int inlineStatus;
    int anyChange = 0;
    
    do {
        newInlines = 0;
        for (f = P->functions; f; f = f->next) {
            IRList *firl = FuncIRL(f);
            if (ShouldSkipFunction(f))
                continue;
            curfunc = f;
            for(;;) {
                change = ExpandInlines(firl);
                if (!change) break;
                // may be new opportunities for optimization
                OptimizeIRLocal(firl, f);
                // revisit the question of whether it should be inlined, given that
                // we've perhaps changed its size
                if (!FuncData(f)->isInline) {
                    inlineStatus = ShouldBeInlined(f);
                    if (inlineStatus) {
                        newInlines++;
                        FuncData(f)->isInline = true;
                    }
                }
            }
        }
        anyChange |= newInlines;
    } while (newInlines);
    return anyChange;
}

void
CompileIntermediate(Module *P)
{
    int change;
    InitAsmCode();
    
    VisitRecursive(NULL, P, AssignFuncNames, VISITFLAG_FUNCNAMES);
    VisitRecursive(NULL, P, CompileFunc_internal, VISITFLAG_COMPILEFUNCS);
    do {
        change = VisitRecursive(NULL, P, ExpandInline_internal, VISITFLAG_EXPANDINLINE);
    } while (change);
}

static int
CompileToIR_internal(void *vptr, Module *P)
{
    IRList *irl = (IRList *)vptr;
    Function *f;
    Function *save = curfunc;
    int docog;

    if (P->visitFlag == VISITFLAG_COMPILEIR_COG) {
        docog = CODE_PLACE_COG;
    } else if (P->visitFlag == VISITFLAG_COMPILEIR_LUT) {
        docog = CODE_PLACE_LUT;
    } else {
        docog = CODE_PLACE_HUB;
    }
    
    // emit output for P
    for(f = P->functions; f; f = f->next) {
        // if the function was private and has
        // been inlined, skip it
        if (ShouldSkipFunction(f)) {
            continue;
        }
        if (RemoveIfInlined(f) && ActuallyInlined(f)) {
            continue;
        }
        // only do cog or hub
        if (f->code_placement != docog) {
            continue;
        }
        curfunc = f;
	EmitNewline(irl);
        CompileWholeFunction(irl, f);
    }
    curfunc = save;
    return 0;
}

bool
CompileToIR_cog(IRList *irl, Module *P)
{
    // and generate real output
    VisitRecursive(irl, P, CompileToIR_internal, VISITFLAG_COMPILEIR_COG);
    
    return gl_errors == 0;
}
bool
CompileToIR_lut(IRList *irl, Module *P)
{
    if (gl_have_lut) {
        // and generate real output
        VisitRecursive(irl, P, CompileToIR_internal, VISITFLAG_COMPILEIR_LUT);
    }
    return gl_errors == 0;
}
bool
CompileToIR_hub(IRList *irl, Module *P)
{
    // and generate real output
    VisitRecursive(irl, P, CompileToIR_internal, VISITFLAG_COMPILEIR_HUB);
    
    return gl_errors == 0;
}
/*
 * emit builtin functions like mul and div
 */
static const char *builtin_mul_p1 =
"\nmultiply_\n"
"\tmov\titmp2_, muldiva_\n"
"\txor\titmp2_, muldivb_\n"
"\tabs\tmuldiva_, muldiva_\n"
"\tabs\tmuldivb_, muldivb_\n"
"\tjmp\t#do_multiply_\n"    
"\nunsmultiply_\n"
"\tmov\titmp2_, #0\n"
"do_multiply_\n"
"\tmov\tresult1, #0\n"
"\tmov\titmp1_, #32\n"
"\tshr\tmuldiva_, #1 wc\n"
"mul_lp_\n"
" if_c\tadd\tresult1, muldivb_ wc\n"
"\trcr\tresult1, #1 wc\n"
"\trcr\tmuldiva_, #1 wc\n"
"\tdjnz\titmp1_, #mul_lp_\n"

"\tshr\titmp2_, #31 wz\n"
"\tnegnz\tmuldivb_, result1\n"
" if_nz\tneg\tmuldiva_, muldiva_ wz\n"
" if_nz\tsub\tmuldivb_, #1\n"
"multiply__ret\n"
"unsmultiply__ret\n"
"\tret\n"
;

static const char *builtin_mul_p1_fast =
"\nmultiply_\n"
"       mov    itmp2_, muldiva_\n"
"       xor    itmp2_, muldivb_\n"
"       abs    muldiva_, muldiva_\n"
"       abs    muldivb_, muldivb_\n"
"       jmp    #do_multiply_\n"
"unsmultiply_\n"
"       mov    itmp2_, #0\n"
"do_multiply_\n"
"	mov    result1, #0\n"
"mul_lp_\n"
"	shr    muldivb_, #1 wc,wz\n"
" if_c	add    result1, muldiva_\n"
"	shl    muldiva_, #1\n"
" if_ne	jmp    #mul_lp_\n"
"       shr    itmp2_, #31 wz\n"
"       negnz  muldiva_, result1\n"
"multiply__ret\n"
"unsmultiply__ret\n"
"	ret\n"
;

static const char *builtin_mul_p2 =
"\nunsmultiply_\n"
"\tqmul\tmuldiva_, muldivb_\n"
"\tgetqx\tmuldiva_\n"
"\tgetqy\tmuldivb_\n"
"\tret\n"

"\nmultiply_\n"
"\tqmul\tmuldiva_, muldivb_\n"
"\tmov\titmp2_, #0\n"
"\tcmps\tmuldiva_, #0 wc\n"
"  if_c\tadd\titmp2_, muldivb_\n"
"\tcmps\tmuldivb_, #0 wc\n"
"  if_c\tadd\titmp2_, muldiva_\n"
"\tgetqx\tmuldiva_\n"
"\tgetqy\tmuldivb_\n"
"\tsub\tmuldivb_, itmp2_\n"
"\tret\n"
;

/*
 * signed divide, taken from spin interpreter
 * calculates muldiva_ / muldivb_
 * quotient in muldivb_, remainder in muldiva_
 */

static const char *builtin_div_p1 =
"' code originally from spin interpreter, modified slightly\n"
"\nunsdivide_\n"
"       mov     itmp2_,#0\n"
"       jmp     #udiv__\n"
"\ndivide_\n"
"       abs     muldiva_,muldiva_     wc       'abs(x)\n"
"       muxc    itmp2_,divide_haxx_            'store sign of x (mov x,#1 has bits 0 and 31 set)\n"
"       abs     muldivb_,muldivb_     wc,wz    'abs(y)\n"
" if_z  jmp     #divbyzero__\n"
" if_c  xor     itmp2_,#1                      'store sign of y\n"
"udiv__\n"
"divide_haxx_\n"
"        mov     itmp1_,#1                    'unsigned divide (bit 0 is discarded)\n"
"        mov     DIVCNT,#32\n"
"mdiv__\n"
"        shr     muldivb_,#1        wc,wz\n"
"        rcr     itmp1_,#1\n"
" if_nz   djnz    DIVCNT,#mdiv__\n"
"mdiv2__\n"
"        cmpsub  muldiva_,itmp1_        wc\n"
"        rcl     muldivb_,#1\n"
"        shr     itmp1_,#1\n"
"        djnz    DIVCNT,#mdiv2__\n"

"        shr     itmp2_,#31       wc,wz    'restore sign\n"
"        negnz   muldiva_,muldiva_         'remainder\n"
"        negc    muldivb_,muldivb_ wz      'division result\n"

"divbyzero__\n"
"divide__ret\n"
"unsdivide__ret\n"
    "\tret\n"
"DIVCNT\n"
"\tlong\t0\n"
;
static const char *builtin_div_p2 = 
"\ndivide_\n"
"       abs     muldivb_,muldivb_     wcz      'abs(y)\n"
"       wrc     itmp2_                         'store sign of y\n"
"       abs     muldiva_,muldiva_     wc       'abs(x)\n"
"       qdiv    muldiva_, muldivb_             'queue divide\n"
" if_c  xor     itmp2_,#1                      'store sign of x\n"
"       getqx   muldivb_                       'get quotient\n"
"       getqy   muldiva_                       'get remainder\n"
"       negc    muldiva_,muldiva_              'restore sign, remainder (sign of x)\n"
"       testb   itmp2_,#0             wc       'restore sign, division result\n"
" _ret_ negc    muldivb_,muldivb_     \n"

;

#include "sys/lmm_orig.spin.h"
#include "sys/lmm_slow.spin.h"
#include "sys/lmm_trace.spin.h"
#include "sys/lmm_cache.spin.h"
#include "sys/lmm_compress.spin.h"

const char *builtin_fcache_p2 =
    "FCACHE_LOAD_\n"
    "    pop\tfcache_tmpb_\n"
    "    add\tfcache_tmpb_, pa\n"
    "    push\tfcache_tmpb_\n"
    "    sub\tfcache_tmpb_, pa\n"
    "    shr\tpa, #2\n" // convert to words
    "    altd\tpa\n"
    "    mov\t 0-0, ret_instr_\n" 
    "    sub\tpa, #1\n"
    "    setq\tpa\n"
    "    rdlong\t$0, fcache_tmpb_\n"
    "    jmp\t#\\$0 ' jmp to cache\n"
    "ret_instr_\n"
    "    ret\n"
    "fcache_tmpb_\n"
    "    long 0\n"
    ;

//
// pushregs sets up a frame
// the frame pointer is the last thing pushed before the new frame
// pointer is established
// so when sp == fp the stack looks like:
//    oldfp <- top of stack
//    count <- next thing on stack, count of locals to pop (may be 0)
//    locals <- 1..count locals

const char *builtin_pushregs_p1 = 
    "COUNT_\n"
    "    long 0\n"
    "prcnt_\n"
    "    long 0\n"
    "pushregs_\n"
    "      movd  :write, #local01\n"
    "      mov   prcnt_, COUNT_ wz\n"
    "  if_z jmp  #pushregs_done_\n"
    ":write\n"
    "      wrlong 0-0, sp\n"
    "      add    :write, inc_dest1\n"
    "      add    sp, #4\n"
    "      djnz   prcnt_, #:write\n"
    "pushregs_done_\n"
    "      wrlong COUNT_, sp\n"
    "      add    sp, #4\n"
    "      wrlong fp, sp\n"
    "      add    sp, #4\n"
    "      mov    fp, sp\n"
    "pushregs__ret\n"
    "      ret\n"
    "popregs_\n"
    "      sub   sp, #4\n"
    "      rdlong fp, sp\n"
    "      sub   sp, #4\n"
    "      rdlong COUNT_, sp wz\n"
    "  if_z jmp  #popregs__ret\n"
    "      add   COUNT_, #local01\n"
    "      movd  :read, COUNT_\n"
    "      sub   COUNT_, #local01\n"
    ":loop\n"
    "      sub    :read, inc_dest1\n"
    "      sub    sp, #4\n"
    ":read\n"
    "      rdlong 0-0, sp\n"
    "      djnz   COUNT_, #:loop\n"
    "popregs__ret\n"
    "      ret\n"
    ;

const char *builtin_gosub_p1 =
    "gosub_\n"
    "      wrlong pc, sp\n"
    "      add    sp, #4\n"
    "      mov    COUNT_, #0\n"
    "      call   #pushregs_\n"
    "      mov    pc, arg01\n"
    "      jmp    #LMM_LOOP\n"
    "gosub__ret\n"
    "      ret\n"
    ;

const char *builtin_pushregs_p2 =
    "COUNT_\n"
    "    long 0\n"
    "RETADDR_\n"
    "    long 0\n"
    "fp\n"
    "    long 0\n"
    "pushregs_\n"
    "    pop  pa\n"
    "    pop  RETADDR_\n"
    "    tjz  COUNT_, #pushregs_done_\n"
    "    altd  COUNT_, #511\n"  // adjust for setq/wrlong
    "    setq #0-0\n"
    "    wrlong local01, ptra++\n"
    "pushregs_done_\n"
    "    setq #2 ' push 3 registers starting at COUNT_\n"
    "    wrlong COUNT_, ptra++\n"
    "    mov    fp, ptra\n"
    "    jmp  pa\n"

    " popregs_\n"
    "    pop    pa\n"
    "    setq   #2\n"
    "    rdlong COUNT_, --ptra\n"
    "    djf    COUNT_, #popregs__ret\n"
    "    setq   COUNT_\n"
    "    rdlong local01, --ptra\n"
    "popregs__ret\n"
    "    push   RETADDR_\n"
    "    jmp    pa\n"
    ;

const char *builtin_gosub_p2 =
    "gosub_\n"
    "    mov  COUNT_, #0\n"
    "    mov  pa, arg01\n"
    "    pop  RETADDR_\n"
    "    jmp  #pushregs_done_\n"
    ;

// ptr = arg01, val = arg02, count = arg03
const char *builtin_memfill_p2 =
    "builtin_bytefill_\n"
    "        shr	arg03, #1 wc\n"
    " if_c   wrbyte	arg02, arg01\n"
    " if_c   add	arg01, #1\n"
    "        movbyts	arg02, #0\n"
    "builtin_wordfill_\n"
    "        shr	arg03, #1 wc\n"
    " if_c   wrword	arg02, arg01\n"
    " if_c   add	arg01, #2\n"
    "        setword	arg02, arg02, #1\n"
    "builtin_longfill_\n"
    "        wrfast	#0,arg01\n"
    "        cmp	arg03, #0 wz\n"
    " if_nz  rep	#1, arg03\n"
    " if_nz  wflong	arg02\n"
    "        ret\n"
    ;

/* WARNING: make sure to increase SETJMP_BUF_SIZE if you add
 * more things to be saved in abort/catch
 */
static const char *builtin_abortcode_p1 =
    "__setjmp\n"
    "    mov result1, #0\n"
    "    mov result2, #0\n"
    "    mov abortchain, arg01\n"
    "    wrlong fp, arg01\n"
    "    add arg01, #4\n"
    "    wrlong pc, arg01\n"
    "    add arg01, #4\n"
    "    wrlong sp, arg01\n"
    "    add arg01, #4\n"
    "    wrlong objptr, arg01\n"
    "    add arg01, #4\n"
    "    wrlong __setjmp_ret, arg01\n"
    "__setjmp_ret\n"
    "    ret\n"
    // unwind_stack(curfp, lastfp) walks the frame pointer list and restores everything
    // until it reaches lastfp
    "__unwind_stack\n"
    "   cmp  arg01, arg02 wz\n"
    "  if_z jmp #__unwind_stack_ret\n"
    "   mov   sp, arg01\n"
    "   call  #popregs_\n"
    "   mov   arg01, fp\n"
    "   jmp   #__unwind_stack\n"
    "__unwind_stack_ret\n"
    "   ret\n"

    // __longjmp(buf, n) should jump to buf and return n
    "__longjmp\n"
    "    cmp    arg01, #0 wz\n"
    " if_z jmp #nocatch\n"
    "    mov result1, arg02\n"
    "    mov result2, #1\n"
    "    rdlong arg02, arg01\n"  // new target for fp
    "    add arg01, #4\n"
    "    rdlong pc, arg01\n"
    "    add arg01, #4\n"
    "    rdlong sp, arg01\n"
    "    add arg01, #4\n"
    "    rdlong objptr, arg01\n"
    "    add arg01, #4\n"
    "    rdlong __longjmp_ret, arg01\n"
    "    mov arg01, fp\n"
    "    call #__unwind_stack\n"
    "__longjmp_ret\n"
    "    ret\n"
    "nocatch\n"
    "    cmp arg03, #0 wz\n"
    " if_z jmp #cogexit\n"
    "    jmp #__longjmp_ret\n"
    ;

static const char *builtin_abortcode_p2 =
    "__pc long 0\n"
    "__setjmp\n"
    "    pop __pc\n"
    "    mov result1, #0\n"
    "    mov result2, #0\n"
    "    mov abortchain, arg01\n"
    "    wrlong fp, arg01\n"
    "    add arg01, #4\n"
    "    wrlong ptra, arg01\n"
    "    add arg01, #4\n"
    "    wrlong objptr, arg01\n"
    "    add arg01, #4\n"
    "    wrlong __pc, arg01\n"
    "    jmp __pc\n"

    // unwind_stack(curfp, lastfp) walks the frame pointer list and restores everything
    // NOTE: call this with "call", not "calla"!
    // until it reaches lastfp
    "__unwind_pc long 0\n"
    "__unwind_stack\n"
    "   pop  __unwind_pc\n"
    "__unwind_loop\n"
    "   cmp  arg01, arg02 wz\n"
    "  if_z jmp #__unwind_stack_ret\n"
    "   mov   ptra, arg01\n"
    "   call  #popregs_\n"
    "   mov   arg01, fp\n"
    "   jmp   #__unwind_loop\n"
    "__unwind_stack_ret\n"
    "   jmp  __unwind_pc\n"

    // __longjmp(buf, n) should jump to buf and return n
    "__longjmp\n"
    "    pop __pc\n"
    "    cmp    arg01, #0 wz\n"
    " if_z jmp #nocatch\n"
    "    mov result1, arg02\n"
    "    mov result2, #1\n"
    "    rdlong arg02, arg01\n"  // new target for fp
    "    add arg01, #4\n"
    "    rdlong ptra, arg01\n"
    "    add arg01, #4\n"
    "    rdlong objptr, arg01\n"
    "    add arg01, #4\n"
    "    rdlong __pc, arg01\n"
    "    mov arg01, fp\n"
    "    call #__unwind_stack\n"
    "__longjmp_ret\n"
    "    jmp  __pc\n"
    "nocatch\n"
    "    cmp arg03, #0 wz\n"
    " if_z jmp #cogexit\n"
    "    jmp #__longjmp_ret\n"
    ;

const char *builtin_wrcog =
    "wrcog\n"
    "    mov    0-0, 0-0\n"
    "wrcog_ret\n"
    "    ret\n"
    ;

static void
EmitBuiltins(IRList *irl)
{
    if (HUB_CODE) {
        const char *builtin_lmm = NULL;
        if (gl_p2) {
            if (gl_fcache_size > 0) {
                builtin_lmm = builtin_fcache_p2;
            }
        } else {
            switch(gl_lmm_kind) {
            case LMM_KIND_SLOW:
                builtin_lmm = (const char *)sys_lmm_slow_spin;
                break;
            case LMM_KIND_TRACE:
                builtin_lmm = (const char *)sys_lmm_trace_spin;
                break;
            case LMM_KIND_CACHE:
                builtin_lmm = (const char *)sys_lmm_cache_spin;
                break;
            case LMM_KIND_COMPRESS:
                builtin_lmm = (const char *)sys_lmm_compress_spin;
                break;
            default:
                builtin_lmm = (const char *)sys_lmm_orig_spin;
                break;
            }
        }
        if (builtin_lmm) {
            Operand *loop;
            loop = NewOperand(IMM_STRING, builtin_lmm, 0);
            EmitOp1(irl, OPC_LITERAL, loop);
        }
    }
    if (gl_p2) {
        Operand *memfill = NewOperand(IMM_STRING, builtin_memfill_p2, 0);
        EmitOp1(irl, OPC_LITERAL, memfill);
    }
    if (pushregs_) {
        const char *builtin_pushregs = (gl_p2 ? builtin_pushregs_p2 : builtin_pushregs_p1);
        Operand *loop = NewOperand(IMM_STRING, builtin_pushregs, 0);
        EmitOp1(irl, OPC_LITERAL, loop);
    }
    if (gosub_) {
        const char *builtin_data = (gl_p2 ? builtin_gosub_p2 : builtin_gosub_p1);
        Operand *loop = NewOperand(IMM_STRING, builtin_data, 0);
        EmitOp1(irl, OPC_LITERAL, loop);
    }
    if (mulfunc) {
        Operand *loop;

        if (gl_p2) {
            loop = NewOperand(IMM_STRING, builtin_mul_p2, 0);
        } else if (g_NeedMulHi) {
            loop = NewOperand(IMM_STRING, builtin_mul_p1, 0);
        } else {
            loop = NewOperand(IMM_STRING, builtin_mul_p1_fast, 0);
        }
        EmitOp1(irl, OPC_LITERAL, loop);
        (void)GetOneGlobal(REG_REG, "itmp1_", 0);
        (void)GetOneGlobal(REG_REG, "itmp2_", 0);
        (void)GetOneGlobal(REG_REG, "result1", 0);
    }
    if (divfunc) {
        Operand *loop;

        if (gl_p2) {
            loop = NewOperand(IMM_STRING, builtin_div_p2, 0);
        } else {
            loop = NewOperand(IMM_STRING, builtin_div_p1, 0);
        }
        EmitOp1(irl, OPC_LITERAL, loop);
        (void)GetOneGlobal(REG_REG, "itmp1_", 0);
        (void)GetOneGlobal(REG_REG, "itmp2_", 0);
        (void)GetOneGlobal(REG_REG, "result1", 0);
    }
    if (longjmpfunc) {
        Operand *loop;

        if (gl_p2) {
            loop = NewOperand(IMM_STRING, builtin_abortcode_p2, 0);
        } else {
            loop = NewOperand(IMM_STRING, builtin_abortcode_p1, 0);
        }
        EmitOp1(irl, OPC_LITERAL, loop);
        if (COG_CODE) {
            // abort saves the pc, which we don't have in COG mode, so
            // add a dummy pc register
            (void)GetOneGlobal(REG_REG, "pc", 0);
        }
    }
    if (putcogreg) {
        Operand *func = NewOperand(IMM_STRING, builtin_wrcog, 0);
        EmitOp1(irl, OPC_LITERAL, func);
    }
}

static void
CompileConstant(IRList *irl, AST *ast)
{
    Symbol *sym = NULL;
    AST *expr;
    int32_t val;
    Operand *op1, *op2;
    const char *name;

    if (ast->kind == AST_SYMBOL) {
        sym = (Symbol *)ast->d.ptr;
        name = sym->user_name;
    } else {
        name = ast->d.string;
        sym = LookupSymbol(name);
    }
    if (!IsConstExpr(ast)) {
        ERROR(ast, "%s is not constant", name);
        return;
    }
    if (!sym) {
        ERROR(ast, "constant symbol %s not declared??", name);
        return;
    }
    expr = (AST *)sym->val;
    val = EvalConstExpr(expr);
    op1 = NewOperand(IMM_STRING, name, val);
    op2 = NewImmediate(val);
    EmitOp2(irl, OPC_CONST, op1, op2);
}

static void
CompileConsts(IRList *irl, AST *conblock)
{
    AST *ast, *upper;

    for (upper = conblock; upper; upper = upper->right) {
        ast = upper->left;
        if (ast->kind == AST_COMMENTEDNODE) {
            EmitComments(irl, ast->right);
            ast = ast->left;
        }
        switch (ast->kind) {
        case AST_ENUMSKIP:
            CompileConstant(irl, ast->left);
            break;
        case AST_SYMBOL:
        case AST_IDENTIFIER:
            CompileConstant(irl, ast);
            break;
        case AST_ASSIGN:
            CompileConstant(irl, ast->left);
            break;
        default:
            /* do nothing */
            break;
        }
    }
}

int
EmitDatSection(void *vptr, Module *P)
{
  IRList *irl = (IRList *)vptr;
  Flexbuf *fb;
  Flexbuf *relocs;
  Operand *op;
  IR *ir;
  
  if (!ModData(P) || !ModData(P)->datbase)
      return 0;
  fb = (Flexbuf *)calloc(1, sizeof(*fb));
  relocs = (Flexbuf *)calloc(1, sizeof(*relocs));
  flexbuf_init(fb, 32768);
  flexbuf_init(relocs, 512);
  PrintDataBlock(fb, P->datblock, NULL,relocs);
  op = NewOperand(IMM_BINARY, (const char *)fb, (intptr_t)relocs);
  ir = EmitOp2(irl, OPC_LABELED_BLOB, ModData(P)->datlabel, op);
  ir->src2 = (Operand *)P;
  return 0;
}

int
EmitVarSection(IRList *irl, Module *P)
{
    int len;
    if (!objlabel)
        return 0;
    len = P->varsize;
    len = (len+3) & ~3; // round up to long boundary
    EmitLabel(irl, objlabel);
    if (!len)
        len = 1;
    if (HUB_DATA && (gl_optimize_flags & OPT_REMOVE_HUB_BSS)) {
        EmitReserve(irl, 1, HUB_RESERVE);
    } else {
        EmitReserve(irl, len / 4, HUB_DATA ? HUB_RESERVE : COG_RESERVE);
    }
    return 0;
}

/*
 * emit a small main program
 * it looks something like:
 *
 * entry:
 *         mov arg01, par wz
 *   if_nz jmp spininit
 *         initialize stack
 *         set clock if necessary
 *         set up lock register
 *         call P->firstfunc using stackcall
 * exit:
 *         cogstop(cogid)
 * spininit:
 *         mov sp, arg01
 *         rdlong objbase, sp
 *         add sp, #4
 *         rdlong pc, sp
 *         add sp #4
 *         rdlong muldiva, sp  ' number of arguments
 *         add sp, #4
 * arglp:
 *         rdlong argN, sp
 *
 *         call result1 using stackcall
 *         jmp  #exit
 */

static void
EmitMain_P1(IRList *irl, Module *P)
{
    IR *ir;
    Function *firstfunc;
    const char *firstfuncname;
    Operand *spinlabel;
    Operand *const4 = NewImmediate(4);
    Operand *pc = NewOperand(REG_REG, "pc", 0);
    Operand *objptr = NewOperand(REG_REG, "objptr", 0);
    Operand *arg1;
    int maxArgs = max_coginit_args;
    int i;
    
    // always need at least arg1
    arg1 = GetArgReg(0);

    firstfunc = GetMainFunction(P);
    if (!firstfunc) {
        return;  // no functions at all
    }
    firstfunc->no_inline = 1; // make sure it is never inlined or removed
    firstfunc->toplevel = 1;  // does not need to save registers
    firstfunc->callSites += 1;
    firstfuncname = IdentifierModuleName(firstfunc->module, firstfunc->name);
    
    spinlabel = NewOperand(IMM_COG_LABEL, "spininit", 0);
    cogexit = NewOperand(IMM_COG_LABEL, "cogexit", 0);
    hubexit = NewOperand(IMM_HUB_LABEL, "hubexit", 0);

    ir = EmitMove(irl, arg1, GetOneGlobal(REG_HW, "par", 0));
    ir->flags |= FLAG_WZ;

    // set up to run LMM
    if (HUB_CODE) {
        ValidateStackptr();
        ValidateObjbase();
        if (maxArgs >= 0) {
            EmitJump(irl, COND_NE, spinlabel);
        }
        if ( (gl_optimize_flags & OPT_REMOVE_HUB_BSS) ) {
            EmitOp2(irl, OPC_ADD, stackptr, objbase);
        }
        if (!gl_p2) {
            // we will need local01, it is referred to in
            // the LMM code
            Operand *local1 = GetLocalReg(0, 0);
            local1->used = 1;
        }
    }

    if (InCog(firstfunc)) {
        EmitOp1(irl, OPC_CALL, NewOperand(IMM_COG_LABEL, firstfuncname, 0));
    } else {
        EmitOp1(irl, OPC_CALL, NewOperand(IMM_HUB_LABEL, firstfuncname, 0));
    }
    EmitLabel(irl, cogexit);
    EmitOp1(irl, OPC_COGID, arg1);
    EmitOp1(irl, OPC_COGSTOP, arg1);

    if (HUB_CODE && maxArgs >= 0) {
        // this is the code for coginit/cognew
        // it is not needed if maxArgs < 0
        if (COG_DATA) {
            // the stack code below will fail to compile if --data=cog
            ERROR(NULL, "The combination --code=hub --data=cog is not supported");
            return;
        }
        EmitLabel(irl, spinlabel);
        EmitMove(irl, stackptr, arg1);
        EmitMove(irl, objptr, stacktop);
        EmitOp2(irl, OPC_ADD, stackptr, const4);
        EmitMove(irl, pc, stacktop);      // get address of function
        EmitMove(irl, stacktop, hubexit); // set return address
        EmitOp2(irl, OPC_ADD, stackptr, const4);
        // now pull operands off the stack
        for (i = 0; i < maxArgs; i++) {
            EmitMove(irl, GetArgReg(i), stacktop);
            if (i == maxArgs-1) {
                int off = (maxArgs-1) * 4;
                if (off > 0) {
                    EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(off));
                }
            } else {
                EmitOp2(irl, OPC_ADD, stackptr, const4);
            }
        }
        EmitJump(irl, COND_TRUE, NewOperand(IMM_COG_LABEL, "LMM_LOOP", 0));
    }
}

static void
EmitMain_P2(IRList *irl, Module *P, Operand *lutstart)
{
    Function *firstfunc;
    const char *firstfuncname;
    IR *ir;
    Operand *spinlabel, *skip_clock_label;
    Operand *result1 = GetResultReg(0);
    Operand *arg1 = GetArgReg(0);
    Operand *pa_reg = GetOneGlobal(REG_HW, "pa", 0);
    Operand *clkfreq_addr = NewImmediate(0x14);
    Operand *clkmode_addr = NewImmediate(0x18);
    uint32_t clkmode, clkfreq;
    int maxArgs = max_coginit_args;

    firstfunc = GetMainFunction(P);
    if (!firstfunc) {
        // no functions present; this is pure ASM code
        // force gl_no_coginit to 1 so we don't emit a header
        gl_no_coginit = 1;
        return;  // no functions at all
    }
    firstfunc->no_inline = 1; // make sure it is never inlined or removed
    firstfunc->toplevel = 1;
    firstfunc->callSites += 1;
    firstfuncname = IdentifierModuleName(firstfunc->module, firstfunc->name);
    
    ValidateStackptr();
    ValidateObjbase();
    cogexit = NewOperand(IMM_COG_LABEL, "cogexit", 0);
    spinlabel = NewOperand(IMM_COG_LABEL, "spininit", 0);
    skip_clock_label = NewOperand(IMM_COG_LABEL, "skip_clock_set_", 0);
    hubexit = NewOperand(IMM_HUB_LABEL, "hubexit", 0);

    ir = EmitOp2(irl, OPC_CMP, stackptr, NewImmediate(0));
    ir->flags |= FLAG_WZ;
    EmitJump(irl, COND_NE, spinlabel);
    if ( (gl_optimize_flags & OPT_REMOVE_HUB_BSS) ) {
        EmitOp2(irl, OPC_MOV, stackptr, objbase);
        EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(current->varsize));
    } else {
        EmitMove(irl, stackptr, stacklabel);
    }
    if (!GetClkFreq(P, &clkfreq, &clkmode)) {
        clkfreq = 160000000;
        clkmode = 0x010007fb;
    }
    // set the clock if the frequency is not known
    ir = EmitOp2(irl, OPC_RDLONG, pa_reg, clkfreq_addr);
    ir->flags |= FLAG_WZ;
    EmitJump(irl, COND_NE, skip_clock_label);
    EmitOp1(irl, OPC_HUBSET, NewImmediate(0));
    EmitOp1(irl, OPC_HUBSET, NewImmediate(clkmode & ~3));
    EmitOp1(irl, OPC_WAITX, NewImmediate(200000));
    EmitMove(irl, pa_reg, NewImmediate(clkmode));
    EmitOp1(irl, OPC_HUBSET, pa_reg);
    EmitOp2(irl, OPC_WRLONG, pa_reg, clkmode_addr);
    EmitOp2(irl, OPC_WRLONG, NewImmediate(clkfreq), clkfreq_addr);
    EmitJump(irl, COND_TRUE, skip_clock_label);
    // make sure $0-$ff are free for inline assembly
    if (gl_fcache_size) {
        EmitOp1(irl, OPC_ORGF, NewImmediate(gl_fcache_size));
    } else {
        EmitOp1(irl, OPC_ORGF, NewImmediate(32));
    }
    
    EmitLabel(irl, skip_clock_label);

    // force LUT code, if any, to be loaded
    if (lutstart) {
        ir = EmitOp2(irl, OPC_MOV, pa_reg, lutstart);
        EmitOp1(irl, OPC_SETQ2, NewImmediate(255));
        EmitOp2(irl, OPC_RDLONG, NewOperand(REG_HW, "0", 0), pa_reg);
    }
    if (InCog(firstfunc)) {
        EmitOp1(irl, OPC_CALL, NewOperand(IMM_COG_LABEL, firstfuncname, 0));
    } else {
        EmitOp1(irl, OPC_CALL, NewOperand(IMM_HUB_LABEL, firstfuncname, 0));
    }
    EmitLabel(irl, cogexit);
    EmitOp1(irl, OPC_WAITX, NewImmediate(160000)); // 1 ms delay at 160 MHz
    EmitOp1(irl, OPC_COGID, arg1);
    EmitOp1(irl, OPC_COGSTOP, arg1);

    // and now the code for when we are started with Spin coginit
    // on P2, stackptr is always PTRA, so opeffects can be used
    EmitLabel(irl, spinlabel);
    EmitOp2(irl, OPC_RDLONG, objbase, stackptr)->srceffect = OPEFFECT_POSTINC;
    EmitOp2(irl, OPC_RDLONG, result1, stackptr)->srceffect = OPEFFECT_POSTINC;
    // now pull operands off the stack
    if (maxArgs > 0)  {
        for (int i=0;i<maxArgs;i++) GetArgReg(i); // Make sure the registers actually exist
        if (maxArgs > 1) EmitOp1(irl,OPC_SETQ,NewImmediate(maxArgs-1));
        EmitOp2(irl,OPC_RDLONG,GetArgReg(0),stackptr);
    }
    EmitAddSub(irl, stackptr, -4);
    EmitOp1(irl, OPC_CALL, result1);
    EmitJump(irl, COND_TRUE, cogexit);
}

/*
 * .cog.spin main program looks like:
 * 
 */

void
EmitMain_CogSpin(IRList *irl, Module *p, int maxArgs, int maxResults)
{
    IR *ir;
    Operand *par;
    Operand *const4 = NewImmediate(4);
    Operand *mboxptr = GetOneGlobal(REG_REG, "mboxptr", 0);
    Operand *mboxcmd = GetOneGlobal(REG_REG, "mboxcmd", 0);
    Operand *waitloop;
    Operand *pasm__init;
    Operand *arg1 = GetArgReg(0);
    Operand *arg2 = GetArgReg(1);
    
    int i;
    if (maxArgs > MAX_COGSPIN_ARGS) {
        ERROR(NULL, ".cog.spin functions may not have more than %d arguments", MAX_COGSPIN_ARGS);
        maxArgs = MAX_COGSPIN_ARGS;
    }
    if (gl_p2) {
        stackptr = GetOneGlobal(REG_HW, "ptra", 0);
        objbase = GetOneGlobal(REG_REG, "objptr", 0);
        par = GetOneGlobal(REG_HW, "ptra", 0);
    } else {
        stackptr = GetOneGlobal(REG_REG, "sp", 0);
        objbase = GetOneGlobal(REG_REG, "objptr", 0);
        linkreg = GetOneGlobal(REG_REG, "linkreg", 0);
        par = GetOneGlobal(REG_HW, "par", 0);
    }
    stacktop = SizedHubMemRef(LONG_SIZE, stackptr, 0);

    // mov mboxptr, par
    EmitMove(irl, mboxptr, par);
    if (gl_p2) {
        Operand *const8 = NewImmediate(8);
        EmitOp2(irl, OPC_ADD, mboxptr, const8);
        EmitOp2(irl, OPC_RDLONG, objbase, mboxptr);
        EmitOp2(irl, OPC_ADD, mboxptr, const4);
        EmitOp2(irl, OPC_RDLONG, stackptr, mboxptr);
        EmitOp2(irl, OPC_SUB, mboxptr, const8);
        EmitOp2(irl, OPC_WRLONG, NewImmediate(0), mboxptr);
    } else {
        // add mboxptr, #4 ' skip over lock
        EmitOp2(irl, OPC_ADD, mboxptr, const4);
    }
    waitloop = NewOperand(IMM_COG_LABEL, "waitloop", 0);
    EmitLabel(irl, waitloop);
    ir = EmitOp2(irl, OPC_RDLONG, mboxcmd, mboxptr);
    ir->flags |= FLAG_WZ;
    EmitJump(irl, COND_EQ, waitloop);
    for (i = 0; i < maxArgs; i++) {
        EmitOp2(irl, OPC_ADD, mboxptr, const4);
        EmitOp2(irl, OPC_RDLONG, GetArgReg(i), mboxptr);
    }
    if (maxArgs > 1) {
        EmitOp2(irl, OPC_SUB, mboxptr, NewImmediate((maxArgs-1)*4));
    } else if (maxArgs == 0) {
        EmitOp2(irl, OPC_ADD, mboxptr, const4);
    }
    // divide address by 4 to convert from cog to hub
    EmitOp2(irl, OPC_SHR, mboxcmd, NewImmediate(2));
    // call command
    if (gl_p2) {
        EmitOp1(irl, OPC_CALL, mboxcmd);
    } else {
        //EmitJump(irl, COND_TRUE, mboxcmd);
        EmitOp2(irl, OPC_JMPRET, linkreg, mboxcmd);
    }
    // write back the result
    for (i = 0; i < maxResults; i++) {
        EmitOp2(irl, OPC_WRLONG, GetResultReg(i), mboxptr);
        if (i != maxResults-1) {
            EmitOp2(irl, OPC_ADD, mboxptr, const4);
        }
    }
    EmitOp2(irl, OPC_SUB, mboxptr, NewImmediate(4*maxResults));
    EmitMove(irl, arg1, NewImmediate(0));
    EmitOp2(irl, OPC_WRLONG, arg1, mboxptr);
    EmitJump(irl, COND_TRUE, waitloop);

    if (!gl_p2) {
        pasm__init = NewOperand(IMM_COG_LABEL, "pasm__init", 0);
        EmitLabel(irl, pasm__init);
        EmitMove(irl, objbase, arg1);
        EmitMove(irl, stackptr, arg2);
        EmitJump(irl, COND_TRUE, linkreg);
    }
}

void
InitAsmCode()
{
    static int initDone = 0;

    if (initDone) return;

    newlineOp = NewOperand(IMM_STRING, "\n", 0);
    initDone = 1;
}

/* routine to try to guess how much room we need for fcache */
/* this does not work very well at present, and may be impossible
   in general if the user places functions in the same memory as
   fcache. Certainly some major re-arrangement will be necessary :(
   So for now it just punts
 */

static int
GuessFcacheSize(IRList *irl)
{
    /* for now, just go by p2/p1 */
    if (gl_p2) {
        if (gl_optimize_flags & OPT_AUTO_FCACHE) {
             return 128;
        }
        return 0; // disable fcache if no optimization
    } else {
        return 96; // default size for P1
    }
}

static void
CompileSystemModule(IRList *where, Module *M, int flag)
{
    VisitRecursive(where, M, CompileToIR_internal, flag);
}

void
OutputAsmCode(const char *fname, Module *P, int outputMain)
{
    FILE *f = NULL;
    Module *save;
    IR *orgh = NULL;
    Operand *entrylabel = NewOperand(IMM_COG_LABEL, ENTRYNAME, 0);
    Operand *lutstart = NULL;
    Operand *cog_bss_start = NewOperand(IMM_COG_LABEL, "COG_BSS_START", 0);
    bool emitSpinCode = true;
    
    const char *asmcode;
    int maxargs = 2; // initialization code wants 2 arguments
    int maxrets = 1;  // assume 1 return value is default
    
    save = current;
    current = P;

    if (!P->functions) {
        // no Spin methods, so just output the DAT section
        emitSpinCode = false;
        P->bedata = calloc(sizeof(AsmModData), 1);
        ValidateDatBase(P); // include DAT even though no labels referenced
        if (gl_p2) {
            gl_no_coginit = 1;
            gl_nospin = 1;
            gl_hub_base = 0;
        }
    }
    InitAsmCode();
    CompileIntermediate(systemModule);
    
    memset(&cogcode, 0, sizeof(cogcode));
    memset(&hubcode, 0, sizeof(hubcode));
    memset(&cogdata, 0, sizeof(cogdata));
    memset(&hubdata, 0, sizeof(hubdata));
    memset(&cogbss, 0, sizeof(cogbss));
    
    if (gl_output == OUTPUT_COGSPIN) {
        // count max. number of arguments in any public function
        // also try to guess at stack size required
        Function *func;
        Symbol *stackSym;
        int mboxSize;
        int stackSize = 1;
        int maxLeafSize = 0;
        func = P->functions;
#define STACK_SIZE_NAME "__STACK_SIZE"
        stackSym = FindSymbol(&P->objsyms, STACK_SIZE_NAME);
        while (func) {
            if (func->is_public) {
                if (func->numparams > maxargs) {
                    maxargs = func->numparams;
                    if (maxargs > MAX_COGSPIN_ARGS) {
                        ERROR(NULL, "function %s: too many arguments for .cog.spin", func->name);
                    }
                }
                if (func->numresults > maxrets) {
                    maxrets = func->numresults;
                }
            }
            if (func->local_address_taken || func->is_recursive || func->cog_task) {
                int savesize = FuncLocalSize(func) / LONG_SIZE;
                if (func->is_recursive) {
                    savesize *= 16;
                }
                if (IS_LEAF(func)) {
                    if (maxLeafSize < savesize) maxLeafSize = savesize;
                } else {
                    stackSize += savesize;
                }
                stackSize += maxLeafSize;
            }

            func = func->next;            
        }
        if (maxargs > maxrets) {
            mboxSize = maxargs + 3;
        } else {
            mboxSize = maxrets + 3;
        }
        EmitOp2(&cogcode, OPC_CONST, NewOperand(IMM_STRING, "__MBOX_SIZE", mboxSize), NewImmediate(mboxSize));

        if (!stackSym) {
            EmitOp2(&cogcode, OPC_CONST, NewOperand(IMM_STRING, STACK_SIZE_NAME, stackSize), NewImmediate(stackSize));
        }
    }
    /* count max parameters to coginit */
    {
        Module *Q;
        Function *pf;
        for (Q = allparse; Q; Q = Q->next) {
            for (pf = Q->functions; pf; pf = pf->next) {
                if (pf->cog_task) {
                    if (pf->numparams > max_coginit_args) {
                        if (pf->numparams > MAX_COGSPIN_ARGS) {
                            ERROR(NULL, "coginit function %s has too many parameters (%d), max is %d",
                                  pf->name, pf->numparams, MAX_COGSPIN_ARGS);
                        } else {
                            max_coginit_args = pf->numparams;
                        }
                    }
                }
            }
        }
    }
    if (emitSpinCode) {
        EmitOp1(&cogbss, OPC_ORG, cog_bss_start);
    }
    CompileConsts(&cogcode, P->conblock);

    // guesstimate how much space we will have for FCACHE, if
    // a dynamic size is requested
    if (gl_fcache_size < 0) {
        gl_fcache_size = GuessFcacheSize(&cogcode);
    }
    
    if (emitSpinCode) {
        // output the main stub
        EmitLabel(&cogcode, entrylabel);
        if (gl_have_lut) {
            lutstart = NewOperand(STRING_DEF, "lutentry", 0);
            EmitOp1(&lutcode, OPC_ORG, NewImmediate(0x200));
            EmitLabel(&lutcode, lutstart);
        }
        if (gl_output == OUTPUT_COGSPIN) {
            EmitMain_CogSpin(&cogcode, P, maxargs, maxrets);
        } else if (outputMain) {
            if (gl_p2) {
                EmitMain_P2(&cogcode, P, lutstart);
            } else {
                EmitMain_P1(&cogcode, P);
            }
        }
        // generate code for inlining
        CompileIntermediate(P);
        // compile COG functions
        if (!CompileToIR_cog(&cogcode, P)) {
            return;
        }
        if (!CompileToIR_lut(&lutcode, P)) {
            return;
        }

        if (HUB_CODE) {
            ValidateStackptr();
            if (!gl_p2) {
                EmitLabel(&hubcode, NewOperand(IMM_HUB_LABEL, "hub_ret_to_cog", 0));
                EmitJump(&hubcode, COND_TRUE,
                         NewOperand(IMM_COG_LABEL, "LMM_CALL_FROM_COG_ret", 0));
            }
            EmitLabel(&hubcode, NewOperand(IMM_HUB_LABEL, "hubentry", 0));
            if (!CompileToIR_hub(&hubcode, P)) {
                return;
            }
        }
        if (hubexit) {
            EmitLabel(&hubcode, hubexit);
            EmitJump(&hubcode, COND_TRUE, cogexit);
        }
        // output global functions
        CompileSystemModule(&cogcode, systemModule, VISITFLAG_COMPILEIR_COG);
        CompileSystemModule(&hubcode, systemModule, VISITFLAG_COMPILEIR_HUB);

        // now copy the hub code into place
        EmitBuiltins(&cogcode);

        // check for compression
        if (gl_compress) {
            IRCompress(&hubcode, &cogcode);
        }
    
        orgh = EmitOp0(&cogcode, OPC_HUBMODE);
    }

    if (emitSpinCode) {
        // add LUT checks
        if (gl_have_lut) {
            EmitOp1(&lutcode, OPC_FIT, NewImmediate(0x300));
            AppendIR(&hubcode, lutcode.head);
        }
        AppendIR(&cogcode, hubcode.head);

        // we have to optimize all code before emitting any variables
        OptimizeIRGlobal(&cogcode);

        // cog data
        EmitGlobals(&cogdata, &cogbss, &hubdata);
    
        // COG bss
        // FCACHE space
        if (HUB_CODE && !gl_p2) {
            EmitNamedCogLabel(&cogbss, "LMM_RETREG");
            EmitReserve(&cogbss, 1, COG_RESERVE);
            EmitNamedCogLabel(&cogbss, "LMM_FCACHE_START");
            EmitReserve(&cogbss, gl_fcache_size+1, COG_RESERVE);
            EmitNamedCogLabel(&cogbss, "LMM_FCACHE_END");
        }
    }

    // we need to emit all dat sections
    VisitRecursive(&hubdata, P, EmitDatSection, VISITFLAG_EMITDAT);
    VisitRecursive(&hubdata, systemModule, EmitDatSection, VISITFLAG_EMITDAT);
    
    // emit heap space, if we need it
    if (heaplabel) {
        Symbol *sym = LookupSymbol("__real_heapsize__");
        unsigned heapsize;
        if (sym && sym->kind == SYM_CONSTANT) {
            heapsize = EvalConstExpr((AST *)sym->val);
        } else {
            heapsize = 256;
        }
        heapsize += 2; // extra room for gc
        if (HUB_DATA) {
            EmitLabel(&hubdata, heaplabel);
            EmitReserve(&hubdata, heapsize, HUB_RESERVE);
        } else {
            EmitLabel(&cogbss, heaplabel);
            EmitReserve(&cogbss, heapsize, COG_RESERVE);
        }
    }
    
    // only the top level variable space is needed
    EmitVarSection(&hubdata, P);

    // finally the stack, if we need it
    if (stacklabel) {
        if (HUB_DATA) {
            EmitLabel(&hubdata, stacklabel);
            EmitReserve(&hubdata, 1, HUB_RESERVE);
        } else {
            EmitLabel(&cogbss, stacklabel);
            EmitReserve(&cogbss, 1, COG_RESERVE);
        }
    }

    if (emitSpinCode) {
        int cog_limit;
        Operand *limitop;
        if (gl_p2) {
            cog_limit = 480;
        } else {
            cog_limit = 496;
        }
        limitop = NewImmediate(cog_limit);
        // now insert the cog data after the cog code, before the orgh
        EmitLabel(&cogdata, cog_bss_start);
        EmitOp1(&cogdata, OPC_FIT, limitop);
    
        EmitOp1(&cogbss, OPC_FIT, limitop);
        if (orgh) {
            InsertAfterIR(&cogcode, orgh->prev, cogdata.head);
        }
    }
    // and the hub data at the end
    AppendIR(&cogcode, hubdata.head);
    // now the cog bss (which doesn't need any actual space
    AppendIR(&cogcode, cogbss.head);

    // and assemble the result
    asmcode = IRAssemble(&cogcode, P);
    
    current = save;

    f = fopen(fname, "w");
    if (!f) {
        fprintf(stderr, "Unable to open pasm output: ");
        perror(fname);
        exit(1);
    }
    // write a header if appropriate
    if (gl_output == OUTPUT_COGSPIN && gl_header1) {
        fprintf(f, "'' %s", gl_header1);
        fprintf(f, "'' %s", gl_header2);
    }
    fwrite(asmcode, 1, strlen(asmcode), f);
    fclose(f);
}
