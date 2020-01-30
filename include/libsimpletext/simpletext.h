#ifndef SIMPLETEXT_H_
#define SIMPLETEXT_H_
#include <stdarg.h>
#include <compiler.h>

#define TERM_NAME_LEN 20
#define TERM_COG_LEN 7

/**
 * @brief Structure that contains data used by simple text device libraries.
 */
typedef struct text_struct
{
  /** Pointer to text device library's character receiving function. */
  int  (*rxChar)(struct text_struct *p);         
  /** Pointer to text device library's character transmitting function. */
  int  (*txChar)(struct text_struct *p, int ch); 
  /** Pointer to text device library's cog variable(s). */      
  int  cogid[TERM_COG_LEN];                      
  /** Pointer to text device library's info. */ 
  volatile void *devst;                          
  /** Echo setting, typically for usage with a terminal. */ 
  volatile int terminalEcho;                          
  /** List of end characters. */ 
  //char ec[3];                          
  /** End character sequence when an end character is encountered. */ 
  //char ecs[3];                          
  volatile char ecA;
  volatile char ecB;
  volatile char ecsA;
  volatile char ecsB;
} text_t;

char *_safe_gets(text_t *text, char *buf, int count) _IMPL("libsimpletext/safe_gets.c");
char *getStr(char *buf, int max) _IMPL("libsimpletext/getStr.c");

float string2float(char *s, char **end) _IMPL("libsimpletext/stringToFloat.c");
int _doscanf(const char *str, const char *fmt, va_list args) _IMPL("libsimpletext/doscanf.c");

const char* _scanf_getl(const char *str, int *dst, int base, unsigned width, int isSigned) _IMPL("libsimpletext/scanf_getl.c");
const char* _scanf_getf(const char *str, float *dst) _IMPL("libsimpletext/scanf_getf.c");

int scan(const char *fmt, ...) _IMPL("libsimpletext/scan.c");

#endif
