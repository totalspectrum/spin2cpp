//
// Commented byte buffer for spin2cpp
//
// Copyright 2021 Ada Gottensträter and Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include "bcbuffers.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

OutputSpan *BOB_PushSpan(ByteOutputBuffer* buf, OutputSpan* newSpan) {
    if (buf->tail != NULL) buf->tail->next = newSpan;
    buf->tail = newSpan;
    if (buf->head == NULL) buf->head = newSpan;

    buf->total_size += newSpan->size;

    return newSpan;
}

OutputSpan *BOB_Push(ByteOutputBuffer *buf,uint8_t *data,int data_size,const char *comment) {
    OutputSpan *newSpan = calloc(sizeof(OutputSpan)+data_size,1);
    if (!newSpan) {
        ERROR(NULL,"Out of memory (while allocating OutputSpan)");
        exit(1);
    }

    if (data&&data_size) memcpy(newSpan->data,data,data_size);
    newSpan->size = data_size;
    newSpan->comment = comment;

    return BOB_PushSpan(buf,newSpan);
}

void BOB_Replace(OutputSpan *span,uint8_t *data,int data_size,const char *comment) {
    if (!span) return;
    if (data && span->size != data_size) {
        ERROR(NULL,"Error replacing span data: data size(%d) doesn't match span size(%d)",data_size,span->size);
        return;
    }
    if (data) memcpy(span->data,data,data_size);
    if(comment) span->comment = comment;
}

OutputSpan *BOB_PushByte(ByteOutputBuffer *buf,uint8_t data,const char *comment) {
    return BOB_Push(buf,&data,1,comment);
}
OutputSpan *BOB_PushWord(ByteOutputBuffer *buf,uint16_t data,const char *comment) {
    #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        data = bswap16(data);
    #endif
    return BOB_Push(buf,(uint8_t*)&data,2,comment);
}
OutputSpan *BOB_PushLong(ByteOutputBuffer *buf,uint32_t data,const char *comment) {
    #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        data = bswap32(data);
    #endif
    return BOB_Push(buf,(uint8_t*)&data,4,comment);
}

void BOB_ReplaceByte(OutputSpan *span,uint8_t data,const char *comment) {
    return BOB_Replace(span,&data,1,comment);
}
void BOB_ReplaceWord(OutputSpan *span,uint16_t data,const char *comment) {
    #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        data = bswap16(data);
    #endif
    return BOB_Replace(span,(uint8_t*)&data,2,comment);
}
void BOB_ReplaceLong(OutputSpan *span,uint32_t data,const char *comment) {
    #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        data = bswap32(data);
    #endif
    return BOB_Replace(span,(uint8_t*)&data,4,comment);
}



int BOB_Align(ByteOutputBuffer *buf,int alignment) {
    int pad = alignment - (buf->total_size%alignment);
    if (pad != alignment) {
        BOB_Push(buf,NULL,pad,"(padding)");
        return pad;
    } else return 0;
}

// fixup bytes in an existing ByteOutputBuffer
void BOB_FixupData(ByteOutputBuffer *bob, uint32_t fixaddr, uint8_t *data, size_t size)
{
    uint32_t off = 0;
    OutputSpan *sp;
    if (size > (size_t)bob->total_size) {
        ERROR(NULL, "fixup beyond end of buffer");
        return;
    }
    for (sp = bob->head; sp; sp = sp->next) {
        if (off <= fixaddr && fixaddr < off + sp->size) {
            break;
        }
        off += sp->size;
    }
    if (!sp || fixaddr + size > off + sp->size) {
        ERROR(NULL, "bad fixup");
    }
    uint8_t *ptr = &sp->data[fixaddr - off];
    for (int i = 0; i < size; i++) {
        *ptr++ = *data++;
    }
}

void BOB_FixupWord(ByteOutputBuffer *bob,uint32_t addr,uint16_t data) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    data = bswap16(data);
#endif
    BOB_FixupData(bob,addr,(uint8_t *)&data,sizeof(uint16_t));
}

void BOB_FixupLong(ByteOutputBuffer *bob,uint32_t addr,uint32_t data) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    data = bswap32(data);
#endif
    BOB_FixupData(bob,addr,(uint8_t *)&data,sizeof(uint32_t));
}
