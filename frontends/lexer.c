//
// Simple lexical analyzer for a language where indentation
// may be significant (Spin); also contains lexers for BASIC and C
//
// Copyright (c) 2011-2020 Total Spectrum Software Inc.
//
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "spinc.h"
#include "lexer.h"
#include "preprocess.h"

int allow_type_names = 1;

#ifndef EOF
#define EOF (-1)
#endif

// have to work around bison problems with -p
// instead of #defines bison outputs an enum yytokentype,
// which is a problem if we use -p to create multiple parsers;
// the bison 3.x solution is %api.prefix, but we want to work with
// older bisons and byacc too
#undef YYTOKENTYPE
#define yytokentype spinyytokentype
#include "spin.tab.h"
#undef YYTOKENTYPE
#undef yytokentype
#define yytokentype basicyytokentype
#include "basic.tab.h"
#undef YYTOKENTYPE
#undef yytokentype
#define yytokentype cgramyytokentype
#include "cgram.tab.h"

// flag for case insensitivity
int gl_caseSensitive;

// preprocessor struct
struct preprocess gl_pp;

// used for error messages
AST *last_ast;

// accumulated comments
static AST *comment_chain;

/* flag: if set, run the  preprocessor */
int gl_preprocess = 1;

/* flag: if set, add original source as comments */
int gl_srccomments = 0;

/* some ctype style functions that work on non-ASCII tokens */
static inline int
safe_isalpha(unsigned int x) {
    return (x < 255) ? isalpha(x) : 0;
}
static inline int
safe_isdigit(unsigned int x) {
    return (x < 255) ? isdigit(x) : 0;
}

SymbolTable spinCommonReservedWords;
SymbolTable spin1ReservedWords;
SymbolTable spin2ReservedWords;
SymbolTable basicReservedWords;
SymbolTable basicAsmReservedWords;
SymbolTable cReservedWords;
SymbolTable cppReservedWords;
SymbolTable cAsmReservedWords;
SymbolTable pasmWords;
SymbolTable ckeywords;

static void InitPasm(int flags);

/* functions for handling string streams */
static int 
strgetc(LexStream *L)
{
    char *s;
    int c;

    s = (char *)L->ptr;
    c = (*s++) & 0x00ff;
    if (c == '\r') {
        c = (*s++) & 0x00ff;
        if (c != '\n') {
            --s;
            c = '\n';
        }
    }
    if (c != 0)
        L->ptr = s;
    return (c == 0) ? EOF : c;
}

/* open a stream from a string s */
void strToLex(LexStream *L, const char *s, const char *name, int language)
{
    if (!L) {
        current->Lptr = L = (LexStream *)malloc(sizeof(*L));
    }
    memset(L, 0, sizeof(*L));
    L->arg = L->ptr = (void *)s;
    L->getcf = strgetc;
    L->pendingLine = 1;
    L->fileName = name ? name : "<string>";
    L->language = language;
    flexbuf_init(&L->curLine, 1024);
    flexbuf_init(&L->lineInfo, 1024);
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
    /* translate CR+LF ->LF, plain CR ->LF */
    if (c == '\r') {
        c = fgetc(f);
        if (c != '\n') {
            ungetc(c, f);
            c = '\n';
        }
    }

    return (c >= 0) ? c : EOF;
}

static int 
filegetwc(LexStream *L)
{
    FILE *f;
    int c1, c2;
    f = (FILE *)L->ptr;
again:
    c1 = fgetc(f);
    if (c1 < 0) return EOF;
    c2 = fgetc(f);
    if (c2 != 0 || c1 >= 0x80) {
        wchar_t w;
        size_t n;
        char buf[8];
        if (c2 < 0) return EOF;
        /* convert to UTF-8 */
        w = (c2<<8) | c1;
        n = to_utf8(buf, w);
        while (n > 1) {
            --n;
            lexungetc(L, buf[n] & 0xff);
        }
        return buf[0] & 0xff;
    }
    /* eliminate carriage returns */
    /* actually there's a problem: if we have a MAC file,
       we may not have line feeds, so we need to
       convert carriage returns to line feeds; but for
       Windows we need to ignore a line feed right after a
       carriage return!
    */
    if (c1 == '\r') {
        c1 = '\n';
        L->sawCr = 1;
    } else {
        if (c1 == '\n') {
            if (L->sawCr) {
                L->sawCr = 0;
                goto again;
            }
        }
        L->sawCr = 0;
    }
    return c1;
}

/* open a stream from a FILE f */
void fileToLex(LexStream *L, FILE *f, const char *name, int language)
{
    int c1, c2;

    if (!L) {
        current->Lptr = L = (LexStream *)malloc(sizeof(*L));
    }
    memset(L, 0, sizeof(*L));
    L->ptr = (void *)f;
    L->arg = NULL;
    L->getcf = filegetc;
    L->pendingLine = 1;
    L->language = language;
    flexbuf_init(&L->curLine, 1024);
    flexbuf_init(&L->lineInfo, 1024);
    /* check for Unicode */
    c1 = fgetc(f);
    c2 = fgetc(f);
    if (c1 == 0xff && c2 == 0xfe) {
        L->getcf = filegetwc;
    } else {
        rewind(f);
    }
    L->fileName = name;

}

/*
 * utility function: start a new line
 */
static void startNewLine(LexStream *L)
{
    LineInfo lineInfo;

    flexbuf_addchar(&L->curLine, 0); // terminate the line
    lineInfo.linedata = flexbuf_get(&L->curLine);
    lineInfo.fileName = L->fileName;
    lineInfo.lineno = L->lineCounter;
    flexbuf_addmem(&L->lineInfo, (char *)&lineInfo, sizeof(lineInfo));
}

#define TAB_STOP 8
/*
 *
 */
int
lexgetc(LexStream *L)
{
    int c;
    if (L->ungot_ptr) {
        --L->ungot_ptr;
        return L->ungot[L->ungot_ptr];
    }
    if (L->pendingLine) {
        startNewLine(L);
        L->lineCounter++;
        L->pendingLine = 0;
        L->colCounter = 0;
    }
    c = (L->getcf(L));
    if (c == '\n') {
        L->pendingLine = 1;
    } else if (c == '\t') {
        L->colCounter += TAB_STOP;
        /* now go back to the previous stop */
        L->colCounter = TAB_STOP * (L->colCounter/TAB_STOP);
    } else {
        L->colCounter++;
    }
    if (c == EOF) {
        startNewLine(L);
    } else {
        flexbuf_addchar(&L->curLine, c);
    }
    return c;
}

void
lexungetc(LexStream *L, int c)
{
    if (L->ungot_ptr == UNGET_MAX-1) {
        SYNTAX_ERROR("internal error: unget limit exceeded\n");
    }
    L->ungot[L->ungot_ptr++] = c;
}

int
lexpeekc(LexStream *L)
{
    int c = lexgetc(L);
    lexungetc(L, c);
    return c;
}

AST *IntegerLabel(AST *num)
{
    int x = EvalConstExpr(num);
    char *name = (char *)calloc(1, 32);
    sprintf(name, "LINE_%d", x);
    return AstIdentifier(name);
}

//
// establish an indent level
// if the line is indented more than this, an SP_INDENT will
// be emitted
//
void
EstablishIndent(LexStream *L, int level)
{
    if (L->indentsp >= MAX_INDENTS-1) {
        SYNTAX_ERROR("too many nested indentation levels");
        return;
    }
    if (level < 0) {
        level = L->firstNonBlank;
    }
    L->indentsp++;
    L->indent[L->indentsp] = level;
    L->pending_indent++;
}

void
resetLineState(LexStream *L)
{
    lexungetc(L, '\n');
}

/*
 * utility functions
 */
int
isIdentifierStart(int c)
{
    if (safe_isalpha(c))
        return 1;
    if (c == '_')
        return 1;
    return 0;
}

int
isIdentifierChar(int c)
{
    return isIdentifierStart(c) || safe_isdigit(c);
}

/* dynamically grow a string */
#define INCSTR 16

/*
 * actual parsing functions
 */
static int
parseNumber(LexStream *L, unsigned int base, uint32_t *num)
{
    unsigned long uval, digit;
    unsigned long tenval;
    unsigned int c;
    int sawdigit = 0;
    int kind = SP_NUM;
    int lang;

    lang = L->language;
    uval = 0;
    tenval = 0;
    
    for(;;) {
        c = lexgetc(L);
        if (c == '_')
            continue;
        else if (c >= 'A' && c <= 'Z')
            digit = 10 + c - 'A';
        else if (c >= 'a' && c <= 'z')
            digit = 10 + c - 'a';
        else if (c >= '0' && c <= '9') {
            digit = (c - '0');
        } else {
            break;
        }
        if (base <= 10 && digit < 10) {
            // keep parallel track of the interpretation in base 10
            tenval = 10 * tenval + digit;
        }
        if (digit < base) {
            uval = base * uval + digit;
            sawdigit = 1;
        } else if (base == 8 && digit < 10) {
            uval = tenval;
            base = 10;
        } else {
            break;
        }
    }
    if ( (base <= 10) && (c == '.' || c == 'e' || c == 'E') ) {
        /* potential floating point number */
        float f = (float)tenval;
        float ff = 0.0;
        static float divby[45] = {
            1e-1f, 1e-2f, 1e-3f, 1e-4f, 1e-5f,
            1e-6f, 1e-7f, 1e-8f, 1e-9f, 1e-10f,
            1e-11f, 1e-12f, 1e-13f, 1e-14f, 1e-15f,
            1e-16f, 1e-17f, 1e-18f, 1e-19f, 1e-20f,
            1e-21f, 1e-22f, 1e-23f, 1e-24f, 1e-25f,
            1e-26f, 1e-27f, 1e-28f, 1e-29f, 1e-30f,
            1e-31f, 1e-32f, 1e-33f, 1e-34f, 1e-35f,
            1e-36f, 1e-37f, 1e-38f, 1e-39f, 1e-40f,
            1e-41f, 1e-42f, 1e-43f, 1e-44f, 1e-45f,
        };
        int counter = 0;
        int exponent = 0;

        if (c == '.') {
            c = lexgetc(L);
            if ( IsSpinLang(lang) && c != 'e' && c != 'E' && (c < '0' || c > '9')) {
                lexungetc(L, c);
                c = '.';
                goto donefloat;
            }
        }
        while ( (c >= '0' && c <= '9') || (c == '_')) {
            if (c != '_') {
                ff = ff + divby[counter]*(float)(c-'0');
                counter++;
            }
            sawdigit = 1;
            c = lexgetc(L);
        }
        if (c == 'e' || c == 'E') {
            int expval = 0;
            int neg = 1;
            c = lexgetc(L);
            if (c == '+') {
                c = lexgetc(L);
            } else if (c == '-') {
                c = lexgetc(L);
                neg = -neg;
            }
            while (c >= '0' && c <= '9') {
                expval = 10*expval + (c - '0');
                c = lexgetc(L);
            }
            if (neg < 0)
                expval = -expval;
            exponent += expval;
        }
        f = f + ff;
        if (exponent < 0 && exponent >= -45) {
            f *= divby[-(exponent+1)];
        } else if (exponent != 0) {
            f *= powf(10.0f, (float)exponent);
        }
        if (gl_fixedreal) {
            uval = ((1<<G_FIXPOINT) * f) + 0.5;
        } else {
            uval = floatAsInt(f);
        }
        kind = SP_FLOATNUM;
    }
donefloat:
    lexungetc(L, c);
    *num = uval;
    return sawdigit ? kind : ((base == 16) ? SP_HERE : '%');
}

/* check for DAT or ASM blocks */
static int InDatBlock(LexStream *L)
{
    return L->block_type >= BLOCK_DAT && L->block_type <= BLOCK_PASM;
}

/* check for PASM (Spin compatible assembly) block in C or BASIC */
static int InPasmBlock(LexStream *L)
{
    return L->block_type == BLOCK_PASM;
}

/* convert a Spin block type into a standard block type */
static int MapSpinBlock(int c)
{
    switch(c) {
    case SP_CON:
        return BLOCK_CON;
    case SP_VAR:
        return BLOCK_VAR;
    case SP_OBJ:
        return BLOCK_OBJ;
    case SP_DAT:
        return BLOCK_DAT;
    case SP_ASM:
    case SP_ORG:
        return BLOCK_ASM;
    case SP_PUB:
        return BLOCK_PUB;
    case SP_PRI:
        return BLOCK_PRI;
    default:
        return BLOCK_UNKNOWN;
    }
}

/* parse an identifier */
static int
parseSpinIdentifier(LexStream *L, AST **ast_ptr, const char *prefix)
{
    int c;
    struct flexbuf fb;
    Symbol *sym;
    AST *ast = NULL;
    int startColumn = L->colCounter - 1;
    char *idstr;
    int gatherComments = 1;
    bool forceLower = 0; // !gl_caseSensitive;

    flexbuf_init(&fb, INCSTR);
    if (prefix) {
        flexbuf_addmem(&fb, prefix, strlen(prefix));
        if (gl_gas_dat) {
            flexbuf_addchar(&fb, '.');
        } else {
            flexbuf_addchar(&fb, ':');
        }
    }
    c = lexgetc(L);
    while (isIdentifierChar(c) || c == '`') {
        if (c == '`') {
            c = lexgetc(L);
        } else if (forceLower) {
            c = tolower(c);
        }
        flexbuf_addchar(&fb, c);
        c = lexgetc(L);
    }
    // add a trailing 0, and make sure there is room for an extra
    // character in case the name mangling needs it
    flexbuf_addchar(&fb, '\0');
    flexbuf_addchar(&fb, '\0');
    idstr = flexbuf_get(&fb);
    lexungetc(L, c);

    /* check for reserved words */
    if (InDatBlock(L)) {
        sym = FindSymbol(&pasmWords, idstr);
        if (sym) {
            free(idstr);
            if (sym->kind == SYM_INSTR) {
                ast = NewAST(AST_INSTR, NULL, NULL);
                ast->d.ptr = sym->val;
                if (comment_chain) {
                    ast = AddToList(comment_chain, ast);
                    comment_chain = NULL;
                }
                *ast_ptr = ast;
                return SP_INSTR;
            }
            if (sym->kind == SYM_INSTRMODIFIER) {
                ast = NewAST(AST_INSTRMODIFIER, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return SP_INSTRMODIFIER;
            }
            if (sym->kind == SYM_HWREG) {
                ast = NewAST(AST_HWREG, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return SP_HWREG;
            }
            fprintf(stderr, "Internal error: Unknown pasm symbol type %d\n", sym->kind);
        }
    }
    if (L->language == LANG_SPIN_SPIN2) {
        sym = FindSymbol(&spin2ReservedWords, idstr);
    } else {
        sym = FindSymbol(&spin1ReservedWords, idstr);
    }
    if (sym == NULL) {
        sym = FindSymbol(&spinCommonReservedWords, idstr);
    }
    if (sym != NULL) {
        if (sym->kind == SYM_BUILTIN)
        {
            /* run any parse hooks */
            Builtin *b = (Builtin *)sym->val;
            if (b && b->parsehook) {
                (*b->parsehook)(b);
            }
            goto is_identifier;
        }
        if (sym->kind == SYM_CONSTANT
            || sym->kind == SYM_FLOAT_CONSTANT)
        {
            goto is_identifier;
        }
        if (sym->kind == SYM_RESERVED) {
            c = INTVAL(sym);
            /* check for special handling */
            switch(c) {
            case SP_PUB:
            case SP_PRI:
            case SP_DAT:
            case SP_OBJ:
            case SP_VAR:
            case SP_CON:
                L->block_type = MapSpinBlock(c);
                L->block_firstline = L->lineCounter;
                //EstablishIndent(L, 1);
                break;
	    case SP_ASM:
            case SP_ORG:
	        if (L->block_type == BLOCK_ASM && c == SP_ASM) {
		    fprintf(stderr, "WARNING: ignoring nested asm\n");
                } else if (InDatBlock(L)) {
                    /* for ORG, nothing special to do */
                    /* for ASM, check for identifier */
                    if (c == SP_ASM && L->colCounter - L->firstNonBlank > 4) {
                        goto is_identifier;
                    }
                } else {
		    L->save_block = L->block_type;
                    L->block_type = BLOCK_ASM;
		}
		break;
            case SP_END:
                if (L->block_type != BLOCK_ASM || L->colCounter - L->firstNonBlank > 4) {
                    goto is_identifier;
                }
                /* fall through */
	    case SP_ENDASM:
	        L->block_type = L->save_block;
	        break;
            case SP_IF:
            case SP_IFNOT:
            case SP_ELSE:
            case SP_ELSEIF:
            case SP_ELSEIFNOT:
            case SP_REPEAT:
            case SP_CASE:
            case SP_CASE_FAST:
                EstablishIndent(L, startColumn);
                break;
            case SP_LONG:
            case SP_BYTE:
            case SP_WORD:
                if (InDatBlock(L)) {
                    gatherComments = 1;
                } else {
                    gatherComments = 0;
                }
                break;
            default:
                gatherComments = 0;
                break;
            }
            if (!ast) {
                if (gatherComments) {
                    ast = GetComments();
                }
            }
            free(idstr);
            *ast_ptr = ast;
            return c;
        }
        if (sym->kind == SYM_HWREG) {
            ast = NewAST(AST_HWREG, NULL, NULL);
            ast->d.ptr = sym->val;
            free(idstr);
            *ast_ptr = ast;
            return SP_HWREG;
        }
        fprintf(stderr, "Internal error: Unknown symbol type %d\n", sym->kind);
    }

is_identifier:
    ast = NewAST(AST_IDENTIFIER, NULL, NULL);

    if (gl_normalizeIdents) {
        NormalizeIdentifier(idstr);
    }

    /* peek ahead to handle foo.bar as foo#bar */
    if (gl_p2 && InDatBlock(L)) {
        c = lexgetc(L);
        if (c == '.') {
            lexungetc(L, '#');
        } else {
            lexungetc(L, c);
        }
    }
    ast->d.string = idstr;
    *ast_ptr = ast;
    return SP_IDENTIFIER;
}

/* parse the rest of the line as a string */
static void
parseLineAsString(LexStream *L, AST **ast_ptr)
{
    int c;
    struct flexbuf fb;
    AST *ast;
    
    ast = NewAST(AST_STRING, NULL, NULL);
    flexbuf_init(&fb, INCSTR);

    // skip leading spaces
    do {
        c = lexgetc(L);
    } while (c == ' ');
    
    while (c > 0 && c < 256) {
        if (c == 10 || c == 13) {
            // newline in mid-string, this is what we want
            lexungetc(L, c);
            break;
        }
        flexbuf_addchar(&fb, c);
        c = lexgetc(L);
    }
    flexbuf_addchar(&fb, 10);
    flexbuf_addchar(&fb, '\0');

    ast->d.string = flexbuf_get(&fb);
    *ast_ptr = ast;
}

/* parse a string */
static void
parseString(LexStream *L, AST **ast_ptr)
{
    int c;
    struct flexbuf fb;
    AST *ast;

    ast = NewAST(AST_STRING, NULL, NULL);
    flexbuf_init(&fb, INCSTR);
    c = lexgetc(L);
    while (c != '"' && c > 0 && c < 256) {
        if (c == 10 || c == 13) {
            // newline in mid-string, this is bad news
            SYNTAX_ERROR("unterminated string");
            lexungetc(L, c);
            break;
        }
        flexbuf_addchar(&fb, c);
        c = lexgetc(L);
    }
    flexbuf_addchar(&fb, '\0');

    ast->d.string = flexbuf_get(&fb);
    *ast_ptr = ast;
}

/* get a character after \ */
static int
getEscapedChar(LexStream *L)
{
    int c = lexgetc(L);
    int g;
    if (c < 0) {
        SYNTAX_ERROR("end of file inside string");
        return c;
    }
    switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
        g = c;
        c = 0;
        while (g >= '0' && g <= '7') {
            c = c*8 + (g-'0');
            g = lexgetc(L);
        }
        lexungetc(L, g);
        break;
    case 'a':
        c = 7; break;
    case 'b':
        c = 8; break;
    case 't':
        c = 9; break;
    case 'n':
        c = 10; break;
    case 'v':
        c = 11; break;
    case 'f':
        c = 12; break;
    case 'r':
        c = 13; break;
    case 'e':
        c = 27; break;
    case 'x':
    case 'X':
        c = 0;
        for(;;) {
            g = lexgetc(L);
            if (g >= '0' && g <= '9') {
                c = 16*c + (g-'0');
            } else if (g >= 'a' && g <= 'f') {
                c = 16*c + (g-'a') + 10;
            } else if (g >= 'A' && g <= 'F') {
                c = 16*c + (g-'A') + 10;
            } else {
                lexungetc(L, g);
                break;
            }
        }
    }
    return c;
}

/* forward declaration */
static int skipSpace(LexStream *L, AST **ast_ptr, int language);

/* parse a C string */
static void
parseCString(LexStream *L, AST **ast_ptr)
{
    int c;
    struct flexbuf fb;
    AST *exprlist;
    AST *ast;
    char *str;
    
    exprlist = NULL;
    flexbuf_init(&fb, INCSTR);
again:
    c = lexgetc(L);
    while (c != '"' && c >= 0 && c < 256) {
        if (c == 10 || c == 13) {
            // newline in mid-string, this is bad news
            SYNTAX_ERROR("unterminated string");
            lexungetc(L, c);
            break;
        }
        if (c == '\\') {
            c = getEscapedChar(L);
            if (c < 0) break;
            if (c == 0) {
                flexbuf_addchar(&fb, 0);
                str = flexbuf_get(&fb);
                if (*str) {
                    ast = NewAST(AST_STRING, NULL, NULL);
                    ast->d.string = str;
                    exprlist = AddToList(exprlist, NewAST(AST_EXPRLIST, ast, NULL));
                }
                exprlist = AddToList(exprlist, NewAST(AST_EXPRLIST, AstInteger(0), NULL));
            } else {
                flexbuf_addchar(&fb, c);
            }
        } else {
            flexbuf_addchar(&fb, c);
        }
        c = lexgetc(L);
    }
    // handle "x" "y" as "xy"
    c = skipSpace(L, NULL, LANG_CFAMILY_C);
    if (c == '"') {
        goto again;
    } else {
        lexungetc(L, c);
    }
    flexbuf_addchar(&fb, 0);
    str = flexbuf_get(&fb);
    if (str && *str) {
        ast = NewAST(AST_STRING, NULL, NULL);
        ast->d.string = str;
        exprlist = AddToList(exprlist, NewAST(AST_EXPRLIST, ast, NULL));
    }
    if (!exprlist) {
        exprlist = AddToList(exprlist, NewAST(AST_EXPRLIST, AstInteger(0), NULL));
    }
    *ast_ptr = exprlist;
}

//
// keep track of accumulated comments
//

AST *
GetComments(void)
{
    AST *ret = comment_chain;
    comment_chain = NULL;
    if (!ret) {
        ret = NewAST(AST_COMMENT, NULL, NULL); // just for holding error lines
        ret->d.string = NULL;
    }
    return ret;
}

/* try to output only one AST_SRCCOMMENT per input line, so check here for
 * duplication
 */
static void CheckSrcComment( LexStream *L )
{
    static LexStream *s_lastStream;
    static int s_lastLine = -1;
    static const char *s_lastFileName;
    
    if (s_lastStream == L && s_lastLine == L->lineCounter
        && s_lastFileName == L->fileName)
    {
        return;
    }
    if ( (L->flags & LEXSTREAM_FLAG_NOSRC) || (current == globalModule)) {
        return;
    }
    comment_chain = AddToList(comment_chain, NewAST(AST_SRCCOMMENT, NULL, NULL));
    s_lastStream = L;
    s_lastLine = L->lineCounter;
    s_lastFileName = L->fileName;
}

// duplicate a file name, possibly quoted
static char *
getFileName(const char *orig)
{
    char *ptr = strdup(orig);
    if (*ptr == '"') {
        size_t siz = strlen(ptr+1);
        if (siz > 1) {
            memmove(ptr, ptr+1, siz-1);
            ptr[siz-2] = 0;
        }
    }
    return ptr;
}

static void
handlePragma(LexStream *L, const char *line)
{
    line += 8; // skip "#pragma "
    while (*line == ' ' || *line=='\t')
        line++;
    if (!strncmp(line, "flexc", 5)) {
        SYNTAX_ERROR("unknown pragma: %s", line);
    }
}

// check for a comment that runs to end of line
static int
checkCommentedLine(struct flexbuf *cbp, LexStream *L, int c, int language)
{
    AST *ast;
    char *commentLine;
    int cameFromHash = 0;
    
    // look for #line directives
    if (c == '#' && L->colCounter == 1) {
        // UGH! it's actually legal in Spin to put #N at the beginning of
        // the line in a CON block
        if (IsSpinLang(language)) {
            int c2 = lexpeekc(L);
            if ( !(c2 == 'l' || (c2 == 'p') ) ) {
                goto not_comment;
            }
            cameFromHash = 1;
        }
        goto docomment;
    }
not_comment:        
    if ( IsBasicLang(language) && (c == 'r' || c == 'R') ) {
        int c2, c3, c4;
        c2 = lexgetc(L);
        if (c2 == 'e' || c2 == 'E') {
            c3 = lexgetc(L);
            if (c3 == 'm' || c3 == 'M') {
                c4 = lexgetc(L);
                if (!safe_isalpha(c4)) {
                    c = c4;
                    goto docomment;
                }
                lexungetc(L, c4);
            }
            lexungetc(L, c3);
        }
        lexungetc(L, c2);
        return c;
    }

    if (IsCLang(language) && !InPasmBlock(L)) {
        if (c == '/') {
            int c2 = lexgetc(L);
            if (c2 == '/') {
                c = lexgetc(L);
                goto docomment;
            }
            lexungetc(L, c2);
        }
    } else if (c == '\'') {
        while (c == '\'') c = lexgetc(L);
        goto docomment;
    }
    return c;
docomment:
    while (c != '\n' && c != EOF) {
        flexbuf_addchar(cbp, c);
        c = lexgetc(L);
    }
    flexbuf_addchar(cbp, c);
    flexbuf_addchar(cbp, 0);
    commentLine = flexbuf_get(cbp);
    //printf("comment: %s", commentLine);
    if (!strncmp(commentLine, "#line ", 6)) {
        char *ptr = commentLine + 6;
        int lineno;
        lineno = strtol(ptr, &ptr, 10);
        if (lineno > 0) {
            while (*ptr == ' ') ptr++;
            L->lineCounter = lineno - 1;
            L->fileName = getFileName(ptr);
        }
    } else if (!strncmp(commentLine, "#pragma ", 8)) {
        handlePragma(L, commentLine);
    } else if (IsSpinLang(language) && commentLine[0] == '#' && cameFromHash) {
        SYNTAX_ERROR("# at start of line not handled properly");
    } else {
        ast = NewAST(AST_COMMENT, NULL, NULL);
        ast->d.string = commentLine;
        comment_chain = AddToList(comment_chain, ast);
    }
    return c;
}

//
// check for start of multi-line comment
//
static bool
commentBlockStart(int language, int c, LexStream *L)
{
    if (IsSpinLang(language)) {
        return c == '{';
    }
    if (IsBasicLang(language)) {
        if (c != '/') {
            return false;
        }
        c = lexgetc(L);
        if (c != '\'') {
            lexungetc(L, c);
            return false;
        }
        return true;
    }
    if (IsCLang(language)) {
        if (c != '/') {
            return false;
        }
        c = lexgetc(L);
        if (c != '*') {
            lexungetc(L, c);
            return false;
        }
        return true;
    }
    return false;
}

static bool
commentBlockEnd(int language, int c, LexStream *L)
{
    if (IsSpinLang(language)) {
        return c == '}';
    }
    if (IsBasicLang(language)) {
        if (c != '\'') {
            return false;
        }
        c = lexgetc(L);
        if (c != '/') {
            lexungetc(L, c);
            return false;
        }
        return true;
    }
    if (IsCLang(language)) {
        if (c != '*') {
            return false;
        }
        c = lexgetc(L);
        if (c != '/') {
            lexungetc(L, c);
            return false;
        }
        return true;
    }
    return false;
}

//
// skip over comments and spaces
// return first non-comment non-space character
// if we are inside a function, emit SP_INDENT when
// we increase the indent level, SP_OUTDENT when we
// decrease it
//

static int
skipSpace(LexStream *L, AST **ast_ptr, int language)
{
    int c;
    int commentNest;
    int start_indent;
    struct flexbuf cb;
    AST *ast;
    int startcol = 0;
    int startline = 0;
    int eoln_token;
    int eof_token;

    if (IsBasicLang(language)) {
      eoln_token = BAS_EOLN;
      eof_token = BAS_EOF;
    } else if (IsCLang(language)) {
        if (InDatBlock(L)) {
            eoln_token = C_EOLN;
        } else {
            eoln_token = ' ';
        }
        eof_token = -1;
    } else {
      eoln_token = SP_EOLN;
      eof_token = SP_EOF;
    }
    
    flexbuf_init(&cb, INCSTR);
refetch:
    c = lexgetc(L);
again:
    if (gl_srccomments && L->eoln) {
        CheckSrcComment(L);
    }
    while (c == ' ' || c == '\t') {
        c = lexgetc(L);
    }

    /* ignore completely empty lines or ones with just comments */
    c = checkCommentedLine(&cb, L, c, language);
    if (commentBlockStart(language, c, L)) {
        struct flexbuf anno;
        int annotate = 0;
        int directive = 0;
	int doccomment = 0;
        int doccommentchar;
        int allowNestedComments;
        
        startcol = L->colCounter;
        startline = L->lineCounter;
        flexbuf_init(&anno, INCSTR);
        commentNest = 1;
        if ( IsSpinLang(language) || L->block_type == BLOCK_PASM) {
            doccommentchar = '{';
            allowNestedComments = 1;
        } else {
            doccommentchar = '*';
            allowNestedComments = 0;
        }
        /* check for special comments {++... } which indicate 
           inline C code
           We also set up the preprocessor to emit {#line xx} directives when
           doing #include
        */
        c = lexgetc(L);
        if (c == '+') {
            c = lexgetc(L);
            if (c == '+') {
                annotate = 1;
                c = lexgetc(L);
            }
        } else if (c == '#') {
            c = lexgetc(L);
            directive = 1;
        } else if (c == doccommentchar) {
	    c = lexgetc(L);
	    doccomment = 1;
            allowNestedComments = 0;
	}
        lexungetc(L, c);
        for(;;) {
            c = lexgetc(L);
            if (allowNestedComments && commentBlockStart(language, c, L)) {
                commentNest++;
            } else if (commentBlockEnd(language, c, L)) {
	        if (doccomment && (IsSpinLang(language) || L->block_type == BLOCK_PASM)) {
	            int peekc;
		    peekc = lexgetc(L);
		    if (peekc == '}') {
		        commentNest = 0;
		    } else {
		        lexungetc(L, peekc);
		    }
		} else {
		  --commentNest;
		}
            }
            if (commentNest <= 0 || c == EOF) {
                break;
            }
            if (annotate || directive) {
                flexbuf_addchar(&anno, c);
            } else {
                flexbuf_addchar(&cb, c);
            }
        }
        if (c == EOF) {
	    if (commentNest > 0)
	        fprintf(stderr, "WARNING: EOF seen inside comment\n");
            return eof_token;
	}
        if (annotate) {
            AST *ast = NewAST(AST_ANNOTATION, NULL, NULL);
            flexbuf_addchar(&anno, '\0');
            ast->d.string = flexbuf_get(&anno);
            *ast_ptr = ast;
            // if this is indented and inside a PUB or PRI,
            // then treat it as inline C code
            if (startcol > 1 && startline > L->block_firstline && (L->block_type == BLOCK_PUB || L->block_type == BLOCK_PRI)) {
                return SP_INLINECCODE;
            }
            return SP_ANNOTATION;
        } else if (directive) {
            char *dir;
            flexbuf_addchar(&anno, '\0');
            dir = flexbuf_get(&anno);

            if (!strncmp(dir, "line ", 5)) {
                char *ptr = dir+5;
                int lineno;
                lineno = strtol(ptr, &ptr, 10);
                if (lineno > 0) {
                    if (*ptr == ' ') ptr++;
                    L->fileName = getFileName(ptr);
                    L->lineCounter = lineno;
                }
            }
            free(dir);
        } else {
            flexbuf_addchar(&cb, '\0');
            ast = NewAST(AST_COMMENT, NULL, NULL);
            ast->d.string = flexbuf_get(&cb);
            comment_chain = AddToList(comment_chain, ast);
        }
        goto refetch;
    }

    if (L->eoln && IsSpinLang(language) && (L->block_type == BLOCK_PUB || L->block_type == BLOCK_PRI)) {
        if (c == '\n') {
            c = lexgetc(L);
            goto again;
        }
        /* if there is a pending indent, send it back */
        if (L->pending_indent) {
            lexungetc(L, c);
            --L->pending_indent;
            return SP_INDENT;
        }
        /* on EOF send as many OUTDENTS as we need */
        if (c == EOF) {
            if (L->indentsp > 0) {
                lexungetc(L, c);
                --L->indentsp;
                return SP_OUTDENT;
            }
        }
        /* if our indentation is <= the start value, send back an outdent */
        start_indent = L->colCounter-1;
        if (start_indent <= L->indent[L->indentsp] && L->indentsp > 0) {
            lexungetc(L, c);
            --L->indentsp;
            return SP_OUTDENT;
        }
    }
    // force an end-of line at EOF
    if (c == EOF) {
      if (!L->eoln && !L->eof) {
        L->eof = L->eoln = 1;
        if (IsBasicLang(language)) {
            L->firstNonBlank = 0;
        }
        if (eoln_token == ' ') goto refetch;
        return eoln_token;
      }
      return eof_token;
    }
    if (L->eoln) {
        L->eoln = 0;
        L->firstNonBlank = L->colCounter-1;
    }
    if (c == '\n') {
        L->eoln = 1;
        if (eoln_token == ' ') goto refetch;
        return eoln_token;
    }
    if (current && !current->sawToken) {
        current->sawToken = 1;
        current->topcomment = GetComments();
    }
    if (IsBasicLang(language)) {
        // if we have an _ followed by a newline, treat it as a space
        if (c == '_') {
            int d;
            d = lexgetc(L);
            if (d == ' ' || d == '\t' || d == '\n') {
                do {
                    c = lexgetc(L);
                } while (c == ' ' || c == '\t' || c == '\n');
                goto again;
            }
            lexungetc(L, d);
        }
    }
    return c;
}


static char operator_chars[] = "-+*/|<>=!@~#^.?&";

int
getSpinToken(LexStream *L, AST **ast_ptr)
{
//    int base = 10;
    int c;
    AST *ast = NULL;
    int at_startofline = (L->eoln == 1);
    int peekc;
    
    c = skipSpace(L, &ast, LANG_SPIN_SPIN1);

    if (c == EOF) {
      c = SP_EOF;
    }
//    printf("L->linecounter=%d\n", L->lineCounter);
    if (c >= 127) {
        *ast_ptr = last_ast = ast;
        return c;
    } else if (safe_isdigit(c) || (c == '.' && safe_isdigit(lexpeekc(L)))) {
        lexungetc(L,c);
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = parseNumber(L, 10, &ast->d.ival);
        if (c == SP_FLOATNUM)
            ast->kind = AST_FLOAT;
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
        c = parseSpinIdentifier(L, &ast, NULL);
        /* if in pasm, and at start of line, restart temporary
           labels */
        if (c == SP_IDENTIFIER && InDatBlock(L) && at_startofline) {
            L->lastGlobal = ast->d.string;
        }
    } else if (c == ':') {
        peekc = lexgetc(L);
        if (peekc == '=') {
            c = SP_ASSIGN;
        } else if (isIdentifierStart(peekc) && InDatBlock(L)) {
            lexungetc(L, peekc);
            if (gl_p2) {
                SYNTAX_ERROR("in P2 temporary labels must start with . rather than :");
            }
            c = parseSpinIdentifier(L, &ast, L->lastGlobal ? L->lastGlobal : "");
        } else {
            lexungetc(L, peekc);
        }
    } else if (gl_p2 && c == '.' && isIdentifierStart(lexpeekc(L)) && InDatBlock(L)) {
            c = parseSpinIdentifier(L, &ast, L->lastGlobal ? L->lastGlobal : "");
    } else if (strchr(operator_chars, c) != NULL) {
        char op[6];
        int i;
        int token;
        Symbol *sym = NULL;

        op[0] = token = c;
        op[1] = 0;
        
        // have to special case single character operators
        if (L->language == LANG_SPIN_SPIN2) {
            sym = FindSymbol(&spin2ReservedWords, op);
        } else {
            sym = FindSymbol(&spin1ReservedWords, op);
        }
        if (sym) {
            token = INTVAL(sym);
        }
        // now check for more characters    
        for (i = 1; i < sizeof(op)-1; i++) {
            c = lexgetc(L);
            if (c >= 128 || c < 0 || strchr(operator_chars, c) == NULL) {
                lexungetc(L, c);
                break;
            }
            op[i] = c;
            op[i+1] = 0;
            if (L->language == LANG_SPIN_SPIN2) {
                sym = FindSymbol(&spin2ReservedWords, op);
            } else {
                sym = FindSymbol(&spin1ReservedWords, op);
            }
            if (!sym) {
                sym = FindSymbol(&spinCommonReservedWords, op);
            }
            if (sym) {
                token = INTVAL(sym);
            } else {
                lexungetc(L, c);
                break;
            }
        }
        c = token;
    } else if (c == '"') {
        parseString(L, &ast);
        c = SP_STRING;
    }
    *ast_ptr = last_ast = ast;
    return c;
}


/*
 * function to initialize the lexer
 */
struct reservedword {
    const char *name;
    intptr_t val;
} init_spin_words[] = {
    { "abort", SP_ABORT },
    { "abs", SP_ABS },
    { "alignl", SP_ALIGNL }, // NON-STANDARD
    { "alignw", SP_ALIGNW }, // NON-STANDARD
    { "and", SP_AND },
    { "__andthen__", SP_ANDTHEN },
    { "asm", SP_ASM },  // NON-STANDARD

    { "__builtin_alloca", SP_ALLOCA }, // NON-STANDARD
    { "byte", SP_BYTE },

    { "case", SP_CASE },
    { "case_fast", SP_CASE_FAST }, // NON-STANDARD
    { "cognew", SP_COGNEW },
    { "coginit", SP_COGINIT },
    { "con", SP_CON },

    { "dat", SP_DAT },

    { "else", SP_ELSE },
    { "elseif", SP_ELSEIF },
    { "elseifnot", SP_ELSEIFNOT },
    { "end",   SP_END },   // NON-STANDARD
    { "endasm", SP_ENDASM }, // NON-STANDARD

    { "file", SP_FILE },
    { "fit", SP_FIT },
    { "float", SP_FLOAT },
    { "from", SP_FROM },

    { "if", SP_IF },
    { "ifnot", SP_IFNOT },

    { "long", SP_LONG },
    { "lookdown", SP_LOOKDOWN },
    { "lookdownz", SP_LOOKDOWNZ },
    { "lookup", SP_LOOKUP },
    { "lookupz", SP_LOOKUPZ },

    { "next", SP_NEXT },
    { "not", SP_NOT },

    { "obj", SP_OBJ },
    { "or", SP_OR },
    { "__orelse__", SP_ORELSE },
    { "org", SP_ORG },
    { "orgh", SP_ORGH }, // NON-STANDARD
    { "orgf", SP_ORGF }, // NON-STANDARD
    { "other", SP_OTHER },

    { "quit", SP_QUIT },
    { "pri", SP_PRI },
    { "pub", SP_PUB },
    { "repeat", SP_REPEAT },
    { "res", SP_RES },
    { "rev", SP_REV2 },
    { "result", SP_RESULT },
    { "return", SP_RETURN },
    { "rol", SP_ROTL },
    { "ror", SP_ROTR },
    { "round", SP_ROUND },

    { "sar", SP_SAR },
    { "spr", SP_SPR },
    { "step", SP_STEP },
    { "string", SP_STRINGPTR },
    { "to", SP_TO },
    { "trunc", SP_TRUNC },
    { "then", SP_THEN },

    { "until", SP_UNTIL },

    { "var", SP_VAR },

    { "while", SP_WHILE },
    { "word", SP_WORD },

    { "xor", SP_XOR },
    
    /* operators */
    { "+", '+' },
    { "-", '-' },
    { "/", '/' },
    { "?", '?' },
    { "//", SP_REMAINDER },
    { "///", SP_FRAC },
    { "+/", SP_UNSDIV },
    { "+//", SP_UNSMOD },
    { "*", '*' },
    { "**", SP_HIGHMULT },
    { "+**", SP_UNSHIGHMULT },
    { ">", '>' },
    { "<", '<' },
    { "=<", SP_LE },
    { "=>", SP_GE },
    { "<=>", SP_SGNCOMP },
    { "+>", SP_GTU },
    { "+<", SP_LTU },
    { "+=>", SP_GEU },
    { "+=<", SP_LEU },
    { "=", '=' },
    { "==", SP_EQ },
    { "<>", SP_NE },

    { "><", SP_REV },
    { "->", SP_ROTR },
    { "<-", SP_ROTL },

    { "<<", SP_SHL },
    { ">>", SP_SHR },
    { "~>", SP_SAR },

    { "..", SP_DOTS },
    { "|<", SP_DECODE },
    { ">|", SP_ENCODE },
    { "||", SP_ABS },
    { "#>", SP_LIMITMIN },
    { "<#", SP_LIMITMAX },

    { "~~", SP_DOUBLETILDE },
    { "++", SP_INCREMENT },
    { "--", SP_DECREMENT },
    { "^^", SP_SQRT },

    { "??", SP_RANDOM },
    
    { "@@", SP_DOUBLEAT },
    { "@@@", SP_TRIPLEAT },
};

struct reservedword init_spin1_words[] = {
    { "constant", SP_CONSTANT },

    { "?", SP_RANDOM },
};

struct reservedword init_spin2_words[] = {
    { "_", SP_EMPTY },
    { "^^", SP_XOR },
    { "&&", SP_AND },
    { "||", SP_OR },
    { "!!", SP_NOT },
    { "<=", SP_LE },
    { ">=", SP_GE },
    { "+<=", SP_LEU },
    { "+>=", SP_GEU },
    
    { "addbits", SP_ADDBITS },
    { "addpins", SP_ADDPINS },
    { "bmask", SP_BMASK },
    { "cogspin", SP_COGINIT },
    { "debug", SP_DEBUG },
    { "decod", SP_DECODE },
    { "encod", SP_ENCODE2 },
    { "frac", SP_FRAC },
    { "fvar", SP_FVAR },
    { "fvars", SP_FVARS },
    { "ones", SP_ONES },
    { "qexp", SP_QEXP },
    { "qlog", SP_QLOG },
    { "reg", SP_COGREG },
    { "sca", SP_UNSHIGHMULT },
    { "scas", SP_SCAS },
    { "signx", SP_SIGNX },
    { "sqrt", SP_SQRT },
    { "zerox", SP_ZEROX },
};

struct reservedword basic_keywords[] = {
  { "_", BAS_EMPTY },
  
  { "abs", BAS_ABS },
  { "and", BAS_AND },
  { "andalso", BAS_ANDALSO },
  { "any", BAS_ANY },
  { "append", BAS_APPEND },
  { "as", BAS_AS },
  { "asc", BAS_ASC },
  { "asm", BAS_ASM },
  { "__builtin_alloca", BAS_ALLOCA },
  { "byref", BAS_BYREF },
  { "byte", BAS_BYTE },
  { "byval", BAS_BYVAL },
  { "case", BAS_CASE },
  { "cast", BAS_CAST },
  { "catch", BAS_CATCH },
  { "class", BAS_CLASS },
  { "close", BAS_CLOSE },
  { "const", BAS_CONST },
  { "continue", BAS_CONTINUE },
  { "cpu", BAS_CPU },
  { "data", BAS_DATA },
  { "declare", BAS_DECLARE },
  { "def", BAS_DEF },
  { "defint", BAS_DEFINT },
  { "defsng", BAS_DEFSNG },
  { "delete", BAS_DELETE },
  { "dim", BAS_DIM },
  { "direction", BAS_DIRECTION },
  { "do", BAS_DO },
  { "double", BAS_DOUBLE },
  { "else", BAS_ELSE },
  { "end", BAS_END },
  { "endif", BAS_ENDIF },
  { "enum", BAS_ENUM },
  { "exit", BAS_EXIT },
  { "for", BAS_FOR },
  { "function", BAS_FUNCTION },
  { "goto", BAS_GOTO },
  { "gosub", BAS_GOSUB },
  { "if", BAS_IF },
  { "import", BAS_IMPORT },
  { "input", BAS_INPUT },
  { "int", BAS_CAST_INT },
  { "integer", BAS_INTEGER_KW },
  { "let", BAS_LET },
  { "long", BAS_LONG },
  { "longint", BAS_LONGINT },
  { "loop", BAS_LOOP },
  { "mod", BAS_MOD },
  { "new", BAS_NEW },
  { "next", BAS_NEXT },
  { "not", BAS_NOT },
  { "nil", BAS_NIL },
  { "on", BAS_ON },
  { "open", BAS_OPEN },
  { "option", BAS_OPTION },
  { "or", BAS_OR },
  { "orelse", BAS_ORELSE },
  { "output", BAS_OUTPUT },
  { "pointer", BAS_POINTER },
  { "print", BAS_PRINT },
  { "program", BAS_PROGRAM },
  { "ptr", BAS_PTR },
  { "put", BAS_PUT },
  { "read", BAS_READ },
  { "restore", BAS_RESTORE },
  { "return", BAS_RETURN },
  { "select", BAS_SELECT },
  { "self", BAS_SELF },
  { "shared", BAS_SHARED },
  { "shl", BAS_SHL },
  { "short", BAS_SHORT },
  { "shr", BAS_SHR },
  { "single", BAS_SINGLE },
  { "sqr", BAS_SQRT },
  { "sqrt", BAS_SQRT },
  { "step", BAS_STEP },
  { "string", BAS_STRING_KW },
  { "struct", BAS_STRUCT },
  { "sub", BAS_SUB },
  { "subroutine", BAS_SUB },
  { "then", BAS_THEN },
  { "throw", BAS_THROW },
  { "to", BAS_TO },
  { "try", BAS_TRY },
  { "type", BAS_TYPE },
  { "ubyte", BAS_UBYTE },
  { "uinteger", BAS_UINTEGER },
  { "ulong", BAS_ULONG },
  { "ulongint", BAS_ULONGINT },
  { "until", BAS_UNTIL },
  { "ushort", BAS_USHORT },
  { "using", BAS_USING },
  { "var", BAS_VAR },
  { "wend", BAS_WEND },
  { "while", BAS_WHILE },
  { "with", BAS_WITH },
  { "word", BAS_WORD },
  { "xor", BAS_XOR },
};

struct reservedword c_keywords[] = {
  { "__asm", C_ASM },
  { "__attribute__", C_ATTRIBUTE },
  { "_Bool", C_BOOL },
  { "break", C_BREAK },
  {"__builtin_abs", C_BUILTIN_ABS },
  { "__builtin_alloca", C_BUILTIN_ALLOCA },
  { "__builtin_cogstart", C_BUILTIN_COGSTART },
  { "__builtin_expect", C_BUILTIN_EXPECT },
  { "__builtin_longjmp", C_BUILTIN_LONGJMP },
  { "__builtin_printf", C_BUILTIN_PRINTF },
  { "__builtin_propeller_rev", C_BUILTIN_REV },
  { "__builtin_setjmp", C_BUILTIN_SETJMP },
  { "__builtin_sqrt", C_BUILTIN_SQRT },
  { "__builtin_va_arg", C_BUILTIN_VA_ARG },
  { "__builtin_va_start", C_BUILTIN_VA_START },
  { "case", C_CASE },
  { "__class", C_CLASS },
  { "char", C_CHAR },
  { "const", C_CONST },
  { "continue", C_CONTINUE },
  { "default", C_DEFAULT },
  { "do", C_DO },
  { "double", C_DOUBLE },
  { "else", C_ELSE },
  { "enum", C_ENUM },
  { "extern", C_EXTERN },
  { "float", C_FLOAT },
  { "for", C_FOR },
  { "__fromfile", C_FROMFILE },
  { "goto", C_GOTO },
  { "if", C_IF },
  { "_Imaginary", C_IMAGINARY },
  { "__inline", C_INLINE },
  { "__inline__", C_INLINE },
  { "inline", C_INLINE },
  { "int", C_INT },
  { "long", C_LONG },
  { "__pasm", C_PASM },
  { "__restrict", C_RESTRICT },
  { "register", C_REGISTER },
  { "return", C_RETURN },
  { "short", C_SHORT },
  { "signed", C_SIGNED },
  { "sizeof", C_SIZEOF },
  { "static", C_STATIC },
  { "struct", C_STRUCT },
  { "switch", C_SWITCH },
  { "typedef", C_TYPEDEF },
  { "__this", C_THIS },
  { "union", C_UNION },
  { "unsigned", C_UNSIGNED },
  { "__using", C_USING },
  { "void", C_VOID },
  { "volatile", C_VOLATILE },
  { "while", C_WHILE },
};
// C++ keywords
struct reservedword cpp_keywords[] = {
  { "catch", C_CATCH },
  { "class", C_CLASS },
  { "nullptr", C_NULLPTR },
  { "private", C_PRIVATE },
  { "public", C_PUBLIC },
  { "template", C_TEMPLATE },
  { "this", C_THIS },
  { "throw", C_THROW },
};

// keywords reserved in BASIC only in asm blocks
struct reservedword bas_pasm_keywords[] = {
  { "alignl", BAS_ALIGNL },
  { "alignw", BAS_ALIGNW },
  { "asm", BAS_ASM },
  { "byte", BAS_BYTE },
  { "end", BAS_END },
  { "file", BAS_FILE },
  { "fit", BAS_FIT },
  { "long", BAS_LONG },
  { "org", BAS_ORG },
  { "orgf", BAS_ORGF },
  { "orgh", BAS_ORGH },
  { "res", BAS_RES },
  { "shared", BAS_SHARED },
  { "word", BAS_WORD },
};
// keywords reserved in C only with __asm blocks
struct reservedword c_pasm_keywords[] = {
  { "alignl", C_ALIGNL },
  { "alignw", C_ALIGNW },
  { "byte", C_BYTE },
  { "file", C_FILE },
  { "fit", C_FIT },
  { "org", C_ORG },
  { "orgf", C_ORGF },
  { "orgh", C_ORGH },
  { "res", C_RES },
  { "word", C_WORD },
};

// list of words to detect symbols that may conflict with
// C keywords (used by the C/C++ back end)
static char *c_words[] = {
    "abort",
    "abs",
    "and",
    "and_eq",
    "asm",
    "atexit",
    "atof",
    "atoi",
    "atol",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "bsearch",
    "calloc",
    "case",
    "catch",
    "char",
    "class",
    "compl",
    "const",
    "const_cast",
    "continue",
    "default",
    "delete",
    "div",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "exit",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "free",
    "friend",
    "getenv",
    "goto",
    "if",
    "inline",
    "int",
    "labs",
    "ldiv",
    "long",
    "malloc",
    "memcpy",
    "memmove",
    "memset",
    "mutable",
    "namespace",
    "new",
    "not",
    "not_eq",
    "NULL",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "qsort",
    "rand",
    "register",
    "reinterpret_cast",
    "return",
    "short",
    "signed",
    "sizeof",
    "srand",
    "static",
    "static_cast",
    "strcmp",
    "strcpy",
    "struct",
    "strtod",
    "strtof",
    "strtol",
    "strtoul",
    "system",
    "switch",
    "template",
    "this",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "while",
    "xor",
    "xor_eq"
};

extern void defaultBuiltin(Flexbuf *, Builtin *, AST *);
extern void defaultVariable(Flexbuf *, Builtin *, AST *);
extern void memBuiltin(Flexbuf *, Builtin *, AST *);
extern void memFillBuiltin(Flexbuf *, Builtin *, AST *);
extern void str1Builtin(Flexbuf *, Builtin *, AST *);
extern void strcompBuiltin(Flexbuf *, Builtin *, AST *);
extern void rebootBuiltin(Flexbuf *, Builtin *, AST *);
extern void waitpeqBuiltin(Flexbuf *, Builtin *, AST *);

/* hooks to be called when we recognize a builtin */
static void lockhook(Builtin *dummy) { /*current->needsLockFuncs = 1;*/ }

// the fields here are:
// "spin name", numparameters, outputfunc, cname, c2name, gasname, extradata, parsehook

// c2name is the P2 C version of the name
Builtin builtinfuncs[] = {
    { "__clkfreq_var", 0, defaultVariable, "_clkfreq", "_clockfreq()", NULL, 0, NULL },
    { "__clkmode_var", 0, defaultVariable, "_clkmode", "_clockmode()", NULL, 0, NULL },
    { "clkset", 2, defaultBuiltin, "clkset", "_clkset", "_clkset", 0, NULL },

    { "__builtin_clkfreq", 0, defaultVariable, "_clkfreq", "_clockfreq()", NULL, 0, NULL },
    { "__builtin_clkmode", 0, defaultVariable, "_clkmode", "_clockmode()", NULL, 0, NULL },
    { "_clkset", 2, defaultBuiltin, "clkset", "_clkset", "_clkset", 0, NULL },

    { "_cogstop", 1, defaultBuiltin, "cogstop", "_cogstop", "__builtin_propeller_cogstop", 0, NULL },
    { "_cogid", 0, defaultBuiltin, "cogid", "_cogid", "__builtin_propeller_cogid", 0, NULL },

    { "_locknew", 0, defaultBuiltin, "locknew", "_locknew", "__builtin_propeller_locknew", 0, lockhook },
    { "_lockset", 1, defaultBuiltin, "lockset", "_lockset", "__builtin_propeller_lockset", 0, lockhook },
    { "_lockclr", 1, defaultBuiltin, "lockclr", "_lockclr", "__builtin_propeller_lockclr", 0, lockhook },
    { "_lockret", 1, defaultBuiltin, "lockret", "_lockret", "__builtin_propeller_lockret", 0, lockhook },

    { "strsize", 1, str1Builtin, "strlen", NULL, NULL, 0, NULL },
    { "__builtin_strlen", 1, str1Builtin, "strlen", NULL, NULL, 0, NULL },
    { "strcomp", 2, strcompBuiltin, "strcmp", NULL, NULL, 0, NULL },
    { "waitcnt", 1, defaultBuiltin, "waitcnt", "_waitcnt", "_waitcnt", 0, NULL },
    { "waitpeq", 3, waitpeqBuiltin, "waitpeq", "_waitpeq", "__builtin_propeller_waitpeq", 0, NULL },
    { "waitpne", 3, waitpeqBuiltin, "waitpne", "_waitpne", "__builtin_propeller_waitpne", 0, NULL },

    { "reboot", 0, rebootBuiltin, "reboot", NULL, NULL, 0, NULL },
    { "_reboot", 0, rebootBuiltin, "reboot", NULL, NULL, 0, NULL },

    { "longfill", 3, memFillBuiltin, "memset", NULL, NULL, 4, NULL },
    { "longmove", 3, memBuiltin, "memmove", NULL, NULL, 4, NULL },
    { "wordfill", 3, memFillBuiltin, "memset", NULL, NULL, 2, NULL },
    { "wordmove", 3, memBuiltin, "memmove", NULL, NULL, 2, NULL },
    { "bytefill", 3, memBuiltin, "memset", NULL, NULL, 1, NULL },
    { "bytemove", 3, memBuiltin, "memmove", NULL, NULL, 1, NULL },

    { "getcnt", 0, defaultBuiltin, "getcnt", "_cnt", "_getcnt", 0, NULL },
    { "_getcnt", 0, defaultBuiltin, "getcnt", "_cnt", "_getcnt", 0, NULL },

    { "_string_cmp", 2, defaultBuiltin, "strcmp", NULL, NULL, 0, NULL },

    { "_string_concat", 2, defaultBuiltin, "_string_concat", NULL, NULL, 0, NULL },

    // float support routines
    { "_fixed_div", 3, defaultBuiltin, "fixed_div", NULL, NULL, 0, NULL },
    { "_fixed_mul", 2, defaultBuiltin, "fixed_mul", NULL, NULL, 0, NULL },

    // BASIC compiler builtins
    { "_basic_print_unsigned", 4, defaultBuiltin, "basic_print_unsigned", NULL, NULL, 0, NULL },

    // Spin2 builtins
    { "__builtin_propeller_akpin",    1, defaultBuiltin, "_akpin", "_akpin", NULL, 0, NULL },
    { "__builtin_propeller_rdpin",    1, defaultBuiltin, "_rdpin", "_rdpin", NULL, 0, NULL },
    { "__builtin_propeller_rqpin",    1, defaultBuiltin, "_rqpin", "_rqpin", NULL, 0, NULL },

    { "__builtin_propeller_fltl",     1, defaultBuiltin, "_pinf", "_pinf", NULL, 0, NULL },
    { "__builtin_propeller_drvh",     1, defaultBuiltin, "_pinh", "_pinh", NULL, 0, NULL },
    { "__builtin_propeller_drvl",     1, defaultBuiltin, "_pinl", "_pinl", NULL, 0, NULL },
    { "__builtin_propeller_pinr",     1, defaultBuiltin, "_pinr", "_pinr", NULL, 0, NULL },
    { "_pinwrite", 2, defaultBuiltin, "_pinw", "_pinw", NULL, 0, NULL },

    { "_waitms",   1, defaultBuiltin, "_waitms", "_waitms", NULL, 0, NULL },
    { "_waitus",   1, defaultBuiltin, "_waitus", "_waitus", NULL, 0, NULL },
    
    { "_wrpin",    2, defaultBuiltin, "_wrpin", "_wrpin", NULL, 0, NULL },
    { "_wxpin",    2, defaultBuiltin, "_wxpin", "_wxpin", NULL, 0, NULL },
    { "_wypin",    2, defaultBuiltin, "_wypin", "_wypin", NULL, 0, NULL },

    /* some useful C functions */
    { "printf",    -1, defaultBuiltin, "printf", "printf", NULL, 0, NULL },
};

struct constants {
    const char *name;
    int     type;
    int32_t val;
} p1_constants[] = {
    { "chipver", SYM_CONSTANT, 1 },
    { "true", SYM_CONSTANT, -1 },
    { "false", SYM_CONSTANT, 0 },
    { "posx", SYM_CONSTANT, 0x7fffffff },
    { "negx", SYM_CONSTANT, 0x80000000U },
    { "rcfast", SYM_CONSTANT, 0x00000001 },
    { "rcslow", SYM_CONSTANT, 0x00000002 },
    { "xinput", SYM_CONSTANT, 0x00000004 },
    { "xtal1", SYM_CONSTANT, 0x00000008 },
    { "xtal2", SYM_CONSTANT, 0x00000010 },
    { "xtal3", SYM_CONSTANT, 0x00000020 },
    { "pll1x", SYM_CONSTANT, 0x00000040 },
    { "pll2x", SYM_CONSTANT, 0x00000080 },
    { "pll4x", SYM_CONSTANT, 0x00000100 },
    { "pll8x", SYM_CONSTANT, 0x00000200 },
    { "pll16x", SYM_CONSTANT, 0x00000400 },
    { "pi", SYM_FLOAT_CONSTANT, 0x40490fdb },
};

struct constants p2_constants[] = {
    { "chipver", SYM_CONSTANT, 2 },
    { "true", SYM_CONSTANT, -1 },
    { "false", SYM_CONSTANT, 0 },
    { "posx", SYM_CONSTANT, 0x7fffffff },
    { "negx", SYM_CONSTANT, 0x80000000U },
    { "rcfast", SYM_CONSTANT, 0x00000001 },
    { "rcslow", SYM_CONSTANT, 0x00000002 },
    { "xinput", SYM_CONSTANT, 0x00000004 },
    { "xtal1", SYM_CONSTANT, 0x00000008 },
    { "xtal2", SYM_CONSTANT, 0x00000010 },
    { "xtal3", SYM_CONSTANT, 0x00000020 },
    { "pll1x", SYM_CONSTANT, 0x00000040 },
    { "pll2x", SYM_CONSTANT, 0x00000080 },
    { "pll4x", SYM_CONSTANT, 0x00000100 },
    { "pll8x", SYM_CONSTANT, 0x00000200 },
    { "pll16x", SYM_CONSTANT, 0x00000400 },
    { "pi", SYM_FLOAT_CONSTANT, 0x40490fdb },

    { "_clr", SYM_CONSTANT, 0x0 },
    { "_nc_and_nz", SYM_CONSTANT, 0x1 },
    { "_nz_and_nc", SYM_CONSTANT, 0x1 },
    { "_gt",        SYM_CONSTANT, 0x1 },
    { "_nc_and_z",  SYM_CONSTANT, 0x2 },
    { "_z_and_nc",  SYM_CONSTANT, 0x2 },
    { "_nc",        SYM_CONSTANT, 0x3 },
    { "_ge",        SYM_CONSTANT, 0x3 },
    { "_c_and_nz",  SYM_CONSTANT, 0x4 },
    { "_nz_and_c",  SYM_CONSTANT, 0x4 },
    { "_nz",        SYM_CONSTANT, 0x5 },
    { "_ne",        SYM_CONSTANT, 0x5 },
    { "_c_ne_z",    SYM_CONSTANT, 0x6 },
    { "_z_ne_c",    SYM_CONSTANT, 0x6 },
    { "_nz_or_nc",  SYM_CONSTANT, 0x7 },
    { "_nc_or_nz",  SYM_CONSTANT, 0x7 },

    { "_c_and_z",   SYM_CONSTANT, 0x8 },
    { "_z_and_c",   SYM_CONSTANT, 0x8 },
    { "_c_eq_z",    SYM_CONSTANT, 0x9 },
    { "_z_eq_c",    SYM_CONSTANT, 0x9 },
    { "_z",         SYM_CONSTANT, 0xA },
    { "_e",         SYM_CONSTANT, 0xA },
    { "_z_or_nc",   SYM_CONSTANT, 0xB },
    { "_nc_or_z",   SYM_CONSTANT, 0xB },
    { "_c",         SYM_CONSTANT, 0xC },
    { "_lt",        SYM_CONSTANT, 0xC },
    { "_c_or_nz",   SYM_CONSTANT, 0xD },
    { "_nz_or_c",   SYM_CONSTANT, 0xD },
    { "_z_or_c",    SYM_CONSTANT, 0xE },
    { "_c_or_z",    SYM_CONSTANT, 0xE },
    { "_le",        SYM_CONSTANT, 0xE },
    { "_set",       SYM_CONSTANT, 0xF },

    { "p_true_a",   SYM_CONSTANT, 0 },
    { "p_invert_a", SYM_CONSTANT, 0x80000000 },
    { "p_local_a", SYM_CONSTANT, 0x0 },
    { "p_plus1_a", SYM_CONSTANT, 0x10000000 },
    { "p_plus2_a", SYM_CONSTANT, 0x20000000 },
    { "p_plus3_a", SYM_CONSTANT, 0x30000000 },
    { "p_outbit_a", SYM_CONSTANT, 0x40000000 },
    { "p_minus3_a", SYM_CONSTANT, 0x50000000 },
    { "p_minus2_a", SYM_CONSTANT, 0x60000000 },
    { "p_minus1_a", SYM_CONSTANT, 0x70000000 },
    { "p_true_b", SYM_CONSTANT, 0x0 },
    { "p_invert_b", SYM_CONSTANT, 0x8000000 },
    { "p_local_b", SYM_CONSTANT, 0x0 },
    { "p_plus1_b", SYM_CONSTANT, 0x1000000 },
    { "p_plus2_b", SYM_CONSTANT, 0x2000000 },
    { "p_plus3_b", SYM_CONSTANT, 0x3000000 },
    { "p_outbit_b", SYM_CONSTANT, 0x4000000 },
    { "p_minus3_b", SYM_CONSTANT, 0x5000000 },
    { "p_minus2_b", SYM_CONSTANT, 0x6000000 },
    { "p_minus1_b", SYM_CONSTANT, 0x7000000 },
    { "p_pass_ab", SYM_CONSTANT, 0x0 },
    { "p_and_ab", SYM_CONSTANT, 0x200000 },
    { "p_or_ab", SYM_CONSTANT, 0x400000 },
    { "p_xor_ab", SYM_CONSTANT, 0x600000 },
    { "p_filt0_ab", SYM_CONSTANT, 0x800000 },
    { "p_filt1_ab", SYM_CONSTANT, 0xa00000 },
    { "p_filt2_ab", SYM_CONSTANT, 0xc00000 },
    { "p_filt3_ab", SYM_CONSTANT, 0xe00000 },
    { "p_logic_a", SYM_CONSTANT, 0x0 },
    { "p_logic_a_fb", SYM_CONSTANT, 0x20000 },
    { "p_logic_b_fb", SYM_CONSTANT, 0x40000 },
    { "p_schmitt_a", SYM_CONSTANT, 0x60000 },
    { "p_schmitt_a_fb", SYM_CONSTANT, 0x80000 },
    { "p_schmitt_b_fb", SYM_CONSTANT, 0xa0000 },
    { "p_compare_ab", SYM_CONSTANT, 0xc0000 },
    { "p_compare_ab_fb", SYM_CONSTANT, 0xe0000 },
    { "p_adc_gio", SYM_CONSTANT, 0x100000 },
    { "p_adc_vio", SYM_CONSTANT, 0x108000 },
    { "p_adc_float", SYM_CONSTANT, 0x110000 },
    { "p_adc_1x", SYM_CONSTANT, 0x118000 },
    { "p_adc_3x", SYM_CONSTANT, 0x120000 },
    { "p_adc_10x", SYM_CONSTANT, 0x128000 },
    { "p_adc_30x", SYM_CONSTANT, 0x130000 },
    { "p_adc_100x", SYM_CONSTANT, 0x138000 },
    { "p_dac_990r_3v", SYM_CONSTANT, 0x140000 },
    { "p_dac_600r_2v", SYM_CONSTANT, 0x150000 },
    { "p_dac_124r_3v", SYM_CONSTANT, 0x160000 },
    { "p_dac_75r_2v", SYM_CONSTANT, 0x170000 },
    { "p_level_a", SYM_CONSTANT, 0x180000 },
    { "p_level_a_fbn", SYM_CONSTANT, 0x1a0000 },
    { "p_level_b_fbp", SYM_CONSTANT, 0x1c0000 },
    { "p_level_b_fbn", SYM_CONSTANT, 0x1e0000 },
    { "p_async_io", SYM_CONSTANT, 0x0 },
    { "p_sync_io", SYM_CONSTANT, 0x10000 },
    { "p_true_in", SYM_CONSTANT, 0x0 },
    { "p_invert_in", SYM_CONSTANT, 0x8000 },
    { "p_true_output", SYM_CONSTANT, 0x0 },
    { "p_invert_output", SYM_CONSTANT, 0x4000 },
    { "p_high_fast", SYM_CONSTANT, 0x0 },
    { "p_high_1k5", SYM_CONSTANT, 0x800 },
    { "p_high_15k", SYM_CONSTANT, 0x1000 },
    { "p_high_150k", SYM_CONSTANT, 0x1800 },
    { "p_high_1ma", SYM_CONSTANT, 0x2000 },
    { "p_high_100ua", SYM_CONSTANT, 0x2800 },
    { "p_high_10ua", SYM_CONSTANT, 0x3000 },
    { "p_high_float", SYM_CONSTANT, 0x3800 },
    { "p_low_fast", SYM_CONSTANT, 0x0 },
    { "p_low_1k5", SYM_CONSTANT, 0x100 },
    { "p_low_15k", SYM_CONSTANT, 0x200 },
    { "p_low_150k", SYM_CONSTANT, 0x300 },
    { "p_low_1ma", SYM_CONSTANT, 0x400 },
    { "p_low_100ua", SYM_CONSTANT, 0x500 },
    { "p_low_10ua", SYM_CONSTANT, 0x600 },
    { "p_low_float", SYM_CONSTANT, 0x700 },
    { "p_tt_00", SYM_CONSTANT, 0x0 },
    { "p_tt_01", SYM_CONSTANT, 0x40 },
    { "p_tt_10", SYM_CONSTANT, 0x80 },
    { "p_tt_11", SYM_CONSTANT, 0xc0 },
    { "p_oe", SYM_CONSTANT, 0x40 },
    { "p_channel", SYM_CONSTANT, 0x40 },
    { "p_bitdac", SYM_CONSTANT, 0x80 },
    { "p_normal", SYM_CONSTANT, 0x0 },
    { "p_repository", SYM_CONSTANT, 0x2 },
    { "p_dac_noise", SYM_CONSTANT, 0x2 },
    { "p_dac_dither_rnd", SYM_CONSTANT, 0x4 },
    { "p_dac_dither_pwm", SYM_CONSTANT, 0x6 },
    { "p_pulse", SYM_CONSTANT, 0x8 },
    { "p_transition", SYM_CONSTANT, 0xa },
    { "p_nco_freq", SYM_CONSTANT, 0xc },
    { "p_nco_duty", SYM_CONSTANT, 0xe },
    { "p_pwm_triangle", SYM_CONSTANT, 0x10 },
    { "p_pwm_sawtooth", SYM_CONSTANT, 0x12 },
    { "p_pwm_smps", SYM_CONSTANT, 0x14 },
    { "p_quadrature", SYM_CONSTANT, 0x16 },
    { "p_reg_up", SYM_CONSTANT, 0x18 },
    { "p_reg_up_down", SYM_CONSTANT, 0x1a },
    { "p_count_rises", SYM_CONSTANT, 0x1c },
    { "p_count_highs", SYM_CONSTANT, 0x1e },
    
    { "p_state_ticks", SYM_CONSTANT, 0x20 },
    { "p_high_ticks",  SYM_CONSTANT, 0x22 },
    { "p_events_ticks", SYM_CONSTANT, 0x24 },
    { "p_periods_ticks", SYM_CONSTANT, 0x26 },
    { "p_periods_highs", SYM_CONSTANT, 0x28 },
    { "p_counter_ticks", SYM_CONSTANT, 0x2a },
    { "p_counter_highs", SYM_CONSTANT, 0x2c },
    { "p_counter_periods", SYM_CONSTANT, 0x2e },
    
    { "p_adc",      SYM_CONSTANT, 0x30 },
    { "p_adc_ext",  SYM_CONSTANT, 0x32 },
    { "p_adc_scope", SYM_CONSTANT, 0x34 },
    { "p_usb_pair", SYM_CONSTANT, 0x36 },
    { "p_sync_tx",  SYM_CONSTANT, 0x38 },
    { "p_sync_rx",  SYM_CONSTANT, 0x3a },
    { "p_async_tx", SYM_CONSTANT, 0x3c },
    { "p_async_rx", SYM_CONSTANT, 0x3e },

    { "x_rfbyte_1p_1dac1", SYM_CONSTANT, 0x8000 << 16 },
    { "x_rfbyte_2p_2dac1", SYM_CONSTANT, 0x9000 << 16 },
    { "x_rfbyte_2p_1dac2", SYM_CONSTANT, 0x9002 << 16 },
    { "x_rfbyte_4p_4dac1", SYM_CONSTANT, 0xa000 << 16 },
    { "x_rfbyte_4p_2dac2", SYM_CONSTANT, 0xa002 << 16 },
    { "x_rfbyte_4p_1dac4", SYM_CONSTANT, 0xa004 << 16 },
    { "x_rfbyte_8p_4dac2", SYM_CONSTANT, 0xa006 << 16 },
    { "x_rfbyte_8p_2dac4", SYM_CONSTANT, 0xa007 << 16 },
    { "x_rfbyte_8p_1dac8", SYM_CONSTANT, 0xa00e << 16 },
    { "x_rfword_16p_4dac4", SYM_CONSTANT, 0xa00f << 16 },
    { "x_rfword_16p_2dac8", SYM_CONSTANT, 0xb000 << 16 },
    { "x_rflong_32p_4dac8", SYM_CONSTANT, 0xb001 << 16 },

    { "x_rfbyte_luma8", SYM_CONSTANT, 0xb002 << 16 },
    { "x_rfbyte_rgbi8", SYM_CONSTANT, 0xb003 << 16 },
    { "x_rfbyte_rgb8", SYM_CONSTANT, 0xb004 << 16 },
    { "x_rfword_rgb16", SYM_CONSTANT, 0xb005 << 16 },
    { "x_rflong_rgb24", SYM_CONSTANT, 0xb006 << 16 },
    
    { "cogexec", SYM_CONSTANT, 0 },
    { "cogexec_new", SYM_CONSTANT, 0x10 },
    { "hubexec", SYM_CONSTANT, 0x20 },
    { "hubexec_new", SYM_CONSTANT, 0x30 },
    { "cogexec_new_pair", SYM_CONSTANT, 0x11 },
    { "hubexec_new_pair", SYM_CONSTANT, 0x31 },
};

#if defined(WIN32)
#include <windows.h>
#include <psapi.h>
#endif

#if defined(MACOSX)
#include <mach-o/dyld.h>
#endif

#if defined(LINUX)
#include <unistd.h>
#endif

#if defined(WIN32)
#define PATH_SEP    ';'
#define DIR_SEP     '\\'
#define DIR_SEP_STR "\\"
#else
#define PATH_SEP    ':'
#define DIR_SEP     '/'
#define DIR_SEP_STR "/"
#endif

int 
getProgramPath(const char **argv, char *path, int size)
{
#if defined(WIN32)

#if defined(Q_OS_WIN32) || defined(MINGW)
    /* get the full path to the executable */
    if (!GetModuleFileNameA(NULL, path, size))
        return -1;
#else
    /* get the full path to the executable */
    if (!GetModuleFileNameEx(GetCurrentProcess(), NULL, path, size))
        return -1;
#endif  /* Q_OS_WIN32 */

#elif defined(LINUX)
    int r;
    r = readlink("/proc/self/exe", path, size - 1);
    if (r >= 0)
      path[r] = 0;
    else
      return -1;
#elif defined(MACOSX)
    uint32_t bufsize = size - 1;
    int r = _NSGetExecutablePath(path, &bufsize);
    if (r < 0)
      return -1;
#else
    /* fall back on argv[0]... probably not the best bet, since
       shells might not put the full path in, but it's the most portable
    */
    strcpy(path, argv[0]);
#endif

    return 0;
}

char gl_prognamebuf[1024];

void InitPreprocessor(const char **argv)
{
    const char *envpath;
    char *progname;
    pp_init(&gl_pp);
    SetPreprocessorLanguage(LANG_SPIN_SPIN1);

    // add a path relative to the executable
    if (argv[0] != NULL) {
        if (getProgramPath(argv, gl_prognamebuf, sizeof(gl_prognamebuf)) != 0) {
            strcpy(gl_prognamebuf, argv[0]);
        }
        progname = strrchr(gl_prognamebuf, '/');
        if (progname) {
            progname++;
        } else {
            progname = gl_prognamebuf;
        }
        strcpy(progname, "../include");
        pp_add_to_path(&gl_pp, gl_prognamebuf);
    }
    // check for environment variables
    envpath = getenv("FLEXCC_INCLUDE_PATH");
    if (!envpath) {
        envpath = getenv("FASTSPIN_INCLUDE_PATH");
    }
    if (envpath) {
        pp_add_to_path(&gl_pp, envpath);
    }
}

void SetPreprocessorLanguage(int language)
{
    if (IsBasicLang(language)) {
        pp_setcomments(&gl_pp, "\'", "/'", "'/");
        //pp_setlinedirective(&gl_pp, "/'#line %d %s'/");   
        //pp_setlinedirective(&gl_pp, "");   
    } else if (IsCLang(language)) {
        pp_setcomments(&gl_pp, "//", "/*", "*/");
        //pp_setlinedirective(&gl_pp, "/*#line %d %s*/");   
    } else {
        pp_setcomments(&gl_pp, "\'", "{", "}");
        //pp_setlinedirective(&gl_pp, "{#line %d %s}");   
    }
}

#ifdef NOTUSED
static char *
NormalizedName(const char *nameOrig)
{
    char *name;
    name = strdup(nameOrig);
    if (gl_normalizeIdents) {
        NormalizeIdentifier(name);
    }
    return name;
}
#endif

void
initSpinLexer(int flags)
{
    int i;

    spinCommonReservedWords.flags |= SYMTAB_FLAG_NOCASE;
    spin1ReservedWords.flags |= SYMTAB_FLAG_NOCASE;
    spin2ReservedWords.flags |= SYMTAB_FLAG_NOCASE;
    basicReservedWords.flags |= SYMTAB_FLAG_NOCASE;
    basicAsmReservedWords.flags |= SYMTAB_FLAG_NOCASE;
    
    /* add our reserved words */
    for (i = 0; i < N_ELEMENTS(init_spin_words); i++) {
        AddSymbol(&spinCommonReservedWords, init_spin_words[i].name, SYM_RESERVED, (void *)init_spin_words[i].val, NULL);
    }
    for (i = 0; i < N_ELEMENTS(init_spin2_words); i++) {
        AddSymbol(&spin2ReservedWords, init_spin2_words[i].name, SYM_RESERVED, (void *)init_spin2_words[i].val, NULL);
    }
    for (i = 0; i < N_ELEMENTS(init_spin1_words); i++) {
        AddSymbol(&spin1ReservedWords, init_spin1_words[i].name, SYM_RESERVED, (void *)init_spin1_words[i].val, NULL);
    }

    for (i = 0; i < N_ELEMENTS(basic_keywords); i++) {
        AddSymbol(&basicReservedWords, basic_keywords[i].name, SYM_RESERVED, (void *)basic_keywords[i].val, NULL);
    }
    for (i = 0; i < N_ELEMENTS(bas_pasm_keywords); i++) {
        AddSymbol(&basicAsmReservedWords, bas_pasm_keywords[i].name, SYM_RESERVED, (void *)bas_pasm_keywords[i].val, NULL);
    }
    for (i = 0; i < N_ELEMENTS(c_keywords); i++) {
        AddSymbol(&cReservedWords, c_keywords[i].name, SYM_RESERVED, (void *)c_keywords[i].val, NULL);
    }
    for (i = 0; i < N_ELEMENTS(cpp_keywords); i++) {
        AddSymbol(&cppReservedWords, cpp_keywords[i].name, SYM_RESERVED, (void *)cpp_keywords[i].val, NULL);
    }
    for (i = 0; i < N_ELEMENTS(c_pasm_keywords); i++) {
        AddSymbol(&cAsmReservedWords, c_pasm_keywords[i].name, SYM_RESERVED, (void *)c_pasm_keywords[i].val, NULL);
    }
    
    /* add builtin functions */
    for (i = 0; i < N_ELEMENTS(builtinfuncs); i++) {
        AddSymbol(&spinCommonReservedWords, builtinfuncs[i].name, SYM_BUILTIN, (void *)&builtinfuncs[i], NULL);
    }

    /* and builtin constants */
    if (gl_p2) {
        for (i = 0; i < N_ELEMENTS(p2_constants); i++) {
            AddSymbol(&spinCommonReservedWords, p2_constants[i].name, p2_constants[i].type, AstInteger(p2_constants[i].val), NULL);
        }
    } else {
        for (i = 0; i < N_ELEMENTS(p1_constants); i++) {
            AddSymbol(&spinCommonReservedWords, p1_constants[i].name, p1_constants[i].type, AstInteger(p1_constants[i].val), NULL);
        }
    }
    
    /* C keywords */
    for (i = 0; i < N_ELEMENTS(c_words); i++) {
        AddSymbol(&ckeywords, c_words[i], SYM_RESERVED, NULL, NULL);
    }

    /* add the PASM instructions */
    InitPasm(flags);
}

int
IsReservedWord(const char *name)
{
    int x;
    x = FindSymbol(&spinCommonReservedWords, name) != 0;
    if (!x) {
        if (gl_p2) {
            x = FindSymbol(&spin2ReservedWords, name) != 0;
        } else {
            x = FindSymbol(&spin1ReservedWords, name) != 0;
        }
    }
    return x;
}

Instruction *instr;

Instruction
instr_p1[] = {
    { "abs",    0xa8800000, TWO_OPERANDS, OPC_ABS, FLAG_P1_STD },
    { "absneg", 0xac800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "add",    0x80800000, TWO_OPERANDS, OPC_ADD, FLAG_P1_STD },
    { "addabs", 0x88800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "adds",   0xd0800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "addsx",  0xd8800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "addx",   0xc8800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "and",    0x60800000, TWO_OPERANDS, OPC_AND, FLAG_P1_STD },
    { "andn",   0x64800000, TWO_OPERANDS, OPC_ANDN, FLAG_P1_STD },

    { "call",   0x5cc00000, CALL_OPERAND, OPC_CALL, FLAG_P1_STD },
    { "clkset", 0x0c400000, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P1_STD },
    { "cmp",    0x84000000, TWO_OPERANDS, OPC_CMP, FLAG_P1_STD | FLAG_WARN_NOTUSED},
    { "cmps",   0xc0000000, TWO_OPERANDS, OPC_CMPS, FLAG_P1_STD | FLAG_WARN_NOTUSED },
    { "cmpsub", 0xe0800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "cmpsx",  0xc4000000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "cmpx",   0xcc000000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },

    { "cogid",   0x0cc00001, DST_OPERAND_ONLY, OPC_COGID, FLAG_P1_STD },
    { "coginit", 0x0c400002, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P1_STD },
    { "cogstop", 0x0c400003, DST_OPERAND_ONLY, OPC_COGSTOP, FLAG_P1_STD },

    { "djnz",   0xe4800000, JMPRET_OPERANDS, OPC_DJNZ, FLAG_P1_STD },
    { "hubop",  0x0c000000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "jmp",    0x5c000000, JMP_OPERAND, OPC_JUMP, FLAG_P1_STD },
    { "jmpret", 0x5c800000, JMPRET_OPERANDS, OPC_JMPRET, FLAG_P1_STD },

    { "lockclr",0x0c400007, DST_OPERAND_ONLY, OPC_GENERIC_NR, FLAG_P1_STD },
    { "locknew",0x0cc00004, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P1_STD },
    { "lockret",0x0c400005, DST_OPERAND_ONLY, OPC_GENERIC_NR, FLAG_P1_STD },
    { "lockset",0x0c400006, DST_OPERAND_ONLY, OPC_GENERIC_NR, FLAG_P1_STD },

    { "max",    0x4c800000, TWO_OPERANDS, OPC_MAXU, FLAG_P1_STD },
    { "maxs",   0x44800000, TWO_OPERANDS, OPC_MAXS, FLAG_P1_STD },
    { "min",    0x48800000, TWO_OPERANDS, OPC_MINU, FLAG_P1_STD },
    { "mins",   0x40800000, TWO_OPERANDS, OPC_MINS, FLAG_P1_STD },
    { "mov",    0xa0800000, TWO_OPERANDS, OPC_MOV, FLAG_P1_STD },
    { "movd",   0x54800000, TWO_OPERANDS, OPC_MOVD, FLAG_P1_STD },
    { "movi",   0x58800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "movs",   0x50800000, TWO_OPERANDS, OPC_MOVS, FLAG_P1_STD },

    { "muxc",   0x70800000, TWO_OPERANDS, OPC_MUXC, FLAG_P1_STD },
    { "muxnc",  0x74800000, TWO_OPERANDS, OPC_MUXNC, FLAG_P1_STD },
    { "muxz",   0x78800000, TWO_OPERANDS, OPC_MUXZ, FLAG_P1_STD },
    { "muxnz",  0x7c800000, TWO_OPERANDS, OPC_MUXNZ, FLAG_P1_STD },

    { "neg",    0xa4800000, TWO_OPERANDS, OPC_NEG, FLAG_P1_STD },
    { "negc",   0xb0800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "negnc",  0xb4800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "negnz",  0xbc800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "negz",   0xb8800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "nop",    0x00000000, NO_OPERANDS, OPC_NOP, FLAG_P1_STD },

    { "or",     0x68800000, TWO_OPERANDS, OPC_OR, FLAG_P1_STD },

    { "rcl",    0x34800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "rcr",    0x30800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "rdbyte", 0x00800000, TWO_OPERANDS, OPC_RDBYTE, FLAG_P1_STD },
    { "rdlong", 0x08800000, TWO_OPERANDS, OPC_RDLONG, FLAG_P1_STD },
    { "rdword", 0x04800000, TWO_OPERANDS, OPC_RDWORD, FLAG_P1_STD },
    { "ret",    0x5c400000, NO_OPERANDS, OPC_RET, FLAG_P1_STD },
    { "rev",    0x3c800000, TWO_OPERANDS, OPC_REV_P1, FLAG_P1_STD },
    { "rol",    0x24800000, TWO_OPERANDS, OPC_ROL, FLAG_P1_STD },
    { "ror",    0x20800000, TWO_OPERANDS, OPC_ROR, FLAG_P1_STD },

    { "sar",    0x38800000, TWO_OPERANDS, OPC_SAR, FLAG_P1_STD },
    { "shl",    0x2c800000, TWO_OPERANDS, OPC_SHL, FLAG_P1_STD },
    { "shr",    0x28800000, TWO_OPERANDS, OPC_SHR, FLAG_P1_STD },
    { "sub",    0x84800000, TWO_OPERANDS, OPC_SUB, FLAG_P1_STD },
    { "subabs", 0x8c800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "subs",   0xc0800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "subsx",  0xc4800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "subx",   0xcc800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "sumc",   0x90800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "sumnc",  0x94800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "sumnz",  0x9c800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "sumz",   0x98800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },

    { "test",   0x60000000, TWO_OPERANDS, OPC_TEST, FLAG_P1_STD | FLAG_WARN_NOTUSED },
    { "testn",  0x64000000, TWO_OPERANDS, OPC_TESTN, FLAG_P1_STD | FLAG_WARN_NOTUSED },
    { "tjnz",   0xe8000000, JMPRET_OPERANDS, OPC_GENERIC_BRCOND, FLAG_P1_STD },
    { "tjz",    0xec000000, JMPRET_OPERANDS, OPC_GENERIC_BRCOND, FLAG_P1_STD },

    { "waitcnt", 0xf8800000, TWO_OPERANDS, OPC_WAITCNT, FLAG_P1_STD },
    { "waitpeq", 0xf0000000, TWO_OPERANDS, OPC_GENERIC_NR, FLAG_P1_STD },
    { "waitpne", 0xf4000000, TWO_OPERANDS, OPC_GENERIC_NR, FLAG_P1_STD },
    { "waitvid", 0xfc000000, TWO_OPERANDS, OPC_GENERIC_NR, FLAG_P1_STD },

    { "wrbyte", 0x00000000, TWO_OPERANDS, OPC_WRBYTE, FLAG_P1_STD },
    { "wrlong", 0x08000000, TWO_OPERANDS, OPC_WRLONG, FLAG_P1_STD },
    { "wrword", 0x04000000, TWO_OPERANDS, OPC_WRWORD, FLAG_P1_STD },

    { "xor",    0x6c800000, TWO_OPERANDS, OPC_XOR, FLAG_P1_STD },

    { NULL, 0, NO_OPERANDS, OPC_GENERIC, 0 },
};

Instruction
instr_p2[] = {
    { "nop",    0x00000000, NO_OPERANDS,  OPC_NOP, 0 },
    { "ror",    0x00000000, TWO_OPERANDS, OPC_ROR, FLAG_P2_STD },
    { "rol",    0x00200000, TWO_OPERANDS, OPC_ROL, FLAG_P2_STD },
    { "shr",    0x00400000, TWO_OPERANDS, OPC_SHR, FLAG_P2_STD },
    { "shl",    0x00600000, TWO_OPERANDS, OPC_SHL, FLAG_P2_STD },
    { "rcr",    0x00800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "rcl",    0x00a00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "sar",    0x00c00000, TWO_OPERANDS, OPC_SAR, FLAG_P2_STD },
    { "sal",    0x00e00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "add",    0x01000000, TWO_OPERANDS, OPC_ADD, FLAG_P2_STD },
    { "addx",   0x01200000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "adds",   0x01400000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "addsx",  0x01600000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "sub",    0x01800000, TWO_OPERANDS, OPC_SUB, FLAG_P2_STD },
    { "subx",   0x01a00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "subs",   0x01c00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "subsx",  0x01e00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },

    { "cmp",    0x02000000, TWO_OPERANDS, OPC_CMP, FLAG_P2_STD | FLAG_WARN_NOTUSED },
    { "cmpx",   0x02200000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD | FLAG_WARN_NOTUSED },
    { "cmps",   0x02400000, TWO_OPERANDS, OPC_CMPS, FLAG_P2_STD | FLAG_WARN_NOTUSED },
    { "cmpsx",  0x02600000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD | FLAG_WARN_NOTUSED },
    { "cmpr",   0x02800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD | FLAG_WARN_NOTUSED },
    { "cmpm",   0x02a00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "subr",   0x02c00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "cmpsub", 0x02e00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },

    { "fge",    0x03000000, TWO_OPERANDS, OPC_MINU, FLAG_P2_STD },
    { "fle",    0x03200000, TWO_OPERANDS, OPC_MAXU, FLAG_P2_STD },
    { "fges",   0x03400000, TWO_OPERANDS, OPC_MINS, FLAG_P2_STD },
    { "fles",   0x03600000, TWO_OPERANDS, OPC_MAXS, FLAG_P2_STD },

    { "sumc",   0x03800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "sumnc",  0x03a00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "sumz",   0x03c00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "sumnz",  0x03e00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },

    { "testb",   0x04000000, TWO_OPERANDS, OPC_TESTB, FLAG_P2_CZTEST },
    { "testbn",  0x04200000, TWO_OPERANDS, OPC_TESTBN, FLAG_P2_CZTEST },

    { "bitl",   0x04000000, TWO_OPERANDS, OPC_BITL, FLAG_WCZ },
    { "bith",   0x04200000, TWO_OPERANDS, OPC_BITH, FLAG_WCZ },
    { "bitc",   0x04400000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitnc",  0x04600000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitz",   0x04800000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitnz",  0x04a00000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitrnd", 0x04c00000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitnot", 0x04e00000, TWO_OPERANDS, OPC_BITNOT, FLAG_WCZ },

    { "and",    0x05000000, TWO_OPERANDS, OPC_AND, FLAG_P2_STD },
    { "andn",   0x05200000, TWO_OPERANDS, OPC_ANDN, FLAG_P2_STD },
    { "or",     0x05400000, TWO_OPERANDS, OPC_OR, FLAG_P2_STD },
    { "xor",    0x05600000, TWO_OPERANDS, OPC_XOR, FLAG_P2_STD },
    { "muxc",   0x05800000, TWO_OPERANDS, OPC_MUXC, FLAG_P2_STD },
    { "muxnc",  0x05a00000, TWO_OPERANDS, OPC_MUXNC, FLAG_P2_STD },
    { "muxz",   0x05c00000, TWO_OPERANDS, OPC_MUXZ, FLAG_P2_STD },
    { "muxnz",  0x05e00000, TWO_OPERANDS, OPC_MUXNZ, FLAG_P2_STD },

    { "mov",    0x06000000, TWO_OPERANDS, OPC_MOV, FLAG_P2_STD },
    { "not",    0x06200000, TWO_OPERANDS_OPTIONAL, OPC_NOT, FLAG_P2_STD },
    { "abs",    0x06400000, TWO_OPERANDS_OPTIONAL, OPC_ABS, FLAG_P2_STD },
    { "neg",    0x06600000, TWO_OPERANDS_OPTIONAL, OPC_NEG, FLAG_P2_STD },
    { "negc",   0x06800000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },
    { "negnc",  0x06a00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },
    { "negz",   0x06c00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },
    { "negnz",  0x06e00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },

    { "incmod", 0x07000000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "decmod", 0x07200000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "zerox",  0x07400000, TWO_OPERANDS, OPC_ZEROX, FLAG_P2_STD },
    { "signx",  0x07600000, TWO_OPERANDS, OPC_SIGNX, FLAG_P2_STD },

    { "encod",  0x07800000, TWO_OPERANDS_OPTIONAL, OPC_ENCOD, FLAG_P2_STD },
    { "ones",   0x07a00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },

    { "test",   0x07c00000, TWO_OPERANDS_OPTIONAL, OPC_TEST, FLAG_P2_STD|FLAG_WARN_NOTUSED },
    { "testn",  0x07e00000, TWO_OPERANDS, OPC_GENERIC_NR, FLAG_P2_STD|FLAG_WARN_NOTUSED },

    { "setnib", 0x08000000, THREE_OPERANDS_NIBBLE, OPC_GENERIC, 0 },
    { "getnib", 0x08400000, THREE_OPERANDS_NIBBLE, OPC_GENERIC, 0 },
    { "rolnib", 0x08800000, THREE_OPERANDS_NIBBLE, OPC_GENERIC, 0 },
    { "setbyte", 0x08c00000, THREE_OPERANDS_BYTE, OPC_SETBYTE, 0 },
    { "getbyte", 0x08e00000, THREE_OPERANDS_BYTE, OPC_GETBYTE, 0 },
    { "rolbyte", 0x09000000, THREE_OPERANDS_BYTE, OPC_GENERIC, 0 },
    { "setword", 0x09200000, THREE_OPERANDS_WORD, OPC_SETWORD, 0 },
    { "getword", 0x09300000, THREE_OPERANDS_WORD, OPC_GETWORD, 0 },
    { "rolword", 0x09400000, THREE_OPERANDS_WORD, OPC_GENERIC, 0 },

    { "altsn",  0x09500000, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },
    { "altgn",  0x09580000, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },
    { "altsb",  0x09600000, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },
    { "altgb",  0x09680000, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },
    { "altsw",  0x09700000, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },
    { "altgw",  0x09780000, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },
    { "altr",   0x09800000, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },
    { "altd",   0x09880000, TWO_OPERANDS_DEFZ, OPC_ALTD, 0 },
    { "alts",   0x09900000, TWO_OPERANDS_DEFZ, OPC_ALTS, 0 },
    { "altb",   0x09980000, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },
    { "alti",   0x09a00000, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },
    { "setr",   0x09a80000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "setd",   0x09b00000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "sets",   0x09b80000, TWO_OPERANDS, OPC_GENERIC, 0 },

    { "decod",  0x09c00000, TWO_OPERANDS_OPTIONAL, OPC_DECOD, 0 },
    { "bmask",  0x09c80000, TWO_OPERANDS_OPTIONAL, OPC_BMASK, 0 },
    
    { "crcbit", 0x09d00000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "crcnib", 0x09d80000, TWO_OPERANDS, OPC_GENERIC, 0 },
    
    { "muxnits", 0x09e00000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "muxnibs", 0x09e80000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "muxq",   0x09f00000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "movbyts", 0x09f80000, TWO_OPERANDS, OPC_GENERIC, 0 },

    { "mul",    0x0a000000, TWO_OPERANDS, OPC_MULU, FLAG_WZ },
    { "muls",   0x0a100000, TWO_OPERANDS, OPC_MULS, FLAG_WZ },
    { "sca",    0x0a200000, TWO_OPERANDS, OPC_GENERIC, FLAG_WZ },
    { "scas",   0x0a300000, TWO_OPERANDS, OPC_GENERIC, FLAG_WZ },

    { "addpix", 0x0a400000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "mulpix", 0x0a480000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "blnpix", 0x0a500000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "mixpix", 0x0a580000, TWO_OPERANDS, OPC_GENERIC, 0 },

    { "addct1", 0x0a600000, TWO_OPERANDS, OPC_ADDCT1, 0 },
    { "addct2", 0x0a680000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "addct3", 0x0a700000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "wmlong", 0x0a780000, P2_RDWR_OPERANDS, OPC_GENERIC, 0 },

    { "rqpin",  0x0a800000, TWO_OPERANDS, OPC_GENERIC, FLAG_WC },
    { "rdpin",  0x0a880000, TWO_OPERANDS, OPC_GENERIC, FLAG_WC },
    { "rdlut",  0x0aa00000, P2_RDWR_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
  
    { "rdbyte", 0x0ac00000, P2_RDWR_OPERANDS, OPC_RDBYTE, FLAG_P2_STD },
    { "rdword", 0x0ae00000, P2_RDWR_OPERANDS, OPC_RDWORD, FLAG_P2_STD },
    { "rdlong", 0x0b000000, P2_RDWR_OPERANDS, OPC_RDLONG, FLAG_P2_STD },

    // some aliases from rdlong x, ++ptra
    { "popa",  0x0b00015f, TWO_OPERANDS_DEFZ, OPC_GENERIC, FLAG_P2_STD },
    { "popb",  0x0b0001df, TWO_OPERANDS_DEFZ, OPC_GENERIC, FLAG_P2_STD },
    
    { "calld",  0x0b200000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, FLAG_P2_STD },
    { "reti0",  0x0b3bffff, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "reti1",  0x0b3bfff5, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "reti2",  0x0b3bfff3, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "reti3",  0x0b3bfff1, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "resi0",  0x0b3bfdff, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "resi1",  0x0b3be9f5, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "resi2",  0x0b3be5f3, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "resi3",  0x0b3be1f1, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },

    { "callpa", 0x0b400000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "callpb", 0x0b500000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },

    { "djz",    0x0b600000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "djnz",   0x0b680000, P2_TJZ_OPERANDS, OPC_DJNZ, 0 },
    { "djf",    0x0b700000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "djnf",   0x0b780000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "ijz",    0x0b800000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "ijnz",   0x0b880000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "tjz",    0x0b900000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "tjnz",   0x0b980000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "tjf",    0x0ba00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "tjnf",   0x0ba80000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "tjs",    0x0bb00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "tjns",   0x0bb80000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },
    { "tjv",    0x0bc00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRCOND, 0 },

//  { "jp",     0x0ba00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH },
//  { "jnp",    0x0bb00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH },
    

    { "jint",   0x0bc80000, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jct1",   0x0bc80200, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jct2",   0x0bc80400, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jct3",   0x0bc80600, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jse1",   0x0bc80800, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jse2",   0x0bc80a00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jse3",   0x0bc80c00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jse4",   0x0bc80e00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jpat",   0x0bc81000, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jfbw",   0x0bc81200, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jxmt",   0x0bc81400, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jxfi",   0x0bc81600, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jxro",   0x0bc81800, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jxrl",   0x0bc81a00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jatn",   0x0bc81c00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jqmt",   0x0bc81e00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },

    { "jnint",  0x0bc82000, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnct1",  0x0bc82200, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnct2",  0x0bc82400, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnct3",  0x0bc82600, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnse1",  0x0bc82800, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnse2",  0x0bc82a00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnse3",  0x0bc82c00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnse4",  0x0bc82e00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnpat",  0x0bc83000, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnfbw",  0x0bc83200, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnxmt",  0x0bc83400, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnxfi",  0x0bc83600, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnxro",  0x0bc83800, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnxrl",  0x0bc83a00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnatn",  0x0bc83c00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "jnqmt",  0x0bc83e00, P2_JINT_OPERANDS, OPC_GENERIC_BRANCH, 0 },

    { "setpat", 0x0bf00000, P2_TWO_OPERANDS, OPC_GENERIC_NR, 0 },
    { "wrpin",  0x0c000000, P2_TWO_OPERANDS, OPC_GENERIC_NR, 0 },
    { "akpin",  0x0c080200, SRC_OPERAND_ONLY, OPC_GENERIC_NR, 0 },
    { "wxpin",  0x0c100000, P2_TWO_OPERANDS, OPC_GENERIC_NR, 0 },
    { "wypin",  0x0c200000, P2_TWO_OPERANDS, OPC_GENERIC_NR, 0 },
    { "wrlut",  0x0c300000, P2_RDWR_OPERANDS, OPC_GENERIC_NR, 0 },

    { "wrbyte", 0x0c400000, P2_RDWR_OPERANDS, OPC_WRBYTE, 0 },
    { "wrword", 0x0c500000, P2_RDWR_OPERANDS, OPC_WRWORD, 0 },
    { "wrlong", 0x0c600000, P2_RDWR_OPERANDS, OPC_WRLONG, 0 },

    // some aliases for wrlong x, ptra++ etc.
    { "pusha",  0x0c600161, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },
    { "pushb",  0x0c6001e1, TWO_OPERANDS_DEFZ, OPC_GENERIC, 0 },

    { "rdfast", 0x0c700000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "wrfast", 0x0c800000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "fblock", 0x0c900000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },

    { "xinit",  0x0ca00000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "xstop",  0x0cac0000, NO_OPERANDS, OPC_GENERIC, 0 },
    { "xzero",  0x0cb00000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "xcont",  0x0cc00000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },

    { "rep",    0x0cd00000, P2_TWO_OPERANDS, OPC_REPEAT, 0 },
  
    { "coginit",0x0ce00000, P2_TWO_OPERANDS, OPC_GENERIC, FLAG_WC },
    { "qmul",   0x0d000000, P2_TWO_OPERANDS, OPC_QMUL, 0 },
    { "qdiv",   0x0d100000, P2_TWO_OPERANDS, OPC_QDIV, 0 },
    { "qfrac",  0x0d200000, P2_TWO_OPERANDS, OPC_QFRAC, 0 },
    { "qsqrt",  0x0d300000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "qrotate",0x0d400000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "qvector",0x0d500000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },

    { "hubset", 0x0d600000, P2_DST_CONST_OK,  OPC_HUBSET, 0 },
    { "cogid",  0x0d600001, P2_DST_CONST_OK,  OPC_COGID, FLAG_WC },
    { "cogstop",0x0d600003, P2_DST_CONST_OK,  OPC_COGSTOP, 0 },
    { "locknew",0x0d600004, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_WC },
    { "lockret",0x0d600005, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "locktry",0x0d600006, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WC },
    { "lockrel",0x0d600007, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WC },

    { "qlog",   0x0d60000e, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "qexp",   0x0d60000f, P2_DST_CONST_OK, OPC_GENERIC, 0 },
  
    { "rfbyte", 0x0d600010, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "rfword", 0x0d600011, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "rflong", 0x0d600012, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "rfvar",  0x0d600013, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "rfvars", 0x0d600014, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    
    { "wfbyte", 0x0d600015, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "wfword", 0x0d600016, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "wflong", 0x0d600017, P2_DST_CONST_OK, OPC_GENERIC, 0 },

    { "getqx",  0x0d600018, DST_OPERAND_ONLY, OPC_GETQX, FLAG_P2_STD },
    { "getqy",  0x0d600019, DST_OPERAND_ONLY, OPC_GETQY, FLAG_P2_STD },
    { "getct",  0x0d60001a, DST_OPERAND_ONLY, OPC_GETCT, FLAG_WC },
    { "getrnd", 0x0d60001b, P2_DST_CONST_OK, OPC_GETRND, FLAG_P2_STD },

    { "setdacs",0x0d60001c, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setxfrq",0x0d60001d, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "getxacc",0x0d60001e, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "waitx",  0x0d60001f, P2_DST_CONST_OK, OPC_WAITX, FLAG_P2_STD },
  
    { "setse1", 0x0d600020, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setse2", 0x0d600021, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setse3", 0x0d600022, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setse4", 0x0d600023, P2_DST_CONST_OK, OPC_GENERIC, 0 },
  
    { "pollint",0x0d600024, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollct1",0x0d600224, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollct2",0x0d600424, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollct3",0x0d600624, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollse1",0x0d600824, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollse2",0x0d600a24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollse3",0x0d600c24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollse4",0x0d600e24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollpat",0x0d601024, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollfbw",0x0d601224, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollxmt",0x0d601424, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollxfi",0x0d601624, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollxro",0x0d601824, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollxrl",0x0d601a24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollatn",0x0d601c24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "pollqmt",0x0d601e24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
  
    { "waitint",0x0d602024, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitct1",0x0d602224, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitct2",0x0d602424, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitct3",0x0d602624, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitse1",0x0d602824, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitse2",0x0d602a24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitse3",0x0d602c24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitse4",0x0d602e24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitpat",0x0d603024, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitfbw",0x0d603224, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitxmt",0x0d603424, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitxfi",0x0d603624, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitxro",0x0d603824, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitxrl",0x0d603a24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "waitatn",0x0d603c24, NO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },

    { "allowi", 0x0d604024, NO_OPERANDS, OPC_GENERIC, 0 },
    { "stalli", 0x0d604224, NO_OPERANDS, OPC_GENERIC, 0 },
    { "trgint1",0x0d604424, NO_OPERANDS, OPC_GENERIC, 0 },
    { "trgint2",0x0d604624, NO_OPERANDS, OPC_GENERIC, 0 },
    { "trgint3",0x0d604824, NO_OPERANDS, OPC_GENERIC, 0 },

    { "nixint1",0x0d604a24, NO_OPERANDS, OPC_GENERIC, 0 },
    { "nixint2",0x0d604c24, NO_OPERANDS, OPC_GENERIC, 0 },
    { "nixint3",0x0d604e24, NO_OPERANDS, OPC_GENERIC, 0 },
  
    { "setint1",0x0d600025, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "setint2",0x0d600026, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "setint3",0x0d600027, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },

    { "setq",   0x0d600028, P2_DST_CONST_OK, OPC_SETQ, 0 },
    { "setq2",  0x0d600029, P2_DST_CONST_OK, OPC_SETQ2, 0 },

    { "push",   0x0d60002a, P2_DST_CONST_OK, OPC_PUSH, 0 },
    { "pop",    0x0d60002b, DST_OPERAND_ONLY, OPC_POP, FLAG_P2_STD },

  // indirect jumps via register
  // normally the user will write "jmp x" and the assembler will
  // recognize if x is a register and rewrite it as "jmp.ind x"
    { "jmp.ind", 0x0d60002c, DST_OPERAND_ONLY, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "call.ind",0x0d60002d, DST_OPERAND_ONLY, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "ret",    0x0d64002d, NO_OPERANDS, OPC_RET, FLAG_P2_STD },
    { "calla.ind",0x0d60002e, DST_OPERAND_ONLY, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "reta",   0x0d64002e, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "callb.ind",0x0d60002f, DST_OPERAND_ONLY, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "retb",   0x0d64002f, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },

    { "jmprel", 0x0d600030, P2_DST_CONST_OK, OPC_JMPREL, 0 },
  
    { "skip",   0x0d600031, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "skipf",  0x0d600032, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "execf",  0x0d600033, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },

    { "getptr", 0x0d600034, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "getbrk", 0x0d600035, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "brk",    0x0d600036, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "setluts",0x0d600037, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "lutsoff",0x0d640037, NO_OPERANDS, OPC_GENERIC_NR, 0 },
    { "lutson", 0x0d640237, NO_OPERANDS, OPC_GENERIC_NR, 0 },
  
    { "setcy",  0x0d600038, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "setci",  0x0d600039, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "setcq",  0x0d60003a, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "setcfrq",0x0d60003b, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "setcmod",0x0d60003c, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "setpiv", 0x0d60003d, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "setpix", 0x0d60003e, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "cogatn", 0x0d60003f, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },

    { "testp",  0x0d600040, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_P2_CZTEST },
    { "testpn", 0x0d600041, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_P2_CZTEST },

    { "dirl",   0x0d600040, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "dirh",   0x0d600041, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "dirc",   0x0d600042, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "dirnc",  0x0d600043, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "dirz",   0x0d600044, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "dirnz",  0x0d600045, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "dirrnd", 0x0d600046, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "dirnot", 0x0d600047, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
  
    { "outl",   0x0d600048, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "outh",   0x0d600049, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "outc",   0x0d60004a, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "outnc",  0x0d60004b, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "outz",   0x0d60004c, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "outnz",  0x0d60004d, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "outrnd", 0x0d60004e, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "outnot", 0x0d60004f, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },

    { "fltl",   0x0d600050, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "flth",   0x0d600051, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "fltc",   0x0d600052, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "fltnc",  0x0d600053, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "fltz",   0x0d600054, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "fltnz",  0x0d600055, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "fltrnd", 0x0d600056, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "fltnot", 0x0d600057, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
  
    { "drvl",   0x0d600058, P2_DST_CONST_OK, OPC_DRVL, FLAG_WCZ },
    { "drvh",   0x0d600059, P2_DST_CONST_OK, OPC_DRVH, FLAG_WCZ },
    { "drvc",   0x0d60005a, P2_DST_CONST_OK, OPC_DRVC, FLAG_WCZ },
    { "drvnc",  0x0d60005b, P2_DST_CONST_OK, OPC_DRVNC, FLAG_WCZ },
    { "drvz",   0x0d60005c, P2_DST_CONST_OK, OPC_DRVZ, FLAG_WCZ },
    { "drvnz",  0x0d60005d, P2_DST_CONST_OK, OPC_DRVNZ, FLAG_WCZ },
    { "drvrnd", 0x0d60005e, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },
    { "drvnot", 0x0d60005f, P2_DST_CONST_OK, OPC_GENERIC_NR, FLAG_WCZ },

    { "splitb", 0x0d600060, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "mergeb", 0x0d600061, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "splitw", 0x0d600062, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "mergew", 0x0d600063, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "seussf", 0x0d600064, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "seussr", 0x0d600065, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "rgbsqz", 0x0d600066, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "rgbexp", 0x0d600067, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "xoro32", 0x0d600068, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "rev",    0x0d600069, DST_OPERAND_ONLY, OPC_REV_P2, 0 },
    
    { "rczr",   0x0d60006a, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "rczl",   0x0d60006b, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "wrc",    0x0d60006c, DST_OPERAND_ONLY, OPC_WRC, 0 },
    { "wrnc",   0x0d60006d, DST_OPERAND_ONLY, OPC_WRNC, 0 },
    { "wrz",    0x0d60006e, DST_OPERAND_ONLY, OPC_WRZ, 0 },
    { "wrnz",   0x0d60006f, DST_OPERAND_ONLY, OPC_WRNZ, 0 },
    { "modcz",  0x0d64006f, P2_MODCZ, OPC_GENERIC, FLAG_P2_STD | FLAG_WARN_NOTUSED },
    { "modc",   0x0d64006f, P2_MODCZ, OPC_GENERIC, FLAG_WC | FLAG_WARN_NOTUSED },
    { "modz",   0x0d64006f, P2_MODCZ, OPC_GENERIC, FLAG_WZ | FLAG_WARN_NOTUSED },
    { "setscp", 0x0d600070, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "getscp", 0x0d600071, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    
  // long jumps
    { "jmp" ,   0x0d800000, P2_JUMP, OPC_JUMP, 0 },
    { "call",   0x0da00000, P2_JUMP, OPC_CALL, 0 },
    { "calla",  0x0dc00000, P2_JUMP, OPC_GENERIC_BRANCH, 0 },
    { "callb",  0x0de00000, P2_JUMP, OPC_GENERIC_BRANCH, 0 },

    { "calld.loc",  0x0e000000, P2_CALLD, OPC_GENERIC_BRANCH, 0 },
    { "loc",    0x0e800000, P2_LOC, OPC_GENERIC, 0 },

    { "augs",   0x0f000000, P2_AUG, OPC_GENERIC, 0 },
    { "augd",   0x0f800000, P2_AUG, OPC_GENERIC, 0 },

    { NULL, 0, NO_OPERANDS, OPC_GENERIC},
};

HwReg hwreg_p1[] = {
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

    { NULL, 0, NULL },
};

HwReg hwreg_p2[] = {
    { "ijmp3", 0x1f0, "_IJMP3" },
    { "iret3", 0x1f1, "_IRET3" },
    { "ijmp2", 0x1f2, "_IJMP2" },
    { "iret2", 0x1f3, "_IRET2" },
    { "ijmp1", 0x1f4, "_IJMP1" },
    { "iret1", 0x1f5, "_IRET1" },
    { "pa", 0x1f6, "_PA" },
    { "pb", 0x1f7, "_PB" },
    { "ptra", 0x1f8, "_PTRA" },
    { "ptrb", 0x1f9, "_PTRB" },

    { "dira", 0x1fa, "_DIRA" },
    { "dirb", 0x1fb, "_DIRB" },
    { "outa", 0x1fc, "_OUTA" },
    { "outb", 0x1fd, "_OUTB" },

    { "ina", 0x1fe, "_INA" },
    { "inb", 0x1ff, "_INB" },

    { NULL, 0, NULL },
};

#define COND_MASK_P1 0xffc3ffff
#define COND_MASK_P2 0x0fffffff

InstrModifier modifiers_p1[] = {
    { "if_never",  COND_MASK_P1, 0 },
    { "if_always", COND_MASK_P1 | (0xf<<18), 0 },

    { "if_a",           COND_MASK_P1 | (0x1<<18), 0 },
    { "if_nc_and_nz",   COND_MASK_P1 | (0x1<<18), 0 },
    { "if_nz_and_nc",   COND_MASK_P1 | (0x1<<18), 0 },

    { "if_nc_and_z",    COND_MASK_P1 | (0x2<<18), 0 },
    { "if_z_and_nc",    COND_MASK_P1 | (0x2<<18), 0 },

    { "if_ae",     COND_MASK_P1 | (0x3<<18), 0 },
    { "if_nc",     COND_MASK_P1 | (0x3<<18), 0 },

    { "if_c_and_nz",    COND_MASK_P1 | (0x4<<18), 0 },
    { "if_nz_and_c",    COND_MASK_P1 | (0x4<<18), 0 },

    { "if_ne",     COND_MASK_P1 | (0x5<<18), 0 },
    { "if_nz",     COND_MASK_P1 | (0x5<<18), 0 },

    { "if_c_ne_z", COND_MASK_P1 | (0x6<<18), 0 },
    { "if_z_ne_c", COND_MASK_P1 | (0x6<<18), 0 },

    { "if_nc_or_nz", COND_MASK_P1 | (0x7<<18), 0 },
    { "if_nz_or_nc", COND_MASK_P1 | (0x7<<18), 0 },

    { "if_c_and_z", COND_MASK_P1 | (0x8<<18), 0 },
    { "if_z_and_c", COND_MASK_P1 | (0x8<<18), 0 },

    { "if_c_eq_z", COND_MASK_P1 | (0x9<<18), 0 },
    { "if_z_eq_c", COND_MASK_P1 | (0x9<<18), 0 },

    { "if_e",      COND_MASK_P1 | (0xa<<18), 0 },
    { "if_z",      COND_MASK_P1 | (0xa<<18), 0 },

    { "if_nc_or_z", COND_MASK_P1 | (0xb<<18), 0 },
    { "if_z_or_nc", COND_MASK_P1 | (0xb<<18), 0 },

    { "if_b",      COND_MASK_P1 | (0xc<<18), 0 },
    { "if_c",      COND_MASK_P1 | (0xc<<18), 0 },

    { "if_c_or_nz", COND_MASK_P1 | (0xd<<18), 0 },
    { "if_nz_or_c", COND_MASK_P1 | (0xd<<18), 0 },

    { "if_be",     COND_MASK_P1 | (0xe<<18), 0 },
    { "if_c_or_z", COND_MASK_P1 | (0xe<<18), 0 },
    { "if_z_or_c", COND_MASK_P1 | (0xe<<18), 0 },


    { "wz", (1<<25), FLAG_WZ },
    { "wc", (1<<24), FLAG_WC },
    { "wr", (1<<23), FLAG_WR },
    { "nr", ~(1<<23), FLAG_NR },

    { NULL, 0 }
};

InstrModifier modifiers_p2[] = {
    { "_ret_",  COND_MASK_P2, 0 },
    { "if_always", COND_MASK_P2 | (0xf<<28), 0 },

    { "if_a",           COND_MASK_P2 | (0x1<<28), 0 },
    { "if_gt",           COND_MASK_P2 | (0x1<<28), 0 },
    { "if_nc_and_nz",   COND_MASK_P2 | (0x1<<28), 0 },
    { "if_nz_and_nc",   COND_MASK_P2 | (0x1<<28), 0 },
    { "if_00",           COND_MASK_P2 | (0x1<<28), 0 },

    { "if_nc_and_z",    COND_MASK_P2 | (0x2<<28), 0 },
    { "if_z_and_nc",    COND_MASK_P2 | (0x2<<28), 0 },
    { "if_01",           COND_MASK_P2 | (0x2<<28), 0 },

    { "if_ae",     COND_MASK_P2 | (0x3<<28), 0 },
    { "if_ge",     COND_MASK_P2 | (0x3<<28), 0 },
    { "if_nc",     COND_MASK_P2 | (0x3<<28), 0 },
    { "if_0x",           COND_MASK_P2 | (0x3<<28), 0 },

    { "if_c_and_nz",    COND_MASK_P2 | (0x4<<28), 0 },
    { "if_nz_and_c",    COND_MASK_P2 | (0x4<<28), 0 },
    { "if_10",    COND_MASK_P2 | (0x4<<28), 0 },

    { "if_ne",     COND_MASK_P2 | (0x5<<28), 0 },
    { "if_nz",     COND_MASK_P2 | (0x5<<28), 0 },
    { "if_x0",     COND_MASK_P2 | (0x5<<28), 0 },

    { "if_c_ne_z", COND_MASK_P2 | (0x6<<28), 0 },
    { "if_z_ne_c", COND_MASK_P2 | (0x6<<28), 0 },
    { "if_diff", COND_MASK_P2 | (0x6<<28), 0 },

    { "if_nc_or_nz", COND_MASK_P2 | (0x7<<28), 0 },
    { "if_nz_or_nc", COND_MASK_P2 | (0x7<<28), 0 },
    { "if_not_11", COND_MASK_P2 | (0x7<<28), 0 },

    { "if_c_and_z", COND_MASK_P2 | (0x8<<28), 0 },
    { "if_z_and_c", COND_MASK_P2 | (0x8<<28), 0 },
    { "if_11", COND_MASK_P2 | (0x8<<28), 0 },

    { "if_c_eq_z", COND_MASK_P2 | (0x9<<28), 0 },
    { "if_z_eq_c", COND_MASK_P2 | (0x9<<28), 0 },
    { "if_same", COND_MASK_P2 | (0x9<<28), 0 },

    { "if_e",      COND_MASK_P2 | (0xa<<28), 0 },
    { "if_z",      COND_MASK_P2 | (0xa<<28), 0 },
    { "if_1x",      COND_MASK_P2 | (0xa<<28), 0 },

    { "if_nc_or_z", COND_MASK_P2 | (0xb<<28), 0 },
    { "if_z_or_nc", COND_MASK_P2 | (0xb<<28), 0 },
    { "if_not_10", COND_MASK_P2 | (0xb<<28), 0 },

    { "if_b",      COND_MASK_P2 | (0xc<<28), 0 },
    { "if_c",      COND_MASK_P2 | (0xc<<28), 0 },
    { "if_lt",      COND_MASK_P2 | (0xc<<28), 0 },
    { "if_x1",      COND_MASK_P2 | (0xc<<28), 0 },

    { "if_c_or_nz", COND_MASK_P2 | (0xd<<28), 0 },
    { "if_nz_or_c", COND_MASK_P2 | (0xd<<28), 0 },
    { "if_not_01", COND_MASK_P2 | (0xd<<28), 0 },

    { "if_be",     COND_MASK_P2 | (0xe<<28), 0 },
    { "if_le",     COND_MASK_P2 | (0xe<<28), 0 },
    { "if_c_or_z", COND_MASK_P2 | (0xe<<28), 0 },
    { "if_z_or_c", COND_MASK_P2 | (0xe<<28), 0 },
    { "if_not_00", COND_MASK_P2 | (0xe<<28), 0 },


    { "wz", (1<<19), FLAG_WZ },
    { "wc", (2<<19), FLAG_WC },
    { "wcz", (3<<19), FLAG_WCZ },
    
    { "andz", (1<<19), FLAG_ANDZ },
    { "andc", (2<<19), FLAG_ANDC },
    { "xorz", (1<<19), FLAG_XORZ },
    { "xorc", (2<<19), FLAG_XORC },
    { "orz", (1<<19), FLAG_ORZ },
    { "orc", (2<<19), FLAG_ORC },

    { NULL, 0 }
};


static void
InitPasm(int flags)
{
    HwReg *hwreg;
    InstrModifier *modifiers;
    int i;

    if (gl_p2) {
        instr = instr_p2;
        hwreg = hwreg_p2;
        modifiers = modifiers_p2;
    } else {
        instr = instr_p1;
        hwreg = hwreg_p1;
        modifiers = modifiers_p1;
    }
    
    pasmWords.flags |= SYMTAB_FLAG_NOCASE;
    
    /* add hardware registers */
    for (i = 0; hwreg[i].name != NULL; i++) {
        AddSymbol(&spinCommonReservedWords, hwreg[i].name, SYM_HWREG, (void *)&hwreg[i], NULL);
        AddSymbol(&basicReservedWords, hwreg[i].name, SYM_HWREG, (void *)&hwreg[i], NULL);
        AddSymbol(&cReservedWords, hwreg[i].cname, SYM_HWREG, (void *)&hwreg[i], NULL);
        AddSymbol(&pasmWords, hwreg[i].name, SYM_HWREG, (void *)&hwreg[i], NULL);
    }

    /* add instructions */
    for (i = 0; instr[i].name != NULL; i++) {
        AddSymbol(&pasmWords, instr[i].name, SYM_INSTR, (void *)&instr[i], NULL);
    }

    /* instruction modifiers */
    for (i = 0; modifiers[i].name != NULL; i++) {
        AddSymbol(&pasmWords, modifiers[i].name, SYM_INSTRMODIFIER, (void *)&modifiers[i], NULL);
    }

}


/*
 * "canonicalize" an identifier, to make sure it
 * does not conflict with any C reserved words
 * also checks for spaces and other illegal characters within the name
 */
bool
Is_C_Reserved(const char *name)
{
    Symbol *s;
    const char *ptr;
    s = FindSymbol(&ckeywords, name);
    if (s && !strcmp(name, s->our_name))
        return true;
    if (strlen(name) < 3)
        return false;
    for (ptr = name; *ptr; ptr++) {
        if (!isIdentifierChar(*ptr)) return true;
    }
    if (ptr[-2] == '_' && ptr[-1] == 't')
        return true;

    return false;
}

void
NormalizeIdentifier(char *idstr)
{
    char *ptr = idstr;
    int needCap = 1;
    while (*ptr) {
        if (needCap && isalpha(*ptr)) {
            *ptr = toupper(*ptr);
            needCap = 0;
        } else if (!isIdentifierChar(*ptr)) {
            *ptr = '_';
        } else {
            *ptr = tolower(*ptr);
        }
        ptr++;
    }
}

int saved_spinyychar;

int
spinyylex(SPINYYSTYPE *yval)
{
    int c;
    
    saved_spinyychar = c = getSpinToken(current->Lptr, yval);
    if (c == SP_EOF)
        return 0;
    return c;
}


static int
parseBasicIdentifier(LexStream *L, AST **ast_ptr)
{
    int c;
    struct flexbuf fb;
    Symbol *sym;
    AST *ast = NULL;
    char *idstr;
    bool forceLower = 0; // !gl_caseSensitive;
    
    flexbuf_init(&fb, INCSTR);
    c = lexgetc(L);
    while (isIdentifierChar(c)) {
        if (forceLower) {
            c = tolower(c);
        }
        flexbuf_addchar(&fb, c);
        c = lexgetc(L);
    }
    // allow trailing $, %, #
    if (c == '$' || c == '#' || c == '%' || c == '!') {
        flexbuf_addchar(&fb, c);
        c = lexgetc(L);
    }
    // add a trailing 0, and make sure there is room for an extra
    // character in case the name mangling needs it
    flexbuf_addchar(&fb, '\0');
    flexbuf_addchar(&fb, '\0');
    idstr = flexbuf_get(&fb);
    lexungetc(L, c);  

    // check for ASM
    /* check for reserved words */
    if (InDatBlock(L)) {
        sym = FindSymbol(&pasmWords, idstr);
        if (sym) {
            free(idstr);
            if (sym->kind == SYM_INSTR) {
                ast = NewAST(AST_INSTR, NULL, NULL);
                ast->d.ptr = sym->val;
                if (comment_chain) {
                    ast = AddToList(comment_chain, ast);
                    comment_chain = NULL;
                }
                *ast_ptr = ast;
                return BAS_INSTR;
            }
            if (sym->kind == SYM_INSTRMODIFIER) {
                ast = NewAST(AST_INSTRMODIFIER, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return BAS_INSTRMODIFIER;
            }
            if (sym->kind == SYM_HWREG) {
                ast = NewAST(AST_HWREG, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return BAS_HWREG;
            }
            fprintf(stderr, "Internal error: Unknown pasm symbol type %d\n", sym->kind);
        }
    }

    // check for keywords
    if (InDatBlock(L)) {
        sym = FindSymbol(&basicAsmReservedWords, idstr);
    } else {
        sym = FindSymbol(&basicReservedWords, idstr);
    }
    if (sym != NULL) {
      if (sym->kind == SYM_RESERVED) {
        int peekc = lexpeekc(L);
	c = INTVAL(sym);
        /* check for special handling */
        switch(c) {
        case BAS_ASM:
            if (InDatBlock(L)) {
                // leave the inline assembly
                L->block_type = BLOCK_PUB;
            } else {
                L->block_type = BLOCK_ASM;
            }
            break;
        case BAS_AND:
            if (peekc == '=') {
                (void)lexgetc(L);
                c = BAS_AND_ASSIGN;
            }
            break;
        case BAS_MOD:
            if (peekc == '=') {
                (void)lexgetc(L);
                c = BAS_MOD_ASSIGN;
            }
            break;
        case BAS_OR:
            if (peekc == '=') {
                (void)lexgetc(L);
                c = BAS_OR_ASSIGN;
            }
            break;
        case BAS_XOR:
            if (peekc == '=') {
                (void)lexgetc(L);
                c = BAS_XOR_ASSIGN;
            }
            break;
        default:
            break;
        }
	if (!ast) {
	  ast = GetComments();
	  *ast_ptr = ast;
	  return c;
	}
      }
    }
    // check for a defined class or similar type
    if (current) {
        sym = LookupSymbolInTable(currentTypes, idstr);
        if (sym) {
            if (sym->kind == SYM_VARIABLE) {
                ast = (AST *)sym->val;
                // check for an abstract object declaration
                if (ast->left && ast->left->kind == AST_OBJDECL && ast->left->left->kind == AST_IDENTIFIER && !strcmp(idstr, ast->left->left->d.string)) {
                    *ast_ptr = ast;
                    last_ast = AstIdentifier(idstr);
                    return BAS_TYPENAME;
                }
            } else if (sym->kind == SYM_TYPEDEF) {
                ast = (AST *)sym->val;
                *ast_ptr = ast;
                last_ast = AstIdentifier(idstr);
                return BAS_TYPENAME;
            } else if (sym->kind == SYM_REDEF) {
                last_ast = AstIdentifier(idstr);
                ast = NewAST(AST_LOCAL_IDENTIFIER, (AST *)sym->val, last_ast);
                *ast_ptr = ast;
                return BAS_IDENTIFIER;
            }
        }
    }
    // it's an identifier
    ast = AstIdentifier(idstr);
    *ast_ptr = last_ast = ast;

    // if the next character is ':' then it may be a label
    {
        c = lexgetc(L);
        if (c == ':') {
            int c2, c3;
            c2 = lexgetc(L);
            if (c2 == ' ' || c2 == '\t') {
                do {
                    c3 = lexgetc(L);
                } while (c3 == ' ' || c3 == '\t');
                lexungetc(L, c3);
                lexungetc(L, ' ');
                c2 = c3;
            } else {
                lexungetc(L, c2);
            }
            if (c2 == '\n') {
                return BAS_LABEL;
            }
        }
        lexungetc(L, c);
    }
    return BAS_IDENTIFIER;
}

static int
getBasicToken(LexStream *L, AST **ast_ptr)
{
    int c;
    AST *ast = NULL;
    
    c = skipSpace(L, &ast, LANG_BASIC_FBASIC);
again:
    // printf("c=%d L->linecounter=%d\n", c, L->lineCounter);
    if (c >= 127) {
        *ast_ptr = ast;
        return c;
    }
    // special case: '&' followed immediately by a letter is potentially
    // a literal like &H0123 for hex 0123

    if (c == '&') {
        int c2 = lexgetc(L);
        ast = NewAST(AST_INTEGER, NULL, NULL);
        if (c2 == 'h' || c2 == 'H' || c2 == 'x' || c2 == 'X') {
            parseNumber(L, 16, &ast->d.ival);
            c = BAS_INTEGER;
        } else if (c2 == 'b' || c2 == 'B') {
            parseNumber(L, 2, &ast->d.ival);
            c = BAS_INTEGER;
        } else if (c2 == '=') {
            c = BAS_AND_ASSIGN;
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '.') {
        int c2 = lexgetc(L);
        lexungetc(L, c2);
        if (safe_isdigit(c2)) {
            lexungetc(L, c);
            c = '0';
            goto again;
        }
    } else if (safe_isdigit(c)) {
        int is_label = 0;
        lexungetc(L,c);
        if (L->firstNonBlank == 0) {
            is_label = 1;
        }
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = parseNumber(L, 10, &ast->d.ival);
        if (c == SP_FLOATNUM) {
            ast->kind = AST_FLOAT;
	    c = BAS_FLOAT;
	} else if (ast->d.ival == 0 && !is_label) {
            // check for hex or binary prefixes like 0x or 0h
            int c2;
            c2 = lexgetc(L);
            if (c2 == 'h' || c2 == 'H' || c2 == 'x' || c2 == 'X') {
                c = parseNumber(L, 16, &ast->d.ival);
            } else if (c2 == 'b' || c2 == 'B') {
                c = parseNumber(L, 2, &ast->d.ival);
            } else {
                lexungetc(L, c2);
            }
            c = BAS_INTEGER;
	} else {
            if (is_label) {
                ast = IntegerLabel(ast);
                c = BAS_LABEL;
                L->firstNonBlank = L->colCounter;
            } else {
                c = BAS_INTEGER;
            }
        }
    } else if (c == '$') {
        int c2 = lexpeekc(L);
        if ( (c2 >= '0' && c2 <= '9') || (c2 >= 'a' && c2 <= 'f') || (c2 >= 'A' && c2 <= 'F') ) {
            ast = NewAST(AST_INTEGER, NULL, NULL);
            parseNumber(L, 16, &ast->d.ival);
            c = BAS_INTEGER;
        }
    } else if (c == '%') {
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = lexgetc(L);
        if (c == '%') {
            parseNumber(L, 4, &ast->d.ival);
        } else if (c >= '0' && c <= '9') {
            lexungetc(L, c);
            parseNumber(L, 2, &ast->d.ival);
        }
        c = BAS_INTEGER;
    } else if (isIdentifierStart(c)) {
        lexungetc(L, c);
        c = parseBasicIdentifier(L, &ast);
        if (c == BAS_DATA) {
            parseLineAsString(L, &ast);
        }
    } else if (c == '"') {
        parseString(L, &ast);
	c = BAS_STRING;
    } else if (c == '<' || c == '>' || c == '=') {
        int c2 = lexgetc(L);
        if (c2 == '<' || c2 == '>' || c2 == '=') {
            c2 += c;
            if (c2 == ('<' + '>')) {
                // note: '<' + '>' == '=' + '=', so beware!
                if (c != '=') c = BAS_NE;
            } else if (c2 == '<' + '<') {
                c = BAS_SHL;
            } else if (c2 == '=' + '<') {
                c = BAS_LE;
            } else if (c2 == '=' + '>') {
                c = BAS_GE;
            } else if (c2 == '>' + '>') {
                c = BAS_SHR;
            } else {
                SYNTAX_ERROR("internal lexer error");
            }
        } else {
            lexungetc(L, c2);
        }
    } else if (lexpeekc(L) == '=' && strchr("+-*/", c) != 0) {
        (void)lexgetc(L); // get the =
        switch (c) {
        case '+':
            c = BAS_ADD_ASSIGN;
            break;
        case '-':
            c = BAS_SUB_ASSIGN;
            break;
        case '/':
            c = BAS_DIV_ASSIGN;
            break;
        case '*':
            c = BAS_MUL_ASSIGN;
            break;
        default:
            SYNTAX_ERROR("internal lexer error");
            break;
        }
    }
    L->firstNonBlank = L->colCounter;
    *ast_ptr = last_ast = ast;
    return c;
}

int saved_basicyychar;

int
basicyylex(BASICYYSTYPE *yval)
{
    int c;
    saved_basicyychar = c = getBasicToken(current->Lptr, yval);
    if (c == BAS_EOF || c == EOF)
        return 0;
    return c;
}

static int
ParseCAttribute(LexStream *L, AST **ast_ptr)
{
    int c;
    int balance = 0;
    struct flexbuf fb;
    AST *ast = NewAST(AST_ANNOTATION, NULL, NULL);
    
    flexbuf_init(&fb, 32);
    do {
        c = lexgetc(L);
        if (c < 0) break;
        if (c == '(') {
            balance++;
        } else if (c == ')') {
            --balance;
        }
        flexbuf_addchar(&fb, c);
    } while (balance > 0);
    flexbuf_addchar(&fb, 0);
    ast->d.string = flexbuf_get(&fb);
    *ast_ptr = ast;
    return C_ATTRIBUTE;
}

static int
parseCIdentifier(LexStream *L, AST **ast_ptr, const char *prefix)
{
    int c;
    struct flexbuf fb;
    Symbol *sym;
    AST *ast = NULL;
    char *idstr;
    
    flexbuf_init(&fb, INCSTR);
    if (prefix) {
        flexbuf_addmem(&fb, prefix, strlen(prefix));
        if (gl_gas_dat) {
            flexbuf_addchar(&fb, '.');
        } else {
            flexbuf_addchar(&fb, ':');
        }
    }
    c = lexgetc(L);
    while (isIdentifierChar(c)) {
        flexbuf_addchar(&fb, c);
        c = lexgetc(L);
    }
    // add a trailing 0, and make sure there is room for an extra
    // character in case the name mangling needs it
    flexbuf_addchar(&fb, '\0');
    flexbuf_addchar(&fb, '\0');
    idstr = flexbuf_get(&fb);
    lexungetc(L, c);  

    // check for ASM
    /* check for reserved words */
    if (InDatBlock(L)) {
        sym = FindSymbol(&pasmWords, idstr);
        if (sym) {
            free(idstr);
            if (sym->kind == SYM_INSTR) {
                ast = NewAST(AST_INSTR, NULL, NULL);
                ast->d.ptr = sym->val;
                if (comment_chain) {
                    ast = AddToList(comment_chain, ast);
                    comment_chain = NULL;
                }
                *ast_ptr = ast;
                return C_INSTR;
            }
            if (sym->kind == SYM_INSTRMODIFIER) {
                ast = NewAST(AST_INSTRMODIFIER, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return C_INSTRMODIFIER;
            }
            if (sym->kind == SYM_HWREG) {
                ast = NewAST(AST_HWREG, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return C_HWREG;
            }
            fprintf(stderr, "Internal error: Unknown pasm symbol type %d\n", sym->kind);
        }
    }

    // check for keywords
    if (InDatBlock(L)) {
        int i;
        int len = strlen(idstr)+1;
        char *lowerSym = (char *)alloca(len);
        for (i = 0; i < len; i++) {
            lowerSym[i] = tolower(idstr[i]);
        }
        lowerSym[i] = 0;
        sym = FindSymbol(&cAsmReservedWords, lowerSym);
    } else {
        sym = NULL;
    }
    if (!sym) {
        sym = FindSymbol(&cReservedWords, idstr);
    }
    if (!sym && L->language == LANG_CFAMILY_CPP) {
        sym = FindSymbol(&cppReservedWords, idstr);
    }
    if (sym != NULL) {
      if (sym->kind == SYM_RESERVED) {
	c = INTVAL(sym);
        /* check for special handling */
        switch(c) {
        case C_ASM:
        case C_PASM:
            if (InDatBlock(L)) {
                // leave the inline assembly
                L->block_type = BLOCK_PUB;
            } else {
                L->block_type = (c == C_PASM) ? BLOCK_PASM : BLOCK_ASM;
            }
            break;
        case C_ATTRIBUTE:
            return ParseCAttribute(L, ast_ptr);
        default:
            break;
        }
	if (!ast) {
	  ast = GetComments();
	  *ast_ptr = ast;
	  return c;
	}
      } else {
          ast = NewAST(AST_SYMBOL, NULL, NULL);
          ast->d.ptr = (void *)sym;
          *ast_ptr = last_ast = ast;
          return C_IDENTIFIER;
      }
    }
    // check for a defined class or similar type
    if (current) {
        sym = LookupSymbolInTable(currentTypes, idstr);
        if (sym) {
            if (sym->kind == SYM_TYPEDEF && allow_type_names) {
                last_ast = ast = AstIdentifier(idstr);
                *ast_ptr = ast;
                return C_TYPE_NAME;
            }
            if (sym->kind == SYM_REDEF) {
                last_ast = AstIdentifier(idstr);
                ast = NewAST(AST_LOCAL_IDENTIFIER, (AST *)sym->val, last_ast);
                *ast_ptr = ast;
                return C_IDENTIFIER;
            }
        }
    }
    // it's an identifier
    ast = AstIdentifier(idstr);
    *ast_ptr = last_ast = ast;
    return C_IDENTIFIER;
}

static int
getCChar(LexStream *L, int isWide)
{
    int c, c2;
    c = lexgetc(L);
    if (c == '\\') {
        c = getEscapedChar(L);
    }
    c2  = lexgetc(L);
    if (c2 != '\'') {
        SYNTAX_ERROR("expected closing \' ");
    }
    if (!isWide) {
        if (255 < (unsigned)c) {
            SYNTAX_ERROR("value does not fit in char");
            c &= 255;
        }
    }
    return c;
}

int saved_cgramyychar;

static int
getCToken(LexStream *L, AST **ast_ptr)
{
    int c, c2;
    AST *ast = NULL;
    int at_startofline = (L->eoln == 1);
    
    c = skipSpace(L, &ast, LANG_CFAMILY_C);

    // printf("c=%d L->linecounter=%d\n", c, L->lineCounter);
    if (c >= 127) {
        *ast_ptr = ast;
        return c;
    }
parse_number:        
    if (safe_isdigit(c)) {
        lexungetc(L,c);
        ast = NewAST(AST_INTEGER, NULL, NULL);
        if (c == '0') {
            // 0ddd is an octal number in C
            c = parseNumber(L, 8, &ast->d.ival);
        } else {
            c = parseNumber(L, 10, &ast->d.ival);
        }
        if (c == SP_FLOATNUM) {
            int c2;
            ast->kind = AST_FLOAT;
	    c = C_CONSTANT;
            c2 = lexpeekc(L);
            if (c2 == 'f' || c2 == 'F') {
                lexgetc(L);
            }
	} else {
            int c2 = lexgetc(L);
            
            if (ast->d.ival == 0) {
                // check for hex or binary prefixes like 0x or 0h
                if (c2 == 'x' || c2 == 'X') {
                    c = parseNumber(L, 16, &ast->d.ival);
                    c2 = lexgetc(L);
                } else if (c2 == 'b' || c2 == 'B') {
                    c = parseNumber(L, 2, &ast->d.ival);
                    c2 = lexgetc(L);
                }
            }
            if (c2 == 'U' || c2 == 'u') {
                // unsigned flag; add in an unsigned type
                ast->left = ast_type_unsigned_long;
                c2 = lexgetc(L);
            }
            if (c2 == 'L' || c2 == 'l') {
                // long constant flag, ignore for now
                c2 = lexgetc(L);
            }
            lexungetc(L, c2);
            c = C_CONSTANT;
        }
    } else if (isIdentifierStart(c)) {
        if (c == 'L') {
            int c2 = lexgetc(L);
            if (c2 == '\'') {
                c = getCChar(L, 1);
                ast = AstInteger(c);
                c = C_CONSTANT;
            } else {
                lexungetc(L, c2);
            }
        }
        if (c != C_CONSTANT) {
            lexungetc(L, c);
            c = parseCIdentifier(L, &ast, NULL);
        }
        if (InDatBlock(L) && c == C_IDENTIFIER) {
            if (at_startofline) {
                L->lastGlobal = ast->d.string;
            }
        }
    } else if (c == '"') {
        parseCString(L, &ast);
	c = C_STRING_LITERAL;
    } else if (c == '}' && InDatBlock(L)) {
        L->block_type = BLOCK_PUB;
    } else if (c == '\'') {
        c = getCChar(L, 0);
        ast = AstInteger(c);
        c = C_CONSTANT;
    } else if (c == '.') {
        c2 = lexgetc(L);
        if (gl_p2 && InDatBlock(L) && (isIdentifierStart(c2) || safe_isdigit(c2)) ) {
            lexungetc(L, c2);
            c = parseSpinIdentifier(L, &ast, L->lastGlobal ? L->lastGlobal : "");
        } else {
            if (safe_isdigit(c2)) {
                // check for .1234 <=> 0.1234
                lexungetc(L, c2);
                lexungetc(L, c);
                c = '0';
                goto parse_number;
            }
            if (c2 == '.') {
                c2 = lexgetc(L);
                if (c2 == '.') {
                    c = C_ELLIPSIS;
                } else {
                    lexungetc(L, c2);
                    lexungetc(L, '.');
                }
            } else {
                lexungetc(L, c2);
            }
        }
    } else if (c == ':' && !gl_p2 && isIdentifierStart(lexpeekc(L)) && InDatBlock(L)) {
        c = parseSpinIdentifier(L, &ast, L->lastGlobal ? L->lastGlobal : "");
    } else if (c == '<') {
        c2 = lexgetc(L);
        if (c2 == '=') {
            c = C_LE_OP;
        } else if (c2 == '<') {
            c2 = lexgetc(L);
            if (c2 == '=') {
                c = C_LEFT_ASSIGN;
            } else {
                lexungetc(L, c2);
                c = C_LEFT_OP;
            }
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '>') {
        c2 = lexgetc(L);
        if (c2 == '=') {
            c = C_GE_OP;
        } else if (c2 == '>') {
            c2 = lexgetc(L);
            if (c2 == '=') {
                c = C_RIGHT_ASSIGN;
            } else {
                lexungetc(L, c2);
                c = C_RIGHT_OP;
            }
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '-') {
        c2 = lexgetc(L);
        if (c2 == '>') {
            c = C_PTR_OP;
        } else if (c2 == '=') {
            c = C_SUB_ASSIGN;
        } else if (c2 == '-') {
            c = C_DEC_OP;
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '+') {
        c2 = lexgetc(L);
        if (c2 == '=') {
            c = C_ADD_ASSIGN;
        } else if (c2 == '+') {
            c = C_INC_OP;
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '&') {
        c2 = lexgetc(L);
        if (c2 == '=') {
            c = C_AND_ASSIGN;
        } else if (c2 == '&') {
            c = C_AND_OP;
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '|') {
        c2 = lexgetc(L);
        if (c2 == '=') {
            c = C_OR_ASSIGN;
        } else if (c2 == '|') {
            c = C_OR_OP;
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '^') {
        c2 = lexgetc(L);
        if (c2 == '=') {
            c = C_XOR_ASSIGN;
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '*') {
        c2 = lexgetc(L);
        if (c2 == '=') {
            c = C_MUL_ASSIGN;
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '/') {
        c2 = lexgetc(L);
        if (c2 == '=') {
            c = C_DIV_ASSIGN;
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '$') {
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = parseNumber(L, 16, &ast->d.ival);
        c = C_CONSTANT;
    } else if (c == '%') {
        c2 = lexgetc(L);
        if (InPasmBlock(L)) {
            ast = NewAST(AST_INTEGER, NULL, NULL);
            if (c2 == '%') {
                c = parseNumber(L, 4, &ast->d.ival);
            } else {
                lexungetc(L, c2);
                c = parseNumber(L, 2, &ast->d.ival);
            }
            if (c == SP_FLOATNUM) {
                ast->kind = AST_FLOAT;
            }
            c = C_CONSTANT;
        } else if (c2 == '=') {
            c = C_MOD_ASSIGN;
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '!') {
        c2 = lexgetc(L);
        if (c2 == '=') {
            c = C_NE_OP;
        } else {
            lexungetc(L, c2);
        }
    } else if (c == '=') {
        c2 = lexgetc(L);
        if (c2 == '=') {
            c = C_EQ_OP;
        } else {
            lexungetc(L, c2);
        }
    }
    *ast_ptr = ast;
//    printf("Ctoken == %d\n", c);
    return c;
}

int
cgramyylex(CGRAMYYSTYPE *yval)
{
    int c;
    saved_cgramyychar = c = getCToken(current->Lptr, yval);
    if (c == EOF)
        return 0;
    return c;
}
