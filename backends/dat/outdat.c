//
// binary data output for spin2cpp
//
// Copyright 2012-2022 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "spinc.h"
#include "becommon.h"

bool IsRelativeHubAddress(AST *);

#ifndef NEED_ALIGNMENT
#define NEED_ALIGNMENT (!gl_p2 && !gl_compress)
#endif

int gl_compress = 0;

static void putbyte(Flexbuf *f, unsigned int x)
{
    flexbuf_putc(x & 0xff, f);
}
static void putword(Flexbuf *f, unsigned int x)
{
    putbyte(f, x & 0xff);
    putbyte(f,  (x>>8) & 0xff);
}
static void putlong(Flexbuf *f, unsigned int x)
{
    putbyte(f, x & 0xff);
    putbyte(f,  (x>>8) & 0xff);
    putbyte(f, (x>>16) & 0xff);
    putbyte(f, (x>>24) & 0xff);
}

static void
OutputSpinDummyHeader(Flexbuf *f, Module *P)
{
    unsigned int clkfreq;
    unsigned int clkmodeval;

    if (!GetClkFreq(P, &clkfreq, &clkmodeval)) {
        // use defaults
        clkfreq = 80000000;
        clkmodeval = 0x6f;
    }
    
    putlong(f, clkfreq);     // offset 0
    
    putbyte(f, clkmodeval);  // offset 4
    putbyte(f, 0);      // checksum
    putword(f, 0x0010); // PBASE
    
    putword(f, 0x7fe8); // VBASE offset 8
    putword(f, 0x7ff0); // DBASE
    
    putword(f, 0x0018); // PCURR  offset 12
    putword(f, 0x7ff8); // DCURR
    
    putword(f, 0x0008); // object length? this ends up getting PCURR-$10
    putbyte(f, 0x02);   // seems always to be 02
    putbyte(f, 0x00);   // object length?
    
    putword(f, 0x0008);
    putword(f, 0x0000); // initial stack: 0 == first run of program
}

/*
 * change a 16 bit word in a Spin header
 */
static void
placeword(uint8_t *where, uint32_t value)
{
    static int warned_once = 0;
    if ((value > 0xffff) && !warned_once) {
        WARNING(NULL, "Program size exceeds legal Spin values");
        warned_once++;
    }
    where[0] = value & 0xff;
    where[1] = (value>>8) & 0xff;
}

/*
 * append a simple Spin program to launch
 * the assembly we just inserted
 */
static void
OutputSpinDummyFooter(Flexbuf *f)
{
    size_t curlen;
    size_t pad;
    uint8_t *image;
    
    // first, round up to longword boundary
    curlen = flexbuf_curlen(f);
    pad = ((curlen + 3) & ~3) - curlen;
    while (pad > 0) {
        putbyte(f, 0);
        curlen++;
    }
    // output simple spin program to start cog 0
    putbyte(f, 0x35);   // constant 0
    putbyte(f, 0xc7);   // memory op long PBASE + 8
    putbyte(f, 0x08);
    putbyte(f, 0x35);   // constant 0
    
    putbyte(f, 0x2c);   // coginit
    putbyte(f, 0x32);   // return
    putbyte(f, 0x00);
    putbyte(f, 0x00);

    // fixup header at start of program
    image = (uint8_t *)flexbuf_peek(f);

    // set PCURR to curlen
    placeword(image+8,  curlen + 8);  // set VBASE to curlen + 8 (end of program)
    placeword(image+10, curlen + 16); // set DBASE to curlen + 16 (after stack??)
    placeword(image+12, curlen);      // set PCURR to object entry point
    placeword(image+14, curlen + 20); // set DCURR to after all program stuff
    placeword(image+16, curlen - 8); // don't know what this variable is
    placeword(image+18, 2);           // always seems to get 2
    placeword(image+20, curlen - 16);
}

void
OutputDatFile(const char *fname, Module *P, int prefixBin)
{
    FILE *f = NULL;
    Module *save;
    Flexbuf fb;
    size_t curlen;
    size_t desiredlen;
    save = current;
    current = P;

    gl_nospin = 1; // we are outputting only the dat section
    f = fopen(fname, "wb");
    if (!f) {
        fprintf(stderr, "Unable to open output file: ");
        perror(fname);
        exit(1);
    }

    flexbuf_init(&fb, BUFSIZ);
    if (prefixBin && !gl_p2) {
        /* output a binary header */
        OutputSpinDummyHeader(&fb, P);
    }
    PrintDataBlock(&fb, P->datblock, NULL, NULL);
    if (prefixBin && !gl_p2) {
        // output the actual Spin program
        OutputSpinDummyFooter(&fb);
    }
    curlen = flexbuf_curlen(&fb);
    fwrite(flexbuf_peek(&fb), curlen, 1, f);
    if (gl_p2) {
        // round up to multiple of 32 bytes, like PNut does
        // desiredlen = (curlen + 31) & ~31;
        desiredlen = curlen; // PNut no longer rounds up
    } else {
        desiredlen = (curlen + 3) & ~3;
    }
    while (curlen < desiredlen) {
        fputc(0, f);
        curlen++;
    }
    fclose(f);
    flexbuf_delete(&fb);
    
    current = save;
}

/*
 * functions for output of DAT sections
 */
/*
 * data block printing functions
 */
#define BYTES_PER_LINE 16  /* must be at least 4 */
static int datacount = 0;

static void
outputByteBinary(Flexbuf *f, int c)
{
    flexbuf_putc(c, f);
}

static DataBlockOutFuncs defaultOutFuncs = {
    NULL,
    outputByteBinary,
    NULL,
};

static DataBlockOutFuncs *outFuncs;

static void
outputByte(Flexbuf *f, int c)
{
    (outFuncs->putByte)(f, c);
    datacount++;
}

static void
outputLong(Flexbuf *f, uint32_t c)
{
    int i;
    for (i = 0; i < 4; i++) {
        outputByte(f, (c & 0xff));
        c = c>>8;
    }
}

static void
outputInstrLong(Flexbuf *f, uint32_t c)
{
    outputLong(f, c);
}

static void
initDataOutput(DataBlockOutFuncs *funcs)
{
    datacount = 0;
    if (funcs) {
        outFuncs = funcs;
    } else {
        outFuncs = &defaultOutFuncs;
    }
}

static int
GetAddrOffset(AST *ast)
{
    AST *offsetExpr = NULL;
    Symbol *sym;
    Label *label;
    int r;
    const char *user_name = NULL;
    const char *internal_name;
    
    if (ast->kind == AST_ARRAYREF) {
        offsetExpr = ast->right;
        ast = ast->left;
    }
    if (IsIdentifier(ast)) {
        user_name = GetUserIdentifierName(ast);
        internal_name = GetIdentifierName(ast);
    } else {
        ERROR(ast, "@@@ supported only on identifiers");
        return 0;
    }
    sym = LookupSymbol(internal_name);
    if (!sym) {
        ERROR(ast, "Unknown symbol %s", user_name);
        return 0;
    }
    if (sym->kind != SYM_LABEL) {
        ERROR(ast, "@@@ supported only on labels and globals; %s is not one", user_name);
        return 0;
    }
    label = (Label *)sym->val;
    r = label->hubval;
    if (offsetExpr) {
        int offset = EvalPasmExpr(offsetExpr);
        if (label->type) {
            offset = offset * TypeSize(BaseType(label->type));
        }
        r += offset;
    }
    return r;
}

// figure out whether an expression is relocatable;
// if it is, calculate the symbol and offset
// NOTE: if the relocation is complicated like "@@@foo / 4"
// then we return -1

static int
IsRelocatable(AST *sub, Symbol **symptr, int32_t *offptr, bool isInitVal)
{
    int32_t myoff;
    int r1 , r2;
    int kind;
    Symbol *mysym;
    
    if (symptr) {
        *symptr = NULL;
    }
    if (offptr) {
        *offptr = 0;
    }
    while (sub && sub->kind == AST_CAST) {
        sub = sub->right;
    }
    if (!sub) {
        if (offptr) {
            *offptr = 0;
        }
        return RELOC_KIND_NONE;
    }
    kind = sub->kind;
    if (kind == AST_SIMPLEFUNCPTR) {
        Symbol *sym = LookupAstSymbol(sub->left, "pointer address");
        if (!sym || sym->kind != SYM_FUNCTION) {
            ERROR(sub, "Bad function pointer");
            return RELOC_KIND_NONE;
        }
        if (symptr) {
            *symptr = sym;
        }
        return RELOC_KIND_I32;
    }
    if (kind == AST_ABSADDROF || (isInitVal && kind == AST_ADDROF) ) {
        if (offptr) {
            *offptr = GetAddrOffset(sub->left);
        }
        return RELOC_KIND_I32;
    }
    if (kind == AST_IDENTIFIER || kind == AST_LOCAL_IDENTIFIER) {
        if (IsRelativeHubAddress(sub)) {
            if (offptr) {
                *offptr = GetAddrOffset(sub);
            }
            return RELOC_KIND_I32;
        }
    }
    if (kind == AST_OPERATOR) {
        r1 = IsRelocatable(sub->left, symptr, offptr, isInitVal);
        r2 = IsRelocatable(sub->right, &mysym, &myoff, isInitVal);
        if (r1 || r2) {
            // if one side has an error, return an error
            if ( -1 == (r1|r2) ) {
                return -1;
            }
            if (symptr && *symptr != mysym) {
                return -1;
            }
            // only certain kinds of math we can do on relocations
            switch (sub->d.ival) {
            case '+':
                if (r1 && r2) {
                    return -1;
                }
                if (offptr) {
                    *offptr += myoff;
                }
                return RELOC_KIND_I32;
            case '-':
                if (r1 && r2) {
                    // difference of absolute relocations
                    // this is fine
                    return RELOC_KIND_NONE;
                }
                if (r1) {
                    // reloc - offset
                    if (offptr) {
                        *offptr -= myoff;
                    }
                    return RELOC_KIND_I32;
                }
                // offset - reloc
                // not implemented
                return -1;
            default:
                return -1;
            }
        }
    }

    // not a relocatable expression per se; so just set the offset
    // to our value
    if (offptr) {
        *offptr = EvalPasmExpr(sub);
    }
    return 0;
}

int64_t
EvalRelocPasmExpr(AST *expr, Flexbuf *f, Flexbuf *relocs, int *relocOff, bool isInitVal, int relocKind)
{
    int checkReloc;
    int32_t offset;
    Symbol *sym;

    if (expr->kind == AST_OPERATOR) {
        if (expr->d.ival == K_INCREMENT || expr->d.ival == K_DECREMENT) {
            ERROR(expr, "invalid addressing mode for instruction");
            return 0;
        }
    }
    if (relocOff) {
        *relocOff = -1;
    }
    if (relocs) {
        checkReloc = IsRelocatable(expr, &sym, &offset, isInitVal);
        if (checkReloc != RELOC_KIND_NONE) {
            if (checkReloc == -1) {
                ERROR(expr, "Illegal operation on relocatable @@@ value");
            } else {
                Reloc r;
                int addr = flexbuf_curlen(f);
                r.kind = relocKind;
                r.addr = addr;
                r.sym = sym;
                r.symoff = offset;
                flexbuf_addmem(relocs, (const char *)&r, sizeof(r));
                if (relocOff) {
                    *relocOff = flexbuf_curlen(relocs) - sizeof(r);
                }
            }
            return offset;
        }
    }
    return EvalPasmExpr(expr);
}

static void
AlignPc(Flexbuf *f, int size)
{
        while ((datacount % size) != 0) {
            outputByte(f, 0);
        }
}

/* output a single data item element */
void
outputInitItem(Flexbuf *f, int elemsize, AST *item, int reps, Flexbuf *relocs, AST *type)
{
    uintptr_t origval, val;
    int i;
    Reloc *rptr;
    int relocOff;
    AST *exprType;
    
    if (elemsize == 0) {
        return;
    }
    if (item) {
        if (item->kind == AST_SIMPLEFUNCPTR) {
            exprType = NULL;
        } else {
            exprType = CheckTypes(item);
            if (IsGenericType(exprType) && item->kind == AST_CAST && item->right->kind == AST_ABSADDROF) {
                exprType = NULL;
            }
        }
        if (exprType) {
            if (!IsPointerType(exprType) && !IsArrayType(exprType) && !IsConstExpr(item)) {
                ERROR(item, "global initializers are evaluated at compile time and therefore must be constant");
            }
            type = CoerceAssignTypes(item, AST_ASSIGN, &item, type, exprType, "initialization");
            // ignore any casts added
            while (item->kind == AST_CAST) {
                item = item->right;
            }
        }
        origval = EvalRelocPasmExpr(item, f, relocs, &relocOff, true, RELOC_KIND_I32);
        if (relocOff >= 0) {
            rptr = (Reloc *)(flexbuf_peek(relocs) + relocOff);
        } else {
            rptr = 0;
        }
        if (rptr && (rptr->kind > RELOC_KIND_NONE) && reps > 1) {
            ERROR(item, "repeat counts not supported on relocatable items");
        }
    } else {
        origval = 0;
    }
    while (reps > 0) {
        val = origval;
        for (i = 0; i < elemsize; i++) {
            outputByte(f, val & 0xff);
            val = val >> 8;
        }
        --reps;
    }
}

/* output a variable initializer list */
/* returns the number of elements output */
int
outputInitList(Flexbuf *f, int elemsize, AST *initval, int numelems, Flexbuf *relocs, AST *type)
{
    int n = 0;
    if (!initval) {
        outputInitItem(f, elemsize, NULL, numelems, relocs, type);
        return numelems;
    }
    if (initval->kind == AST_EXPRLIST) {
        AST *item;
        int elemsleft = numelems;
        int r;
        while (initval && elemsleft > 0) {
            item = initval->left;
            initval = initval->right;
            r = outputInitList(f, elemsize, item, 1, relocs, type);
            n += r;
            elemsleft -= r;
        }
    } else {
        outputInitItem(f, elemsize, initval, 1, relocs, type);
        return 1;
    }
    return n;
}

void
outputInitializer(Flexbuf *f, AST *type, AST *initval, Flexbuf *relocs)
{
    int elemsize;
    int typealign, typesize;
    int numelems;
    AST *varlist;
    Module *P;
    int is_union = 0;
    
    type = RemoveTypeModifiers(type);
    typealign = TypeAlign(type);
    elemsize = typesize = TypeSize(type);
    numelems  = 1;

    AlignPc(f, typealign);
    if (!initval) {
        // just fill with 0s
        outputInitList(f, elemsize, initval, numelems, relocs, type);
        return;
    }

    // done in pasm.c now
    //initval = FixupInitList(type, initval);
    
    switch(type->kind) {
    case AST_GENERICTYPE:
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_FLOATTYPE:
    case AST_PTRTYPE:
        outputInitList(f, elemsize, initval, numelems, relocs, type);
        break;
    case AST_ARRAYTYPE:
    {
        type = RemoveTypeModifiers(BaseType(type));
        elemsize = TypeSize(type);
        numelems = typesize / elemsize;
        if (initval && initval->kind != AST_EXPRLIST) {
            ERROR(initval, "Internal compiler error, expected initializer list");
            break;
        }
        while (numelems > 0 && initval) {
            --numelems;
            outputInitializer(f, type, initval->left, relocs);
            initval = initval->right;
        }
        if (numelems > 0) {
            outputInitList(f, elemsize, NULL, numelems, relocs, type);
        } else if (initval) {
            WARNING(initval, "too many elements found in initializer");
        }
        break;
    }
    
    case AST_OBJECT:
        P = GetClassPtr(type);
        is_union = P->isUnion;
        varlist = P->finalvarblock;
        if (initval->kind != AST_EXPRLIST) {
            initval = NewAST(AST_EXPRLIST, initval, NULL);
        }
        while (varlist) {
            AST *subtype;
            AST *subinit = initval ? initval->left : NULL;
            if (varlist->left->kind == AST_DECLARE_BITFIELD) {
                varlist = varlist->right;
                continue;
            }
            if (is_union) {
                if (subinit->kind == AST_CAST) {
                    /* the user explicitly cast to tell us what kind there is  */
                    subtype = subinit->left;
                    subinit = subinit->right;
                } else if (subinit->kind == AST_INITMODIFIER && subinit->left && subinit->left->kind == AST_METHODREF) {
                    // .f = nn
                    AST *ident = subinit->left->right;
                    subtype = ExprTypeRelative(&P->objsyms, ident, P);
                    subinit = subinit->right;
                } else {
                    // just assume the type matches the first element of the union
                    subtype = ExprTypeRelative(&P->objsyms, varlist->left, P);
                }
            } else {
                subtype = ExprType(varlist->left);
            }
            outputInitializer(f, subtype, subinit, relocs);
            varlist = varlist->right;
            if (initval) initval = initval->right;
            if (is_union) {
                break;
            }
        }
        if (initval) {
            WARNING(initval, "too many initializers");
        }
        // objects are padded to a long boundary
        AlignPc(f, LONG_SIZE);
        return;
    default:    
        ERROR(initval, "Unable to initialize elements of this type");
        initval = NULL;
        break;
    }
}

/* output an FVAR or FVARS item */
static void
outputFvar(Flexbuf *f, Flexbuf *relocs, AST *ast, int isSigned, int32_t *relocOff)
{
    int32_t val;
    int32_t maxval;
    int i;

    if (!ast || ast->kind != AST_EXPRLIST) {
        ERROR(ast, "bad FVAR expression");
        return;
    }
    ast = ast->left;
    val = EvalRelocPasmExpr(ast, f, relocs, relocOff, false, RELOC_KIND_I32);
    if (!isSigned && val < 0) {
        ERROR(ast, "FVAR item is out of range");
        return;
    }
    maxval = isSigned ? (1<<6) : (1<<7);
    for (i = 0; i < 3; i++) {
        if (val >= -maxval && val < maxval) {
            outputByte(f, val & 0x7f);
            return;
        }
        outputByte(f, 0x80 | (val & 0x7f));
        val = val >> 7;
    }
    outputByte(f, val);
}
        
/* output a data list as found in PASM "long", "byte", etc. */
/* checkSize = 1 says the output must fit in an unsigned */
/* checkSize = 2 says the output must fit in a signed */
/* checkSize = 3 allows for either */

#define CHECKSIZE_NONE     0
#define CHECKSIZE_UNSIGNED 1
#define CHECKSIZE_SIGNED   2
#define CHECKSIZE_ANY      3
void
outputDataList(Flexbuf *f, int size, AST *ast, Flexbuf *relocs, int checkSize)
{
    unsigned origval;
    int32_t val;
    int i, reps;
    AST *sub;
    int32_t relocOff = 0;
    int origsize = size;
    int checkThis = 0;
    
    origval = 0;
    while (ast) {
        sub = ast->left;
        checkThis = checkSize;
        if (sub->kind == AST_EXPRLIST && sub->right == 0) {
            sub = sub->left;
        }
        if (sub->kind == AST_BYTELIST || sub->kind == AST_BYTEFITLIST) {
            size = 1;
            if (sub->kind == AST_BYTEFITLIST) {
                checkThis = CHECKSIZE_UNSIGNED;
            }
            sub = ExpectOneListElem(sub->left);
        } else if (sub->kind == AST_WORDLIST || sub->kind == AST_WORDFITLIST) {
            size = 2;
            if (sub->kind == AST_WORDFITLIST) {
                checkThis = CHECKSIZE_UNSIGNED;
            }
            sub = ExpectOneListElem(sub->left);
        } else if (sub->kind == AST_LONGLIST) {
            size = 4;
            sub = ExpectOneListElem(sub->left);
        } else {
            size = origsize;
        }
        if (sub->kind == AST_ARRAYDECL || sub->kind == AST_ARRAYREF) {
            origval = EvalPasmExpr(ast->left->left);
            reps = EvalPasmExpr(ast->left->right);
        } else if (sub->kind == AST_STRING) {
            const char *ptr = sub->d.string;
            while (*ptr) {
                val = (*ptr++) & 0xff;
                outputByte(f, val);
                for (i = 1; i < size; i++) {
                    outputByte(f, 0);
                }
            }
            reps = 0;
        } else if (sub->kind == AST_RANGE) {
            int start = EvalPasmExpr(sub->left);
            int end = EvalPasmExpr(sub->right);
            while (start <= end) {
                val = start;
                for (i = 0; i < size; i++) {
                    outputByte(f, val & 0xff);
                    val = val >> 8;
                }
                start++;
            }
            reps = 0;
        } else if (sub->kind == AST_FVAR_LIST) {
            outputFvar(f, relocs, sub->left, 0, &relocOff);
            reps = 0;
        } else if (sub->kind == AST_FVARS_LIST) {
            outputFvar(f, relocs, sub->left, 0, &relocOff);
            reps = 0;
        } else {
            origval = EvalRelocPasmExpr(sub, f, relocs, &relocOff, false, RELOC_KIND_I32);
            if (relocOff >= 0) {
                Reloc *r = (Reloc *)(flexbuf_peek(relocs) + relocOff);       
                (void)r;
            }
            reps = 1;
        }
        while (reps > 0) {
            int sign = 0;
            val = (int32_t) origval;
            for (i = 0; i < size; i++) {
                outputByte(f, val & 0xff);
                sign = (val & 0x80);
                val = val >> 8;
            }
            if (checkThis) {
                int ok = 0;
                if (checkThis & CHECKSIZE_UNSIGNED) {
                    if (val == 0) ok = 1;
                }
                if (checkThis & CHECKSIZE_SIGNED) {
                    if (val == 0 && sign == 0) ok = 1;
                    if (val == -1 && sign == 0x80) ok = 1;
                }
                if (!ok) {
                    char *sizemsg = (size == 1) ? "byte" : "word";
                    ERROR(ast, "Value $%x does not fit in %s", origval, sizemsg);
                }
                checkThis = 0;
            }
            --reps;
        }
        ast = ast->right;
    }
}


#define BIG_IMM_SRC 0x01
#define BIG_IMM_DST 0x02
#define ANY_BIG_IMM (BIG_IMM_SRC|BIG_IMM_DST)
#define DUMMY_MASK  0x80

static unsigned
ImmMask(Instruction *instr, int opnum, bool bigImm, AST *ast)
{
    unsigned mask;
    
    if (gl_p2) {
        mask = P2_IMM_SRC;
        if (bigImm) {
            mask |= BIG_IMM_SRC;
        }
    } else {
        mask = IMMEDIATE_INSTR;
    }
    switch(instr->ops) {
    case P2_JUMP:
    case P2_LOC:
    case P2_CALLD:
    case P2_AUG:
    case P2_JINT_OPERANDS:
    case JMP_OPERAND:
    case CALL_OPERAND:
        if (bigImm) {
            ERROR(ast, "## is not legal with %s", instr->name);
        }
        return mask;
    case SRC_OPERAND_ONLY:
        return mask;
    case TWO_OPERANDS:
    case TWO_OPERANDS_OPTIONAL:
    case TWO_OPERANDS_DEFZ:
    case JMPRET_OPERANDS:
    case P2_TJZ_OPERANDS:
    case P2_TWO_OPERANDS:
    case P2_RDWR_OPERANDS:
    case THREE_OPERANDS_BYTE:
    case THREE_OPERANDS_NIBBLE:
    case THREE_OPERANDS_WORD:
        if (gl_p2) {
            if (opnum == 0) {
                /* check that immediate source is legal */
                /* the L bit occupies the same place as Z, so if the
                   instruction can do a wz or wcz no immediate is legal */
                if (instr->flags & (FLAG_WZ|FLAG_WCZ|FLAG_ANDZ)) {
                    ERROR(ast, "Immediate values are not allowed for first operand of %s", instr->name);
                }
                mask = P2_IMM_DST;
                if (bigImm) {
                    mask |= BIG_IMM_DST;
                }
            } else if (opnum == 2) {
                mask = DUMMY_MASK; // no change to opcode
            }
        } else {
            if (opnum == 0) {
                ERROR(ast, "bad immediate operand to %s", instr->name);
                return 0;
            }
        }
        return mask;
    case P2_DST_CONST_OK:
        /* uses the I bit instead of L?? */
        mask = P2_IMM_SRC;
        if (bigImm) {
            mask |= BIG_IMM_DST;
        }
        return mask;
    default:
        ERROR(ast, "immediate value not supported for %s instruction", instr->name);
        return 0;
    }
}

/*
 * assemble a special read operand
 */
unsigned
SpecialRdOperand(AST *ast, uint32_t opimm)
{
    HwReg *hwreg;
    uint32_t val;
    int subval = 0;
    int negsubval = 0;
    int saw_array = 0;
    
    if (opimm && ast->kind != AST_RANGEREF) {
        // user provided an immediate value; make sure it
        // fits in $00-$ff
        val = EvalPasmExpr(ast);
        if ( (val > 0xff) && 0 == (opimm & BIG_IMM_SRC) ) {
            WARNING(ast, "immediate value out of range");
        }
    }
    val = 0;

    // handle ptra++[INDEX], which is parsed as (ptra++)[INDEX]
    if (ast->kind == AST_ARRAYREF) {
        subval = EvalPasmExpr(ast->right);
        ast = ast->left;
        saw_array = 1;
    }
    
    // other things
    if (ast->kind == AST_OPERATOR && (ast->d.ival == K_INCREMENT
                                      || ast->d.ival == K_DECREMENT))
    {
        if (!subval) {
            if (saw_array) {
                ERROR(ast, "PTRx index of 0 is not valid");
            }
            subval = 1;
        }
        if (ast->d.ival == K_INCREMENT) {
            if (ast->left) {
                ast = ast->left;
                // a++: x110 0001
                val = 0x60;
            } else {
                // ++a: x100 0001
                ast = ast->right;
                val = 0x40;
            }
        } else {
            if (ast->left) {
                ast = ast->left;
                // a--
                val = 0x60;
                subval = -subval;
            } else {
                // --a: x101 1111
                ast = ast->right;
                val = 0x40;
                subval = -subval;
            }
        }
    }

    negsubval = subval < 0 ? -1 : 1;
    if (ast->kind == AST_RANGEREF) {
        AST *idx = ast->right;
        if (idx && idx->kind == AST_RANGE) {
            if (idx->right) {
                ERROR(ast, "illegal ptr dereference");
                return 0;
            }
            subval = EvalPasmExpr(idx->left) * negsubval;
            ast = ast->left;
        } else if (IsConstExpr(idx)) {
            subval = EvalPasmExpr(idx) * negsubval;
            ast = ast->left;
        } else {
            ERROR(ast, "bad ptr expression");
            return 0;
        }
    }
    if (ast->kind == AST_HWREG) {
        hwreg = (HwReg *)ast->d.ptr;
        if (hwreg->addr == 0x1f8) {
            // ptra
            val |= 0x100;
        } else if (hwreg->addr == 0x1f9) {
            // ptrb
            val |= 0x180;
        } else {
            if (val) {
                ERROR(ast, "only ptra or ptrb allowed");
            }
            return 0;
        }
    } else if (val || saw_array) {
        ERROR(ast, "bad rd*/wr* pointer: only ptra or ptrb allowed");
        return 0x100;
    }

    if (opimm & BIG_IMM_SRC) {
        // the "val" bits have to go up
        if (val & 0x100) {
            val = val << 15;
            val |= subval & 0xfffff;
            return val;
        }
    }
    // index ranges from -32 to 31 for all modes on rev A,
    // and for most of them on rev B (except for plain indexing)
    if (0 != (val & 0x60) || gl_p2 == P2_REV_A) {
        if (subval < -16 || subval > 15) {
            if (gl_p2 >= P2_REV_A && subval == 16) {
                // this is actually OK, 16 gets encoded as 0
            } else {
                ERROR(ast, "ptr index out of range -16 to 31");
            }
            subval = 0;
        }
        return val | (subval & 0x1f);
    } else {
        // plain indexing on rev B and later
        if (subval < -32 || subval > 31) {
            ERROR(ast, "ptr index out of range");
            subval = 0;
        }
        return val | (subval & 0x3f);
    }
}

/* utility routine to fix up an opcode based on the #N at the end */
uint32_t
FixupThreeOperands(uint32_t val, AST *op, uint32_t immflags, uint32_t maxN, AST *line, Instruction *instr)
{
    uint32_t NN;
    if (!op || immflags == 0) {
        ERROR(line, "Third operand to %s must be an immediate\n", instr->name);
        return val;
    }
    NN = EvalPasmExpr(op);
    if (NN >= maxN) {
        ERROR(line, "Third operand to %s must be less than %u\n", instr->name, maxN);
        return val;
    }
    val |= NN << 19;
    return val;
}

/*
 * check for whether an operand is a constant integer
 */
static int
IsConstInteger(AST *op)
{
    int r = 0;
    uint32_t N;
    
    if (op->kind == AST_INTEGER) {
        r = 1;
    } else if (IsIdentifier(op)) {
        if (IsConstExpr(op)) {
            r = 1;
        }
    }
    if (r) {
        N = EvalPasmExpr(op);
        if (N >= 0x1c0 && N < 0x1f0) {
            // special case;
            // allow constants in range 1c0-1ef to be
            // used as registers without warning
            r = 0;
        }
    }
    
    return r;
}

/* returns a mask indicating which operands should be checked for
 * immediate constants:
 *   0x1: first operand only
 *   0x2: second operand
 *   0x3: both operands
 */
static unsigned
InstructionWarnAboutConsts(Instruction *instr)
{
    if (!(gl_warn_flags & WARN_ASM_USAGE)) {
        return 0;
    }
    switch(instr->ops) {
    case TWO_OPERANDS:
    case TWO_OPERANDS_DEFZ:
    case TWO_OPERANDS_OPTIONAL:
    case P2_LOC:
        return 2;
    case P2_TWO_OPERANDS:
        return 3;
    case P2_DST_CONST_OK:
        return 1;
    default:
        return 0;
    }
}

#define MAX_OPERANDS 3
/*
 * decode operands, taking care of alternate forms and such
 * fills in the operands table with operands for the instruction,
 * and opimm with immediate bits
 * returns number of operands
 */

int
DecodeAsmOperands(Instruction *instr, AST *ast, AST **operand, uint32_t *opimm, uint32_t *val, uint32_t *effectFlags)
{
    int numoperands = 0;
    AST *line = ast;
    bool sawFlagUsed = false;
    uint32_t mask;
    int expectops;
    int jump_operand = -1; /* indicates no jump operand */
    
    /* check for modifiers and operands */
    numoperands = 0;
    ast = ast->right;
    while (ast != NULL) {
        if (ast->kind == AST_SRCCOMMENT || ast->kind == AST_COMMENT) {
            /* do nothing */
        } else if (ast->kind == AST_EXPRLIST) {
            uint32_t imask;
            AST *op;
            if (numoperands >= MAX_OPERANDS) {
                ERROR(line, "Too many operands to instruction");
                return -1;
            }
            if (ast->left && ast->left->kind == AST_IMMHOLDER) {
                imask = ImmMask(instr, numoperands, false, ast);
                op = ast->left->left;
            } else if (ast->left && ast->left->kind == AST_BIGIMMHOLDER) {
                imask = ImmMask(instr, numoperands, true, ast);
                op = ast->left->left;
            } else {
                imask = 0;
                op = ast->left;
            }
            operand[numoperands] = op;
            opimm[numoperands] = imask;
            numoperands++;
        } else if (ast->kind == AST_INSTRMODIFIER) {
            InstrModifier *mod = (InstrModifier *)ast->d.ptr;
            mask = mod->modifier;
            /* record if C or Z flags were set */
            if (mod->flags & (FLAG_CZSET)) {
                sawFlagUsed = true;
            }
            if (effectFlags) {
                *effectFlags |= mod->flags;
            }
            /* verify here that the modifier is permitted for this instruction */
            if (mod->flags) {
                if (0 == (mod->flags & instr->flags)) {
                    ERROR(line, "modifier %s not valid for %s", mod->name, instr->name);
                }
                if (instr->flags == FLAG_P2_CZTEST) {
                    // need to tweak the instruction bits here based on instruction type
                    uint32_t instrMask = 0;
                    if (mod->flags == FLAG_WZ || mod->flags == FLAG_WC) {
                        instrMask = 0;
                    } else if (mod->flags == FLAG_ANDC || mod->flags == FLAG_ANDZ) {
                        instrMask = 2;
                    } else if (mod->flags == FLAG_ORC || mod->flags == FLAG_ORZ) {
                        instrMask = 4;
                    } else if (mod->flags == FLAG_XORC || mod->flags == FLAG_XORZ) {
                        instrMask = 6;
                    }
                    if (instr->ops == P2_DST_CONST_OK) {
                        // testp: modify src bits
                        if (val) *val |= instrMask;
                    } else if (instr->ops == TWO_OPERANDS) {
                        // testb: modify opcode bits
                        if (val) *val |= (instrMask << 21);
                    } else {
                        ERROR(line, "internal error in instruction table");
                    }
                }
            } else {
                // this is a condition (like if_z, etc.
                if (instr->opc == OPC_NOP) {
                    ERROR(line, "attempt to modify NOP with condition");
                }
            }
            if (val) {
                if (mask & 0x0003ffff) {
                    *val = *val & mask;
                } else {
                    *val = *val | mask;
                }
            }
        } else {
            ERROR(line, "Internal error, expected instruction modifier found %d", ast->kind);
            return -1;
        }
        ast = ast->right;
    }
    
    /* warn about some instructions not having wc or wz */
    /* if the instruction has a _ret_ tag on it it might be deliberately doing nothing ;
       _ret_ cmp 0, #0 can be used to double as a counter
       similarly on P1 for a no-op instruction
    */
    if (gl_p2) {
        mask = 0xf << 28;
    } else {
        mask = 0xf << 18;
    }
    if ((*val & mask) == 0) {
        sawFlagUsed = true; // do not warn here
    }
    if (instr->flags == FLAG_P2_CZTEST && !sawFlagUsed) {
        ERROR(line, "instruction %s requires flags to be tested", instr->name);
    }
    else if ((instr->flags & FLAG_WARN_NOTUSED) && !sawFlagUsed) {
        WARNING(line, "instruction %s used without flags being set", instr->name);
    }
    if (instr->opc == OPC_BREAK && (*val & mask) != mask) {
        WARNING(line, "conditional BRK instruction does not work");
    }
    
    /* parse operands and put them in place */
    switch (instr->ops) {
    case NO_OPERANDS:
        expectops = 0;
        break;
    case JMPRET_OPERANDS:
    case P2_TJZ_OPERANDS:
        jump_operand = 1;
        expectops = 2;
        break;
    case TWO_OPERANDS:
    case TWO_OPERANDS_OPTIONAL:
    case TWO_OPERANDS_DEFZ:
    case P2_TWO_OPERANDS:
    case P2_RDWR_OPERANDS:
    case P2_LOC:
    case P2_CALLD:
    case P2_MODCZ:
        expectops = 2;
        break;
    case THREE_OPERANDS_BYTE:
    case THREE_OPERANDS_WORD:
    case THREE_OPERANDS_NIBBLE:
        expectops = 3;
        break;
    case JMP_OPERAND:
    case P2_JUMP:
    case CALL_OPERAND:
        jump_operand = 0;
        expectops = 1;
        break;        
    default:
        expectops = 1;
        break;
    }
    if (instr->ops == TWO_OPERANDS_OPTIONAL && numoperands == 1) {
        // duplicate the operand
        // so neg r0 -> neg r0,r0
        operand[numoperands] = operand[0];
        opimm[numoperands] = opimm[0];
        numoperands++;
    } else if (instr->ops == TWO_OPERANDS_DEFZ && numoperands == 1) {
        // supply #0 for second operand
        int defval = 0;
        if (!strcmp(instr->name, "alti")) {
            defval = 0x164; // 1 01_10 0_100
        }
        operand[1] = AstInteger(defval);
        opimm[1] = P2_IMM_SRC;
        numoperands = 2;
    } else if (instr->ops == P2_MODCZ && numoperands == 1) {
        // modc x -> modcz x, 0
        // modz y -> modcz 0, y
        if (!strcmp(instr->name, "modc")) {
            operand[1] = AstInteger(0);
            opimm[1] = 0;
            numoperands = 2;
        } else if (!strcmp(instr->name, "modz")) {
            operand[1] = operand[0];
            operand[0] = AstInteger(0);
            opimm[1] = opimm[0];
            opimm[0] = 0;
            numoperands = 2;
        }
    } else if (instr->opc == OPC_GETRND && numoperands == 0) {
        operand[0] = AstInteger(0);
        opimm[0] = 1;
        numoperands = 1;
    }
    if (expectops == 3 && numoperands == 1) {
        // SETNIB reg/# -> SETNIB 0, reg/#, #0
        // but GETBYTE reg -> GETBYTE reg, 0, #0

        if (!strncmp(instr->name, "set", 3)) {
            operand[1] = operand[0];
            opimm[1] = opimm[0]>>1;  // convert dst imm to src imm
            operand[0] = AstInteger(0);
            opimm[0] = 0;
        } else {
            operand[1] = AstInteger(0);
            opimm[1] = 0;
        }
        operand[2] = AstInteger(0);
        opimm[2] = DUMMY_MASK;
        numoperands = 3;
    }
    if (expectops != numoperands) {
        ERROR(line, "Expected %d operands for %s, found %d", expectops, instr->name, numoperands);
        return -1;
    }
    if (jump_operand >= 0 && opimm[jump_operand] == 0) {
        /* if it's going to a label we may be able to notice if the
           user forgot to type "#"
        */
        AST *op;
        op = operand[jump_operand];
        if (IsIdentifier(op)) {
            const char *internal_name = GetIdentifierName(op);
            const char *user_name = GetUserIdentifierName(op);
            Symbol *sym = LookupSymbol(internal_name);
            if (sym && sym->kind == SYM_LABEL) {
                Label *lab = (Label *)sym->val;
                if (lab && (lab->flags & LABEL_HAS_INSTR) && !(lab->flags & LABEL_HAS_JMP)) {
                    WARNING(line, "%s to %s without #; are you sure this is correct? If so, change the %s operand to %s-0 to suppress this warning", instr->name, user_name, instr->name, user_name);
                }
            }
        }
    } else {
        unsigned warn_mask = InstructionWarnAboutConsts(instr);
        if ( (warn_mask & 1) && operand[0] && !opimm[0]) {
            if (IsConstInteger(operand[0])) {
                WARNING(line, "First operand to %s is a constant used without #; is this correct? If so, you may suppress this warning by putting -0 after the operand", instr->name);
            }
        }
        if ( (warn_mask & 2) && operand[1] && !opimm[1]) {
            if (IsConstInteger(operand[1])) {
                WARNING(line, "Second operand to %s is a constant used without #; is this correct? If so, you may suppress this warning by putting -0 after the operand", instr->name);
            }
        }
    }
        
    return numoperands;
}

// assemble comments
// returns first non-comment thing seen
static AST* AssembleComments(Flexbuf *f, Flexbuf *relocs, AST *ast)
{
    Reloc r;
    while (ast) {
        if (ast->kind == AST_SRCCOMMENT && gl_srccomments) {
            if (relocs) {
                r.kind = RELOC_KIND_DEBUG;
                r.addr = flexbuf_curlen(f);
                r.sym = (Symbol *)GetLineInfo(ast);
                r.symoff = 0;
                flexbuf_addmem(relocs, (const char *)&r, sizeof(r));
            }
        } else if (ast->kind == AST_COMMENT) {
            /* do nothing */
        } else {
            return ast;
        }
        ast = ast->right;
    }
    return ast;
}

bool
IsRelativeHubAddress(AST *ast)
{
    if (ast && ast->kind == AST_LOCAL_IDENTIFIER) {
        ast = ast->left;
    }
    if (!ast) return 0;
    switch(ast->kind) {
    case AST_INTEGER:
    case AST_HWREG:
        return 0;
    case AST_IDENTIFIER:
    {
        Symbol *sym = LookupSymbol(ast->d.string);
        Label *lab;
        if (!sym) return 0;
        if (sym->kind != SYM_LABEL) {
            return 0;
        }
        lab = (Label *)sym->val;
        return 0 != (lab->flags & LABEL_IN_HUB);
    }
    default:
        return IsRelativeHubAddress(ast->left) || IsRelativeHubAddress(ast->right);
    }
}

static uint32_t
EvalOperandExpr(Instruction *instr, AST *op)
{
    const char *iname = instr->name;
    int badAddr = 0;
    const char *opname = "";
    
    switch (op->kind) {
    case AST_CATCH:
        ERROR(op, "\\ absolute expression marker not valid for %s", iname);
        return 0;
    case AST_OPERATOR:
        if (op->d.ival == K_INCREMENT || op->d.ival == K_DECREMENT) {
            badAddr = 1;
            if (instr->ops == P2_RDWR_OPERANDS) {
                opname = "first operand of ";
            }
        }
        break;
    case AST_RANGEREF:
        badAddr = 1;
        if (instr->ops == P2_RDWR_OPERANDS) {
            opname = "first operand of ";
        }
        break;
    default:
        break;
    }
    if (badAddr) {
        ERROR(op, "invalid addressing mode for %s%s", opname, iname);
        return 0;
    }
    return EvalPasmExpr(op);
}

/*
 * assemble an instruction, along with its modifiers,
 * into a flexbuf
 */

void
AssembleInstruction(Flexbuf *f, AST *ast, Flexbuf *relocs)
{
    uint32_t val, src, dst;
    uint32_t immmask;
    uint32_t curpc;
    int inHub;
    int32_t isrc;
    Instruction *instr;
    int numoperands;
    AST *operand[MAX_OPERANDS];
    uint32_t opimm[MAX_OPERANDS];
    AST *line = ast;
    char *callname;
    AST *retast;
    AST *origast;
    int isRelJmp = 0;
    int isRelHubAddr = 0;
    int opidx;
    Reloc *rptr;
    int srcRelocOff = -1;
    int dstRelocOff = -1;
    bool needIndirect = false;
    bool compress = false;
    uint32_t effects = 0;
    
    extern Instruction instr_p2[];
    curpc = ast->d.ival & 0x00ffffff;
    inHub = (0 == (ast->d.ival & (1<<30)));

    if (relocs && ast->right) {
        AssembleComments(f, relocs, ast->right);
    }
    ast = AssembleComments(f, relocs, ast->left);
    if (ast->kind == AST_COMPRESS_INSTR) {
        compress = true;
        instr = (Instruction *)ast->left->d.ptr;
    } else {
        instr = (Instruction *)ast->d.ptr;
    }
decode_instr:
    origast = ast;
    val = instr->binary;
    if (instr->opc != OPC_NOP) {
        /* for anything except NOP set the condition to "always" */
        if (gl_p2) {
            val |= 0xf << 28;
        } else {
            val |= 0xf << 18;
        }
    }
    opidx = 0;

    numoperands = DecodeAsmOperands(instr, ast, operand, opimm, &val, &effects);
    if (numoperands < 0) return;

    immmask = 0;

    {
        int j;
        for (j = 0; j < numoperands; j++) {
            immmask |= opimm[j];
        }
    }
    
    src = dst = 0;
    switch (instr->ops) {
    case NO_OPERANDS:
        break;
    case P2_TWO_OPERANDS:
        if (!strcmp(instr->name, "rep")) {
            // special case: rep @x, N says to count the instructions up to x
            // and repeat them N times; fixup the operand here
            if (operand[0]->kind == AST_ADDROF && !opimm[0]) {
                int32_t label = EvalOperandExpr(instr, operand[0]->left);
                int32_t count;
                if (inHub) {
                    count = (label - (curpc+4)) / 4;
                } else {
                    count = (label - (curpc+4)/4);
                }
                // remove augment prefixes from the count
                if (immmask & BIG_IMM_SRC) {
                    count--;
                }
                operand[0] = AstInteger(count);
                opimm[0] = P2_IMM_DST;
                immmask |= P2_IMM_DST;
            }
        }
        // fall through
    case TWO_OPERANDS:
    case JMPRET_OPERANDS:
    case TWO_OPERANDS_OPTIONAL:
    case TWO_OPERANDS_DEFZ:
    handle_two_operands:
        dst = EvalRelocPasmExpr(operand[0], f, relocs, &dstRelocOff, true, RELOC_KIND_AUGD);
        src = EvalRelocPasmExpr(operand[1], f, relocs, &srcRelocOff, true, RELOC_KIND_AUGS);
        break;
    case P2_MODCZ:
        dst = EvalOperandExpr(instr, operand[0]);
        src = EvalOperandExpr(instr, operand[1]);
        if (dst > 0xf || src > 0xf) {
            ERROR(line, "bad operand for %s", instr->name);
            dst = src = 0;
        }
        dst = (dst<<4) | src;
        src = 0;
        break;
    case P2_RDWR_OPERANDS:
        dst = EvalOperandExpr(instr, operand[0]);
        if (gl_p2 == P2_REV_A && strstr(instr->name, "lut")) {
            // in the original P2 silicon the LUT instructions are just
            // regular two operand instructions
            src = EvalOperandExpr(instr, operand[1]);
        } else {
            src = SpecialRdOperand(operand[1], opimm[1]);
            if (src == 0) {
                src = EvalPasmExpr(operand[1]);
            } else {
                immmask |= P2_IMM_SRC;
            }
        }
        break;
    case P2_TJZ_OPERANDS:
        dst = EvalOperandExpr(instr, operand[0]);
        if (!strcmp(instr->name, "calld") && opimm[1] && (dst >= 0x1f6 && dst <= 0x1f9)) {
            int k = 0;
            // use the .loc version of this instruction
            // if we cannot reach the src address
            // or if we are requested to by the user
            if (operand[1]->kind == AST_CATCH) {
                goto force_loc;
            }
            isrc = EvalOperandExpr(instr, operand[1]);
            if (isrc < 0x400) {
                if (inHub) goto force_loc;
                isrc *= 4;
            } else if (!inHub) {
                goto force_loc;
            }
            isrc = (isrc - (curpc+4));
            if (0 != (isrc & 0x3)) {
                // "loc" required
                goto force_loc;
            }
            isrc /= 4;
            if ((isrc >= -256) && (isrc < 255)) {
                goto skip_loc;
            }
        force_loc:
            while (instr_p2[k].name && strcmp(instr_p2[k].name, "calld.loc") != 0) {
                k++;
            }
            if (!instr_p2[k].name) {
                ERROR(line, "Internal error in calld parsing");
                return;
            }
            instr = &instr_p2[k];
            ast = origast;
            goto decode_instr;
        }
    skip_loc:
        /* fall through */
    case P2_JINT_OPERANDS:
        opidx = (instr->ops == P2_TJZ_OPERANDS) ? 1 : 0;
        if (operand[opidx]->kind == AST_CATCH) {
            // asking for absolute address
            ERROR(line, "Absolute address not valid for %s", instr->name);
            isrc = 0;
        } else if (opimm[opidx]) {
            bool dstLut = false;
            bool dstHub = true;
            bool relHub = IsRelativeHubAddress(operand[opidx]);
            isrc = EvalOperandExpr(instr, operand[opidx]);
            if (isrc < 0x400 && !relHub) {
                dstHub = false;
                dstLut = (isrc >= 0x200);
                isrc *= 4;
            }
            if (inHub) {
                if (!dstHub) {
                    if (dstLut) {
                        ERROR(line, "%s branch crosses HUB/LUT boundary", instr->name);
                    } else {
                        ERROR(line, "%s branch crosses HUB/COG boundary", instr->name);
                    }
                    //isrc = curpc;
                }
            } else {
                if (dstLut && curpc < 0x800) {
#ifdef NEVER  /* crossing LUT/COG is OK  */
                    ERROR(line, "%s branch crosses LUT/COG boundary", instr->name);
                    isrc = curpc;
#endif
                } else if (!dstLut && !dstHub && curpc >= 0x800) {
#ifdef NEVER
                    ERROR(line, "%s branch crosses LUT/COG boundary", instr->name);
                    isrc = curpc;
#endif
                } else if (dstHub) {
                    WARNING(line, "%s branch crosses HUB/COG boundary", instr->name);
                }
            }

            isrc = (isrc - (int)(curpc+4)) / 4;
            if (immmask & BIG_IMM_DST) {
                isrc--;
            }
            if ( (isrc < -256) || (isrc > 255) ) {
                ERROR(line, "Source out of range for relative branch %s", instr->name);
                isrc = 0;
            }
            src = isrc & 0x1ff;
        } else {
            src = EvalPasmExpr(operand[opidx]);
        }
        break;
    case JMP_OPERAND:
    case SRC_OPERAND_ONLY:
    case P2_AUG:
        dst = 0;
        src = EvalOperandExpr(instr, operand[0]);
        break;
    case P2_LOC:
    case P2_CALLD:
        dst = EvalOperandExpr(instr, operand[0]);
        if (dst >= 0x1f6 && dst <= 0x1f9) {
            val |= ( (dst-0x1f6) & 0x3) << 21;
        } else {
            if (instr->ops == P2_CALLD) {
                ERROR(line, "internal error, indirect calld when immediate was given?");
            } else {
                ERROR(line, "bad first operand to %s instruction", instr->name);
            }
        }
        opidx = 1;
        // fall through
    case P2_JUMP:
        // indirect jump needs to be handled
        if (needIndirect || !opimm[opidx]) {
            char tempName[80];
            int k;
            if (instr->ops == P2_LOC && instr->opc != OPC_GENERIC_BRANCH) {
                ERROR(line, "loc requires immediate operand");
                return;
            }
            strcpy(tempName, instr->name);
            strcat(tempName, ".ind"); // convert to indirect
            k = 0;
            while (instr_p2[k].name && strcmp(tempName, instr_p2[k].name) != 0) {
                k++;
            }
            if (!instr_p2[k].name) {
                ERROR(line, "Internal error, could not find %s", tempName);
                return;
            }
            instr = &instr_p2[k];
            ast = origast;
            goto decode_instr;
        }
        // validate that no wc or wz was used (these are only valid on indirect jumps)
        if ( (effects&FLAG_JMPSET) != 0 ) {
            ERROR(line, "wc and wz not valid with this form of %s", instr->name);
        }
        dst = 0;
        immmask = 0; // handle immediates specially for jumps
        // figure out if absolute or relative
        // if  crossing from COG to HUB or vice-versa,
        // make it absolute
        // if it's of the form \xxx, which we parse as CATCH(xxx),
        // then make it absolute
        if (operand[opidx]->kind == AST_CATCH) {
            AST *realval = operand[opidx]->left;
            if (realval->kind == AST_FUNCCALL) {
                realval = realval->left;  // correct for parser misreading
            }
            isrc = EvalRelocPasmExpr(realval, f, relocs, &srcRelocOff, true, RELOC_KIND_I32);
            isRelJmp = 0;
            isRelHubAddr = 0;
        } else {
            isRelHubAddr = IsRelativeHubAddress(operand[opidx]);
            isrc = EvalRelocPasmExpr(operand[opidx], f, relocs, &srcRelocOff, true, RELOC_KIND_I32);
           
            if (inHub) {
                if (isRelHubAddr) {
                    isRelJmp = 1;
                } else if (gl_nospin) {
                    isRelJmp = (isrc >= 0x400);
                } else {
                    isRelJmp = 0;
                }
            } else {
                if (isrc >= 0x400) {
                    isRelJmp = 0;
                } else if (isrc >= 0x200) {
                    // destination in LUT
                    isRelJmp = (curpc >= 0x800);
                } else {
                    isRelJmp = (curpc < 0x800);
                }
            }
        }
        if (isRelJmp) {
            if (!inHub) {
                isrc *= 4;
            }
            isrc = isrc - (curpc+4);
            if (instr->ops == P2_LOC && !inHub) {
                isrc = isrc >> 2;
            }
            if (isrc > 0x7ffff || isrc < -0x80000) {
                ERROR(line, "Operand for %s is out of range", instr->name);
            }                
            src = isrc & 0xfffff;
            val = val | (1<<20);
            if (srcRelocOff >= 0 && isRelHubAddr) {
                // cancel the relocation
                Reloc *r = (Reloc *)(flexbuf_peek(relocs) + srcRelocOff);
                r->kind = RELOC_KIND_NONE;
            }
        } else {
            if ( (isrc > 0xfffff) || (isrc < 0) ) {
                ERROR(line, "Operand for %s is out of range", instr->name);
            }
            src = isrc & 0xfffff;
        }
        goto instr_ok;
    case DST_OPERAND_ONLY:
    case P2_DST_CONST_OK:
        dst = EvalRelocPasmExpr(operand[0], f, relocs, &dstRelocOff, true, RELOC_KIND_AUGD);
        src = 0;
        break;
    case CALL_OPERAND:
        if (operand[0]->kind != AST_IDENTIFIER) {
            ERROR(operand[0], "call operand must be an identifier");
            return;
        }
        src = EvalPasmExpr(operand[0]);
        callname = (char *)malloc(strlen(operand[0]->d.string) + 8);
        strcpy(callname, operand[0]->d.string);
        strcat(callname, "_ret");
        retast = NewAST(AST_IDENTIFIER, NULL, NULL);
        retast->d.string = callname;
        dst = EvalPasmExpr(retast);
        break;
    case THREE_OPERANDS_NIBBLE:
        val = FixupThreeOperands(val, operand[2], opimm[2], 8, line, instr);
        goto handle_two_operands;
    case THREE_OPERANDS_BYTE:
        val = FixupThreeOperands(val, operand[2], opimm[2], 4, line, instr);
        goto handle_two_operands;
    case THREE_OPERANDS_WORD:
        val = FixupThreeOperands(val, operand[2], opimm[2], 2, line, instr);
        goto handle_two_operands;
    default:
        ERROR(line, "Unsupported instruction `%s'", instr->name);
        return;
    }

    if (instr->ops == P2_AUG) {
        if (immmask == 0) {
            ERROR(line, "%s requires immediate operand", instr->name);
        }
        immmask = 0;
        src = src >> 9;
    } else {
        if (immmask & BIG_IMM_DST) {
            uint32_t augval = val & 0xf0000000; // preserve condition
            if (augval == 0) augval = 0xf0000000; // except _ret_
            if (dstRelocOff >= 0) {
                if (srcRelocOff >= 0) {
                    ERROR(line, "fastspin does not currently support two relocations on one instruction");
                }
                rptr = (Reloc *)(flexbuf_peek(relocs) + dstRelocOff);
                rptr->symoff = dst;
                dst = 0;
                dstRelocOff = -1;
            }
            augval |= (dst >> 9) & 0x007fffff;
            augval |= 0x0f800000; // AUGD
            dst &= 0x1ff;
            outputInstrLong(f, augval);
            immmask &= ~BIG_IMM_DST;
        } else if (dstRelocOff >= 0) {
            ERROR(line, "Use of immediate hub address in dest requires ##");
        }
        if (immmask & BIG_IMM_SRC) {
            uint32_t augval = val & 0xf0000000; // preserve condition
            if (augval == 0) augval = 0xf0000000; // except _ret_
            if (srcRelocOff >= 0) {
                rptr = (Reloc *)(flexbuf_peek(relocs) + srcRelocOff);
                rptr->symoff = src;
                src = 0;
                srcRelocOff = -1;
            }
            augval |= (src >> 9) & 0x007fffff;
            augval |= 0x0f000000; // AUGS
            src &= 0x1ff;
            outputInstrLong(f, augval);
            immmask &= ~BIG_IMM_SRC;
        } else if (srcRelocOff >= 0) {
            ERROR(line, "Use of immediate hub address in src requires ##");
        }
        if (src > 511) {
            ERROR(line, "Source operand too big for %s", instr->name);
            return;
        }
    }
    if (dst > 511) {
        ERROR(line, "Destination operand too big for %s", instr->name);
        return;
    }
instr_ok:
    val = val | (dst << 9) | src | (immmask & ~0xff);
    /* output the instruction */
    if (srcRelocOff >= 0) {
        rptr = (Reloc *)(flexbuf_peek(relocs) + srcRelocOff);
        rptr->symoff = val;
    }
    if (compress) {
        val = (val >> 14) | (val << 18);
    }
    outputInstrLong(f, val);
}

void
outputAlignedDataList(Flexbuf *f, int size, AST *ast, Flexbuf *relocs, int checkSize)
{
    if (size > 1 && NEED_ALIGNMENT) {
        AlignPc(f, size);
    }
    outputDataList(f, size, ast, relocs, checkSize);
}

/*
 * output bytes for a file
 */
static void
assembleFile(Flexbuf *f, AST *ast)
{
    FILE *inf;
    const char *name = ast->d.string;
    int c;

    inf = fopen(name, "rb");
    if (!inf) {
        ERROR(ast, "file %s: %s", name, strerror(errno));
        return;
    }
    while ((c = fgetc(inf)) >= 0) {
        outputByte(f, c);
    }
    fclose(inf);
}


void AssembleAsmDebug(Flexbuf *f,AST *ast) {
    if (!gl_p2) ERROR(ast,"ASM DEBUG is only supported on P2!");
    int brkCode = AsmDebug_CodeGen(ast, Pasm_DebugEval, NULL);
    if (brkCode>=0) {
        // Opcode for P2 BRK....
        outputInstrLong(f,0b11111101011001000000000000110110 + (brkCode<<9));
    } else {
        outputInstrLong(f,0);
    }
}

/*
 * output padding bytes
 */
static void
padBytes(Flexbuf *f, AST *ast, int bytes)
{
    while (bytes-- > 0) {
        outputByte(f, 0);
    }
}

/*
 * put a single commented source line in relocs
 */
static void
SendOneComment(Flexbuf *relocs, AST *ast, uint32_t addr)
{
    Reloc r;
    if (relocs && ast) {
        r.kind = RELOC_KIND_DEBUG;
        r.addr = addr;
        r.sym = (Symbol *)GetLineInfo(ast);
        r.symoff = 0;
        flexbuf_addmem(relocs, (const char *)&r, sizeof(r));
    }
}
        
/*
 * send out all comments pending, and return the next non-comment node
 */
AST *
SendComments(Flexbuf *f, AST *ast, Flexbuf *relocs)
{
    while (ast) {
        switch (ast->kind) {
        case AST_COMMENT:
            /* ignore, for now */
            break;
        case AST_SRCCOMMENT:
            SendOneComment(relocs, ast, flexbuf_curlen(f));
            break;
        default:
            return ast;
        }
        ast = ast->right;
    }
    return ast;
}

static void
outputVarDeclare(Flexbuf *f, AST *ast, Flexbuf *relocs)
{
    AST *initval = NULL;
    AST *type = ast->left;
    
    ast = ast->right;
    if (ast->kind == AST_ASSIGN) {
        initval = ast->right;
        ast = ast->left;
    }
    while (ast && ast->kind == AST_ARRAYDECL) {
        type = NewAST(AST_ARRAYTYPE, type, ast->right);
        ast = ast->left;
    }
    outputInitializer(f, type, initval, relocs);
}

/*
 * print out a data block
 */
void
PrintDataBlock(Flexbuf *f, AST *list, DataBlockOutFuncs *funcs, Flexbuf *relocs)
{
    AST *ast;
    AST *top;
    int inHub = 0;
    
    void (*startAst)(Flexbuf *f, AST *ast) = NULL;
    void (*endAst)(Flexbuf *f, AST *ast) = NULL;
    
    initDataOutput(funcs);
    if (funcs) {
        startAst = funcs->startAst;
        endAst = funcs->endAst;
    }
    
    if (gl_errors != 0)
        return;
    top = list;
    while (top) {
        ast = top;
        if (top->kind == AST_COMMENTEDNODE) {
            top = SendComments(f, top->right, relocs);
        } else {
            top = top->right;
        }
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            if (startAst) {
                (*startAst)(f, ast);
            }
            if (endAst) {
                (*endAst)(f, ast);
            }
            ast = ast->left;
        }
        if (!ast) continue;
        if (startAst) {
            (*startAst)(f, ast);
        }
        switch (ast->kind) {
        case AST_BYTELIST:
            outputAlignedDataList(f, 1, ast->left, relocs, 0);
            break;
        case AST_BYTEFITLIST:
            outputAlignedDataList(f, 1, ast->left, relocs, CHECKSIZE_ANY);
            break;
        case AST_WORDLIST:
            outputAlignedDataList(f, 2, ast->left, relocs, 0);
            break;
        case AST_WORDFITLIST:
            outputAlignedDataList(f, 2, ast->left, relocs, CHECKSIZE_ANY);
            break;
        case AST_LONGLIST:
            outputAlignedDataList(f, 4, ast->left, relocs, 0);
            break;
        case AST_ALIGN:
        {
            int size = EvalPasmExpr(ast->left);
            AlignPc(f, size);
            break;
        }
        case AST_INSTRHOLDER:
            /* make sure it is aligned */
            if (NEED_ALIGNMENT || (!inHub) ) {
                while ((datacount % 4) != 0) {
                    outputByte(f, 0);
                }
            }
            AssembleInstruction(f, ast, relocs);
            break;
        case AST_BRKDEBUG:
            if (!gl_brkdebug) WARNING(ast,"Internal error, Got AST_BRKDEBUG, but BRK debugger is not enabled?");
            if (NEED_ALIGNMENT || (!inHub) ) {
                while ((datacount % 4) != 0) {
                    outputByte(f, 0);
                }
            }
            AssembleAsmDebug(f,ast);

            break;
        case AST_IDENTIFIER:
            /* just skip labels */
            break;
        case AST_DECLARE_VAR:
            outputVarDeclare(f, ast, relocs);
            break;
        case AST_FILE:
            assembleFile(f, ast->left);
            break;
        case AST_ORGH:
            if (!gl_nospin && ast->d.ival > 3 && gl_output != OUTPUT_DAT) {
                WARNING(ast, "orgh with explicit origin does not work if Spin methods are present");
            }
            /* skip ahead to PC */
            padBytes(f, ast, ast->d.ival);
            inHub = 1;
            break;

        case AST_ORGF:
            /* need to skip ahead to PC */
            padBytes(f, ast, ast->d.ival);
            inHub = 0;
            break;
        case AST_ORG:
            inHub = 0;
            if (NEED_ALIGNMENT) {
                AlignPc(f, 4);
            }
            break;
        case AST_RES:
        case AST_FIT:
            // emit a debug entry
            SendOneComment(relocs, ast, flexbuf_curlen(f));
            break;
        case AST_LINEBREAK:
            break;
        case AST_COMMENT:
            break;
        case AST_SRCCOMMENT:
            // emit a debug entry
            SendOneComment(relocs, ast, flexbuf_curlen(f));
            break;
        default:
            ERROR(ast, "unknown element in data block");
            break;
        }
        if (endAst) {
            (*endAst)(f, ast);
        }
    }
}
