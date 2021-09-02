#ifndef BACKEND_COMMON_H
#define BACKEND_COMMON_H

#include "ast.h"
#include "symbol.h"
#include "expr.h"

AST *BuildMethodPointer(AST *ast);
void OutputAlignLong(Flexbuf *fb);
void OutputDataBlob(Flexbuf *fb, Flexbuf *databuf, Flexbuf *relocbuf, const char *startLabel);

void NormalizeVarOffsets(Function *f);

void CompileAsmToBinary(const char *binname, const char *asmname); // in cmdline.c

// evaluate any constant expressions inside a string
AST *EvalStringConst(AST *expr);

// turn an AST stringptr into a flexbuf buffer
void StringBuildBuffer(Flexbuf *fb, AST *expr);

// print into a freshly allocated string
char *auto_printf(size_t max,const char *format,...) __attribute__((format(printf,2,3)));

// output for DAT blocks
/* functions for printing data into a flexbuf */
typedef struct DataBlockOutFuncs {
    void (*startAst)(Flexbuf *f, AST *ast);
    void (*putByte)(Flexbuf *f, int c);
    void (*endAst)(Flexbuf *f, AST *ast);
} DataBlockOutFuncs;

/*
 * information about relocations:
 * data blocks are normally just binary blobs; but if an absolute address
 * (like @@@foo) is requested, we need a way to specify that address
 * Note that a normal @foo in a dat section is a relative address;
 * @@@foo requires that we add the base of the dat to @foo.
 * For now, the relocation system works only on longs, and only in some
 * modes. For each long that needs the base of dat added to it we emit
 * a relocation r, which contains (a) the offset of the relocatable long
 * in bytes from the start of the dat, and (b) the symbol to add to the base
 * of the dat section at that long.
 * The relocs should be sorted in order of increasing offset, so we can
 * easily process them in order along with the output.
 *
 * We also re-use the Reloc struct to hold debug information as well, so
 * that we can provide source listings for the DAT section contents.
 * for that purpose we emit DebugEntry
 */
typedef struct Reloc {
    int32_t  kind;    // reloc or debug
    int32_t  addr;    // the address the entry affects (offset from dat base)
    Symbol   *sym;    // the symbol this relocation is relative to (NULL for dat base)
    int32_t  symoff;  // offset relative to sym
} Reloc;

#define RELOC_KIND_NONE  0  // no relocation, should not produce an entry
#define RELOC_KIND_DEBUG 1  // not a real relocation, a debug entry
#define RELOC_KIND_I32   2  // add a symbol to a 32 bit value
#define RELOC_KIND_AUGS  3  // relocation for AUGS
#define RELOC_KIND_AUGD  4  // relocation for AUGD

/* for PASM debug */
#define MAX_BRK 256
extern unsigned brkAssigned; // Currently assigned BRK codes
int AsmDebug_CodeGen(AST *ast);

void PrintDataBlock(Flexbuf *f, AST *list, DataBlockOutFuncs *funcs, Flexbuf *relocs);
void PrintDataBlockForGas(Flexbuf *f, Module *P, int inlineAsm);

#endif /* BACKEND_COMMON_H */
