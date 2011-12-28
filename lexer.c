//
// Simple lexical analyzer for a language where indentation
// is significant
//
// Copyright (c) 2011 Total Spectrum Software Inc.
//
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "spinc.h"

SymbolTable reservedWords;
SymbolTable pasmWords;

/* functions for handling string streams */
static int 
strgetc(LexStream *L)
{
    char *s;
    int c;

    s = (char *)L->ptr;
    c = (*s++) & 0x00ff;
    L->ptr = s;
    return (c == 0) ? T_EOF : c;
}

/* open a stream from a string s */
void strToLex(LexStream *L, const char *s)
{
    memset(L, 0, sizeof(*L));
    L->arg = L->ptr = (void *)s;
    L->getcf = strgetc;
    L->lineCounter = 1;
    L->fileName = "<string>";
}

/* functions for handling FILE streams */
/* filegetc is for ASCII streams;
   filegetwc is for UCS-16LE streams
*/

static int 
filegetc(LexStream *L)
{
    FILE *f;
    int c;

    f = (FILE *)L->ptr;
    c = fgetc(f);
    return (c >= 0) ? c : T_EOF;
}

static int 
filegetwc(LexStream *L)
{
    FILE *f;
    int c1, c2;

    f = (FILE *)L->ptr;
    c1 = fgetc(f);
    if (c1 < 0) return T_EOF;
    c2 = fgetc(f);
    if (c2 != 0) {
        /* FIXME: should convert to UTF-8 */
        return 0xff;
    }
    return c1;
}

/* open a stream from a FILE f */
void fileToLex(LexStream *L, FILE *f, const char *name)
{
    int c1, c2;

    memset(L, 0, sizeof(*L));
    L->ptr = (void *)f;
    L->arg = NULL;
    L->getcf = filegetc;
    L->lineCounter = 1;
    L->fileName = name;

    /* check for Unicode */
    c1 = fgetc(f);
    c2 = fgetc(f);
    if (c1 == 0xff && c2 == 0xfe) {
        L->getcf = filegetwc;
    } else {
        rewind(f);
    }
}

/*
 *
 */
int
lexgetc(LexStream *L)
{
    if (L->ungot_valid) {
        L->ungot_valid = 0;
        return L->ungot;
    }
    return (L->getcf(L));
}

void
lexungetc(LexStream *L, int c)
{
    if (L->ungot_valid) {
        fprintf(stderr, "ERROR: unget limit exceeded");
    }
    L->ungot_valid = 1;
    L->ungot = c;
}


/*
 * utility functions
 */
int
isIdentifierStart(int c)
{
    if (isalpha(c))
        return 1;
    if (c == '_')
        return 1;
    return 0;
}

int
isIdentifierChar(int c)
{
    return isIdentifierStart(c) || isdigit(c);
}

/*
 * actual parsing functions
 */
static int
parseNumber(LexStream *L, unsigned int base, uint32_t *num)
{
    unsigned long uval, digit;
    unsigned int c;
    int sawdigit = 0;

    uval = 0;

    for(;;) {
        c = lexgetc(L);
        if (c == '_')
            continue;
        else if (c >= 'A' && c <= 'Z')
            digit = 10 + c - 'A';
        else if (c >= 'a' && c <= 'z')
            digit = 10 + c - 'a';
        else
            digit = (c - '0');
        if (digit < base) {
            uval = base * uval + digit;
            sawdigit = 1;
        } else {
            break;
        }
    }
    lexungetc(L, c);
    *num = uval;
    return sawdigit ? T_NUM : ((base == 16) ? T_HERE : '%');
}

/* dynamically grow a string */
#define INCSTR 16

static void
addchar(int c, char **place, size_t *space, size_t *len)
{
    if (*len + 1 > *space) {
        *space += 16;
        *place = realloc(*place, *space);
    }
    assert(*place != NULL);
    (*place)[*len] = c;
    *len += 1;
}

/* parse an identifier */
static int
parseIdentifier(LexStream *L, AST **ast_ptr, const char *prefix)
{
    int c;
    char *place = NULL;
    size_t space = 0;
    size_t len = 0;
    Symbol *sym;
    AST *ast;

    if (prefix) {
        place = strdup(prefix);
        space = len = strlen(prefix);
        addchar(':', &place, &space, &len);
    }
    c = lexgetc(L);
    while (isIdentifierChar(c)) {
        addchar(c, &place, &space, &len);
        c = lexgetc(L);
    }
    addchar('\0', &place, &space, &len);
    lexungetc(L, c);

    /* check for reserved words */
    if (L->in_block == T_DAT) {
        sym = FindSymbol(&pasmWords, place);
        if (sym) {
            free(place);
            if (sym->type == SYM_INSTR) {
                ast = NewAST(AST_INSTR, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return T_INSTR;
            }
            if (sym->type == SYM_INSTRMODIFIER) {
                ast = NewAST(AST_INSTRMODIFIER, NULL, NULL);
                ast->d.ival = (int32_t)(intptr_t)sym->val;
                *ast_ptr = ast;
                return T_INSTRMODIFIER;
            }
            fprintf(stderr, "Internal error: Unknown pasm symbol type %d\n", sym->type);
        }
    }
    sym = FindSymbol(&reservedWords, place);
    if (sym != NULL) {
        if (sym->type == SYM_BUILTIN)
            goto is_identifier;

        free(place);
        if (sym->type == SYM_RESERVED) {
            c = INTVAL(sym);
            /* check for special handling */
            if (c == T_PUB || c == T_PRI) {
                L->in_block = T_PUB;
            } else if (c == T_DAT || c == T_OBJ || c == T_VAR || c == T_CON) {
                L->in_block = c;
            }
            return c;
        }
        if (sym->type == SYM_HWREG) {
            ast = NewAST(AST_HWREG, NULL, NULL);
            ast->d.ptr = sym->val;
            *ast_ptr = ast;
            return T_HWREG;
        }
        fprintf(stderr, "Internal error: Unknown symbol type %d\n", sym->type);
    }

is_identifier:
    ast = NewAST(AST_IDENTIFIER, NULL, NULL);
    ast->d.string = place;
    *ast_ptr = ast;
    return T_IDENTIFIER;
}

//
// skip over comments and spaces
// return first non-comment non-space character
// if we are inside a function, emit T_INDENT when
// we increase the indent level, T_OUTDENT when we
// decrease it
//
#define TAB_STOP 8

int
skipSpace(LexStream *L)
{
    int c;
    int commentNest;
    int indent = 0;

    if (L->emit_outdents > 0) {
        --L->emit_outdents;
        return T_OUTDENT;
    }

again:
    c = lexgetc(L);
    while (c == ' ' || c == '\t' || c == '\r') {
        if (L->eoln) {
            if (c == '\t') {
                indent = TAB_STOP * ((indent + (TAB_STOP-1))/TAB_STOP);
            } else {
                indent++;
            }
        }
        c = lexgetc(L);
    }
    if (L->in_block == T_PUB && L->eoln) {
        if (indent > L->indent[L->indentsp]) {
            lexungetc(L, c);
            if (L->indentsp >= MAX_INDENTS) {
                ERROR("too much nesting");
                return T_INDENT;
            }
            L->indent[++L->indentsp] = indent;
            L->eoln = 0;
            return T_INDENT;
        }
        if (indent < L->indent[L->indentsp] && L->indentsp > 0) {
            lexungetc(L, c);
            while (indent < L->indent[L->indentsp] && L->indentsp > 0) {
                --L->indentsp;
                ++L->emit_outdents;
            }
            --L->emit_outdents;
            L->eoln = 0;
            return T_OUTDENT;
        }
    }
    L->eoln = 0;

    /* single quote comments */
    if (c == '\'') {
        do {
            c = lexgetc(L);
        } while (c != '\n' && c != T_EOF);
        if (c == '\n') {
            L->lineCounter++;
            L->eoln = 1;
            return T_EOLN;
        }
    }
    if (c == '{') {
        commentNest = 1;
        do {
            c = lexgetc(L);
            if (c == '\n')
                L->lineCounter++;
            if (c == '{')
                commentNest++;
            else if (c == '}')
                --commentNest;
        } while (commentNest > 0 && c != T_EOF);
        if (c == T_EOF)
            return c;
        goto again;
    }
    if (c == '\n') {
        L->lineCounter++;
        L->eoln = 1;
        return T_EOLN;
    }
    return c;
}

static char operator_chars[] = "-+*/|<>=!@~#^.";

int
getToken(LexStream *L, AST **ast_ptr)
{
//    int base = 10;
    int c;
    AST *ast = NULL;
    int at_startofline = (L->eoln == 1);

    c = skipSpace(L);

    if (isdigit(c)) {
        lexungetc(L,c);
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = parseNumber(L, 10, &ast->d.ival);
    } else if (c == '$') {
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = parseNumber(L, 16, &ast->d.ival);
    } else if (c == '%') {
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = lexgetc(L);
        if (c == '%') {
            c = parseNumber(L, 4, &ast->d.ival);
        } else {
            lexungetc(L, c);
            c = parseNumber(L, 2, &ast->d.ival);
        }
    } else if (isIdentifierStart(c)) {
        lexungetc(L, c);
        c = parseIdentifier(L, &ast, NULL);
        /* if in pasm, and at start of line, restart temporary
           labels */
        if (c == T_IDENTIFIER && L->in_block == T_DAT && at_startofline) {
            L->lastGlobal = ast->d.string;
        }
    } else if (c == ':') {
        int peekc = lexgetc(L);
        if (peekc == '=') {
            c = T_ASSIGN;
        } else if (isIdentifierStart(peekc) && L->in_block == T_DAT && L->lastGlobal) {
            lexungetc(L, peekc);
            c = parseIdentifier(L, &ast, L->lastGlobal);
        } else {
            lexungetc(L, peekc);
        }
    } else if (strchr(operator_chars, c) != NULL) {
        char op[6];
        int i;
        int token;
        Symbol *sym = NULL;

        op[0] = token = c;
        for (i = 1; i < sizeof(op)-1; i++) {
            c = lexgetc(L);
            if (strchr(operator_chars, c) == NULL) {
                lexungetc(L, c);
                break;
            }
            op[i] = c;
            op[i+1] = 0;
            sym = FindSymbol(&reservedWords, op);
            if (sym) {
                token = INTVAL(sym);
            } else {
                lexungetc(L, c);
                break;
            }
        }
        c = token;
    }
    *ast_ptr = ast;
    return c;
}


/*
 * function to initialize the lexer
 */
struct reservedword {
    const char *name;
    intptr_t val;
} init_words[] = {
    { "byte", T_BYTE },
    { "con", T_CON },
    { "dat", T_DAT },

    { "else", T_ELSE },
    { "elseif", T_ELSEIF },
    { "elseifnot", T_ELSEIFNOT },
    { "if", T_IF },
    { "ifnot", T_IFNOT },

    { "long", T_LONG },

    { "obj", T_OBJ },
    { "org", T_ORG },
    { "pri", T_PRI },
    { "pub", T_PUB },
    { "repeat", T_REPEAT },
    { "return", T_RETURN },

    { "until", T_UNTIL },

    { "var", T_VAR },

    { "while", T_WHILE },
    { "word", T_WORD },

    /* operators */
    { "+", '+' },
    { "-", '-' },
    { "/", '/' },
    { "//", T_MODULUS },
    { "*", '*' },
    { "**", T_HIGHMULT },
    { ">", '>' },
    { "<", '<' },
    { "=<", T_LE },
    { "=>", T_GE },
    { "=", '=' },
    { "==", T_EQ },
    { "<>", T_NE },

    { "<<", T_SHL },
    { ">>", T_SHR },
    { "~>", T_SAR },

    { "..", T_DOTS },
    { "|<", T_DECODE },
    { ">|", T_ENCODE },
    { "||", T_ABS },
    { "#>", T_LIMITMIN },
    { "#<", T_LIMITMAX },

    { "~~", T_DOUBLETILDE },
};

extern void defaultBuiltin(FILE *, Builtin *, AST *);

Builtin builtinfuncs[] = {
    { "cognew", 2, defaultBuiltin, "cognew" },
    { "locknew", 0, defaultBuiltin, "locknew" },
    { "lockset", 1, defaultBuiltin, "lockset" },
    { "lockclr", 1, defaultBuiltin, "lockclr" },
    { "lockret", 1, defaultBuiltin, "lockret" },
};

void
initLexer(void)
{
    int i;

    /* add our reserved words */
    for (i = 0; i < N_ELEMENTS(init_words); i++) {
        AddSymbol(&reservedWords, init_words[i].name, SYM_RESERVED, (void *)init_words[i].val);
    }

    /* add builtin functions */
    for (i = 0; i < N_ELEMENTS(builtinfuncs); i++) {
        AddSymbol(&reservedWords, builtinfuncs[i].name, SYM_BUILTIN, (void *)&builtinfuncs[i]);
    }

    /* add the PASM instructions */
    InitPasm();
}

Instruction
instr[] = {
    { "abs",    0xa8800000, TWO_OPERANDS },
    { "absneg", 0xac800000, TWO_OPERANDS },
    { "add",    0x80800000, TWO_OPERANDS },
    { "addabs", 0x88800000, TWO_OPERANDS },
    { "adds",   0xd0800000, TWO_OPERANDS },
    { "addsx",  0xd8800000, TWO_OPERANDS },
    { "addx",   0xc8800000, TWO_OPERANDS },
    { "and",    0x60800000, TWO_OPERANDS },
    { "andn",   0x64800000, TWO_OPERANDS },

    { "call",   0x5cc00000, CALL_OPERAND },
    { "clkset", 0x0c400000, DST_OPERAND_ONLY },
    { "cmp",    0x84000000, TWO_OPERANDS },
    { "cmps",   0xc0000000, TWO_OPERANDS },
    { "cmpsx",  0xc4000000, TWO_OPERANDS },
    { "cmpx",   0xcc000000, TWO_OPERANDS },

    { "cogid",  0x0c400001, DST_OPERAND_ONLY },
    { "coginit", 0x0c400002, DST_OPERAND_ONLY },
    { "cogstop", 0x0c400003, DST_OPERAND_ONLY },

    { "djnz",   0xe4800000, TWO_OPERANDS },
    { "hubop",  0x0c000000, TWO_OPERANDS },
    { "jmp",    0x5c000000, SRC_OPERAND_ONLY },
    { "jmpret", 0x5c800000, TWO_OPERANDS },

    { "lockclr",0x0c400007, DST_OPERAND_ONLY },
    { "locknew",0x0c400004, DST_OPERAND_ONLY },
    { "lockret",0x0c400005, DST_OPERAND_ONLY },
    { "lockset",0x0c400006, DST_OPERAND_ONLY },

    { "max",    0x4c800000, TWO_OPERANDS },
    { "maxs",   0x44800000, TWO_OPERANDS },
    { "min",    0x48800000, TWO_OPERANDS },
    { "mins",   0x40800000, TWO_OPERANDS },
    { "mov",    0xa0800000, TWO_OPERANDS },
    { "movd",   0x54800000, TWO_OPERANDS },
    { "movi",   0x58800000, TWO_OPERANDS },
    { "movs",   0x50800000, TWO_OPERANDS },

    { "muxc",   0x70800000, TWO_OPERANDS },
    { "muxnc",  0x74800000, TWO_OPERANDS },
    { "muxz",   0x78800000, TWO_OPERANDS },
    { "muxnz",  0x7c800000, TWO_OPERANDS },

    { "neg",    0xa4800000, TWO_OPERANDS },
    { "negc",   0xb0800000, TWO_OPERANDS },
    { "negnc",  0xb4800000, TWO_OPERANDS },
    { "negnz",  0xbc800000, TWO_OPERANDS },
    { "negz",   0xb8800000, TWO_OPERANDS },
    { "nop",    0x00000000, NO_OPERANDS },

    { "or",     0x68800000, TWO_OPERANDS },

    { "rcl",    0x34800000, TWO_OPERANDS },
    { "rcr",    0x30800000, TWO_OPERANDS },
    { "rdbyte", 0x00800000, TWO_OPERANDS },
    { "rdlong", 0x08800000, TWO_OPERANDS },
    { "rdword", 0x04800000, TWO_OPERANDS },
    { "ret",    0x5c400000, NO_OPERANDS },
    { "rev",    0x3c800000, TWO_OPERANDS },
    { "rol",    0x24800000, TWO_OPERANDS },
    { "ror",    0x20800000, TWO_OPERANDS },

    { "sar",    0x38800000, TWO_OPERANDS },
    { "shl",    0x2c800000, TWO_OPERANDS },
    { "shr",    0x28800000, TWO_OPERANDS },
    { "sub",    0x84800000, TWO_OPERANDS },
    { "subs",   0xc0800000, TWO_OPERANDS },
    { "subsx",  0xc4800000, TWO_OPERANDS },
    { "subx",   0xcc800000, TWO_OPERANDS },
    { "sumc",   0x90800000, TWO_OPERANDS },
    { "sumnc",  0x94800000, TWO_OPERANDS },
    { "sumnz",  0x9c800000, TWO_OPERANDS },
    { "sumz",   0x98800000, TWO_OPERANDS },

    { "test",   0x60000000, TWO_OPERANDS },
    { "testn",  0x64000000, TWO_OPERANDS },
    { "tjnz",   0xe8000000, TWO_OPERANDS },
    { "tjz",    0xec000000, TWO_OPERANDS },

    { "waitcnt", 0xf8800000, TWO_OPERANDS },
    { "waitpeq", 0xf0800000, TWO_OPERANDS },
    { "waitpne", 0xf4800000, TWO_OPERANDS },
    { "waitvid", 0xfc800000, TWO_OPERANDS },

    { "wrbyte", 0x00000000, TWO_OPERANDS },
    { "wrlong", 0x08000000, TWO_OPERANDS },
    { "wrword", 0x04000000, TWO_OPERANDS },

    { "xor",    0x6c800000, TWO_OPERANDS },
};

HwReg hwreg[] = {
    { "par", 0x1f0, "_PAR" },
    { "cnt", 0x1f1, "_CNT" },
    { "ina", 0x1f2, "_INA" },
    { "inb", 0x1f3, "_INB" },
    { "outa", 0x1f4, "_OUTA" },
    { "outb", 0x1f5, "_OUTB" },
    { "dira", 0x1f6, "_DIRA" },
    { "dirb", 0x1f7, "_DIRB" },
    { "ctra", 0x1f8, "_CTRA" },
    { "ctrb", 0x1f9, "_CTRB" },

    { "frqa", 0x1fa, "_FRQA" },
    { "frqb", 0x1fb, "_FRQB" },
    { "phsa", 0x1fc, "_PHSA" },
    { "phsb", 0x1fd, "_PHSB" },

    { "vcfg", 0x1fe, "_VCFG" },
    { "vscl", 0x1ff, "_VSCL" },

};

#define IF_NEVER 0xffc3ffff

InstrModifier modifiers[] = {
    { "IF_ALWAYS", IF_NEVER | (0xf<<18) },
    { "IF_NEVER",  IF_NEVER },

    { "IF_A",           IF_NEVER | (0x1<<18) },
    { "IF_NC_AND_NZ",   IF_NEVER | (0x1<<18) },
    { "IF_NZ_AND_NC",   IF_NEVER | (0x1<<18) },

    { "IF_NC_AND_Z",    IF_NEVER | (0x2<<18) },
    { "IF_Z_AND_NC",    IF_NEVER | (0x2<<18) },

    { "IF_AE",     IF_NEVER | (0x3<<18) },
    { "IF_NC",     IF_NEVER | (0x3<<18) },

    { "IF_C_AND_NZ",    IF_NEVER | (0x4<<18) },
    { "IF_NZ_AND_C",    IF_NEVER | (0x4<<18) },

    { "IF_NE",     IF_NEVER | (0x5<<18) },
    { "IF_NZ",     IF_NEVER | (0x5<<18) },

    { "IF_C_NE_Z", IF_NEVER | (0x6<<18) },
    { "IF_Z_NE_C", IF_NEVER | (0x6<<18) },

    { "IF_NC_OR_NZ", IF_NEVER | (0x7<<18) },
    { "IF_NZ_OR_NC", IF_NEVER | (0x7<<18) },

    { "IF_C_AND_Z", IF_NEVER | (0x8<<18) },
    { "IF_Z_AND_C", IF_NEVER | (0x8<<18) },

    { "IF_C_EQ_Z", IF_NEVER | (0x9<<18) },
    { "IF_Z_EQ_C", IF_NEVER | (0x9<<18) },

    { "IF_E",      IF_NEVER | (0xa<<18) },
    { "IF_Z",      IF_NEVER | (0xa<<18) },

    { "IF_NC_OR_Z", IF_NEVER | (0xb<<18) },
    { "IF_Z_OR_NC", IF_NEVER | (0xb<<18) },

    { "IF_B",      IF_NEVER | (0xc<<18) },
    { "IF_C",      IF_NEVER | (0xc<<18) },

    { "IF_C_OR_NZ", IF_NEVER | (0xd<<18) },
    { "IF_NZ_OR_C", IF_NEVER | (0xd<<18) },

    { "IF_BE",     IF_NEVER | (0xe<<18) },
    { "IF_C_OR_Z", IF_NEVER | (0xe<<18) },
    { "IF_Z_OR_C", IF_NEVER | (0xe<<18) },


    { "WZ", (1<<25) },
    { "WC", (1<<24) },
    { "WR", (1<<23) },
    { "NR", ~(1<<23) }

};

void
InitPasm(void)
{
    int i;

    /* add hardware registers */
    for (i = 0; i < N_ELEMENTS(hwreg); i++) {
        AddSymbol(&reservedWords, hwreg[i].name, SYM_HWREG, (void *)&hwreg[i]);
    }

    /* add instructions */
    for (i = 0; i < N_ELEMENTS(instr); i++) {
        AddSymbol(&pasmWords, instr[i].name, SYM_INSTR, (void *)&instr[i]);
    }

    /* instruction modifiers */
    for (i = 0; i < N_ELEMENTS(modifiers); i++) {
        AddSymbol(&pasmWords, modifiers[i].name, SYM_INSTRMODIFIER, (void *)(intptr_t)modifiers[i].modifier);
    }
}

