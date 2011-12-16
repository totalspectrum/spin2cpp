//
// Microtests for lexical analyzer
//
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "lexer.h"

static void EXPECTEQfn(long x, long val, int line) {
    if (x != val) {
        fprintf(stderr, "test failed at line %d of %s: expected %ld got %ld\n",
                line, __FILE__, val, x);
        abort();
    }
}

#define EXPECTEQ(x, y) EXPECTEQfn((x), (y), __LINE__)

static void
testNumber(const char *str, long val)
{
    TokenType tok;
    LexStream L;
    int t;
    int c;
    printf("testing number[%s]...", str);
    strToLex(&L, str);
    t = getToken(&L, &tok);
    EXPECTEQ(t, T_NUM);
    c = lexgetc(&L);
    EXPECTEQ(c, T_EOF);
    EXPECTEQ(tok.val, val);
    printf("passed\n");
}

static void
testTokenStream(const char *str, int *tokens, int numtokens)
{
    int i;
    LexStream L;
    TokenType tok;
    int t;

    printf("testing tokens [%s]...", str);
    strToLex(&L, str);
    for (i = 0; i < numtokens; i++) {
        t = getToken(&L, &tok);
        EXPECTEQ(t, tokens[i]);
    }
    printf("passed\n");
}

#define N_ELEM(x) (sizeof(x)/sizeof(x[0]))
static int tokens0[] = { T_NUM, '+', T_NUM, T_EOF };

int
main()
{
    testTokenStream("1 + 1", tokens0, N_ELEM(tokens0));
    testTokenStream("'' a comment line\n $1 + 1", tokens0, N_ELEM(tokens0));
    testTokenStream("{a comment line} $1 + 1", tokens0, N_ELEM(tokens0));
    testTokenStream("{{a nested comment}} $1 + 1", tokens0, N_ELEM(tokens0));

    testNumber("0", 0);
    testNumber("00", 0);
    testNumber("007", 7);
    testNumber("008", 8);
    testNumber("\t \t 123", 123);
    testNumber("65535", 65535);
    testNumber("  $41", 65);
    testNumber("$01_ff", 511);
    testNumber("$A5", 165);
    testNumber("%101", 5);
    testNumber("%11", 3);
    testNumber("%%31", 13);
    testNumber("80_000_000", 80000000);
    printf("all tests passed\n");
    return 0;
}
