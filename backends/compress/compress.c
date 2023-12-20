//
// Executable compressor for Flexspin
//
// Copyright 2023 Ada Gottensträter and Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include <stdlib.h>

#include "compress.h"
#include "becommon.h"
#include "lz4/lz4.h"
#include "lz4/lz4hc.h"

#include "sys/p2_lz4stub.spin.h"


Flexbuf CompressExecutable(const char *inPtr, int inSize) {

    Flexbuf f;
    char *compressBuffer = NULL;
    flexbuf_init(&f,16*1024);
    
    if (!gl_p2) {
        WARNING(NULL,"Executable compression is not supported for P1 (yet?)");
        goto fail;
    }

    int stubSize = (int)CompileStub(&f,"__lz4stub__",(const char*)sys_p2_lz4stub_spin,sys_p2_lz4stub_spin_len);
    
    // TODO add method to resize flexbuf, could eliminate malloc and copy
    compressBuffer = (char*)malloc(LZ4_COMPRESSBOUND(inSize));
    int compressedSize = LZ4_compress_HC(inPtr,compressBuffer,inSize,LZ4_COMPRESSBOUND(inSize),LZ4HC_CLEVEL_MAX);

    int totalSize = stubSize + 4 + compressedSize;
    printf("Executable compressed from %d to %d bytes (%.2f%%, %d stub bytes)\n",inSize,totalSize,(double)totalSize / (double)inSize * 100.0,stubSize+4);
    if (totalSize >= inSize) {
        WARNING(NULL,"Compressed executable larger than original, falling back");
        goto fail;
    }
    // Fill compressed data size
    flexbuf_putc((compressedSize>> 0)&255,&f);
    flexbuf_putc((compressedSize>> 8)&255,&f);
    flexbuf_putc((compressedSize>>16)&255,&f);
    flexbuf_putc((compressedSize>>24)&255,&f);
    // copy buffer
    flexbuf_addmem(&f,compressBuffer,compressedSize);
    free(compressBuffer);
    compressBuffer = NULL;

    return f;
fail:
    if (compressBuffer) free(compressBuffer);
    flexbuf_clear(&f);
    flexbuf_addmem(&f,inPtr,inSize);
    return f;
}

extern int spinyyparse(void); // Still Very cool

size_t CompileStub(Flexbuf *f, const char *moduleName, const char* srcPtr, size_t srcLen) {
    size_t prevlen = flexbuf_curlen(f);
    int old_errors = gl_errors;
    gl_errors = 0; // Have to do this to not segfault when there've been errors
    gl_caseSensitive = false;
    Module *D = NewModule(moduleName,LANG_SPIN_SPIN2);
    current = D;
    D->Lptr = (LexStream *)calloc(sizeof(*systemModule->Lptr), 1);
    D->Lptr->flags |= LEXSTREAM_FLAG_NOSRC;
    strToLex(D->Lptr, srcPtr, srcLen, moduleName, LANG_SPIN_SPIN2);
    spinyyparse();
    ProcessModule(D);
    // We good now?
    PrintDataBlock(f,D->datblock,NULL,NULL);
    gl_errors = old_errors;
    return flexbuf_curlen(f) - prevlen;
}