
// commented byte buffer structure used in the bytecode backend

#ifndef BCBUFFERS_H
#define BCBUFFERS_H

#include "spinc.h"

typedef struct outspanstruct {
    struct outspanstruct *next;
    const char *comment;
    int size;
    uint8_t data[]; // FAM
} OutputSpan;

typedef struct {
    OutputSpan *head,*tail;
    int total_size;
} ByteOutputBuffer;

typedef void (*BCRelocFunc)(Module *P, uint8_t *where, uint32_t addr);

typedef struct BCRelocList {
    struct BCRelocList *next;
    BCRelocFunc func;
    uint8_t *pos;
    Module *M;
} BCRelocList;

OutputSpan *BOB_PushSpan(ByteOutputBuffer *buf,OutputSpan *span);
OutputSpan *BOB_Push(ByteOutputBuffer *buf,uint8_t *data,int data_size,const char *comment);
OutputSpan *BOB_PushByte(ByteOutputBuffer *buf,uint8_t data,const char *comment);
OutputSpan *BOB_PushWord(ByteOutputBuffer *buf,uint16_t data,const char *comment);
OutputSpan *BOB_PushLong(ByteOutputBuffer *buf,uint32_t data,const char *comment);
void BOB_Replace(OutputSpan *span,uint8_t *data,int data_size,const char *comment);
void BOB_ReplaceByte(OutputSpan *span,uint8_t data,const char *comment);
void BOB_ReplaceWord(OutputSpan *span,uint16_t data,const char *comment);
void BOB_ReplaceLong(OutputSpan *span,uint32_t data,const char *comment);
int BOB_Align(ByteOutputBuffer *buf,int alignment);
static inline void BOB_Comment(ByteOutputBuffer *buf,const char *comment) {BOB_Push(buf,NULL,0,comment);}

void BOB_FixupByte(ByteOutputBuffer *buf,uint32_t addr,uint8_t data);
void BOB_FixupWord(ByteOutputBuffer *buf,uint32_t addr,uint16_t data);
void BOB_FixupLong(ByteOutputBuffer *buf,uint32_t addr,uint32_t data);

#endif
