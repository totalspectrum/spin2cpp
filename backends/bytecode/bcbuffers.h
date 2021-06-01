
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


char *auto_printf(size_t max,const char *format,...) __attribute__((format(printf,2,3)));

#endif
