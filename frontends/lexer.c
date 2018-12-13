//
// Simple lexical analyzer for a language where indentation
// may be significant (Spin); also contains a lexer for BASIC
//
// Copyright (c) 2011-2018 Total Spectrum Software Inc.
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

SymbolTable spinReservedWords;
SymbolTable basicReservedWords;
SymbolTable cReservedWords;
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
void strToLex(LexStream *L, const char *s, const char *name)
{
    memset(L, 0, sizeof(*L));
    L->arg = L->ptr = (void *)s;
    L->getcf = strgetc;
    L->pendingLine = 1;
    L->fileName = name ? name : "<string>";
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
    if (c2 != 0) {
        if (c2 < 0) return EOF;
        /* FIXME: should convert to UTF-8 */
        return 0xff;
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
void fileToLex(LexStream *L, FILE *f, const char *name)
{
    int c1, c2;

    memset(L, 0, sizeof(*L));
    L->ptr = (void *)f;
    L->arg = NULL;
    L->getcf = filegetc;
    L->pendingLine = 1;
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
    if (c == 10) L->eoln = 0;
}

int
lexpeekc(LexStream *L)
{
    int c = lexgetc(L);
    lexungetc(L, c);
    return c;
}

//
// establish an indent level
// if the line is indented more than this, aa SP_INDENT will
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
    unsigned int c;
    int sawdigit = 0;
    int kind = SP_NUM;

    uval = 0;

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
        if (digit < base) {
            uval = base * uval + digit;
            sawdigit = 1;
        } else {
            break;
        }
    }
    if ( base == 10 && (c == '.' || c == 'e' || c == 'E') ) {
        /* potential floating point number */
        float f = (float)uval;
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
            if ( c != 'e' && c != 'E' && (c < '0' || c > '9')) {
                lexungetc(L, c);
                c = '.';
                goto donefloat;
            }
        }
        while (c >= '0' && c <= '9') {
            ff = ff + divby[counter]*(float)(c-'0');
            c = lexgetc(L);
            counter++;
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
    return L->in_block == SP_DAT || L->in_block == SP_ASM;
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
    bool forceLower = !gl_caseSensitive;
    
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
            if (sym->type == SYM_INSTR) {
                ast = NewAST(AST_INSTR, NULL, NULL);
                ast->d.ptr = sym->val;
                if (comment_chain) {
                    ast = AddToList(comment_chain, ast);
                    comment_chain = NULL;
                }
                *ast_ptr = ast;
                return SP_INSTR;
            }
            if (sym->type == SYM_INSTRMODIFIER) {
                ast = NewAST(AST_INSTRMODIFIER, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return SP_INSTRMODIFIER;
            }
            fprintf(stderr, "Internal error: Unknown pasm symbol type %d\n", sym->type);
        }
    }
    sym = FindSymbol(&spinReservedWords, idstr);
    if (sym != NULL) {
        if (sym->type == SYM_BUILTIN)
        {
            /* run any parse hooks */
            Builtin *b = (Builtin *)sym->val;
            if (b && b->parsehook) {
                (*b->parsehook)(b);
            }
            goto is_identifier;
        }
        if (sym->type == SYM_CONSTANT
            || sym->type == SYM_FLOAT_CONSTANT)
        {
            goto is_identifier;
        }
        free(idstr);
        if (sym->type == SYM_RESERVED) {
            c = INTVAL(sym);
            /* check for special handling */
            switch(c) {
            case SP_PUB:
            case SP_PRI:
            case SP_DAT:
            case SP_OBJ:
            case SP_VAR:
            case SP_CON:
                L->in_block = c;
                L->block_firstline = L->lineCounter;
                //EstablishIndent(L, 1);
                break;
	    case SP_ASM:
	        if (L->in_block == SP_ASM) {
		    fprintf(stderr, "WARNING: ignoring nested asm\n");
		} else {
		    L->save_block = L->in_block;
		}
		L->in_block = c;
		break;
	    case SP_ENDASM:
	        L->in_block = L->save_block;
	        break;
            case SP_IF:
            case SP_IFNOT:
            case SP_ELSE:
            case SP_ELSEIF:
            case SP_ELSEIFNOT:
            case SP_REPEAT:
            case SP_CASE:
                EstablishIndent(L, startColumn);
                break;
            case SP_LONG:
            case SP_BYTE:
            case SP_WORD:
                if (L->in_block == SP_ASM || L->in_block == SP_DAT) {
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
            *ast_ptr = ast;
            return c;
        }
        if (sym->type == SYM_HWREG) {
            ast = NewAST(AST_HWREG, NULL, NULL);
            ast->d.ptr = sym->val;
            *ast_ptr = ast;
            return SP_HWREG;
        }
        fprintf(stderr, "Internal error: Unknown symbol type %d\n", sym->type);
    }

is_identifier:
    ast = NewAST(AST_IDENTIFIER, NULL, NULL);

    if (gl_normalizeIdents) {
        NormalizeIdentifier(idstr);
    }

    ast->d.string = idstr;
    *ast_ptr = ast;
    return SP_IDENTIFIER;
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
    if (c < 0) {
        SYNTAX_ERROR("end of file inside string");
        return c;
    }
    switch (c) {
    case '0':
        c = 0; break;
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
    AST *ast;

    ast = NewAST(AST_STRING, NULL, NULL);
    flexbuf_init(&fb, INCSTR);
again:
    c = lexgetc(L);
    while (c != '"' && c > 0 && c < 256) {
        if (c == 10 || c == 13) {
            // newline in mid-string, this is bad news
            SYNTAX_ERROR("unterminated string");
            break;
        }
        if (c == '\\') {
            c = getEscapedChar(L);
            if (c < 0) break;
        }   
        flexbuf_addchar(&fb, c);
        c = lexgetc(L);
    }
    // handle "x" "y" as "xy"
    c = skipSpace(L, NULL, LANG_C);
    if (c == '"') {
        goto again;
    } else {
        lexungetc(L, c);
    }
    flexbuf_addchar(&fb, '\0');
    ast->d.string = flexbuf_get(&fb);
    *ast_ptr = ast;
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
    
    // look for #line directives
    if (c == '#' && L->colCounter == 1) {
        goto docomment;
    }
    if (language == LANG_BASIC && (c == 'r' || c == 'R') ) {
        int c2, c3, c4;
        c2 = lexgetc(L);
        if (c2 == 'e' || c2 == 'E') {
            c3 = lexgetc(L);
            if (c3 == 'm' || c3 == 'M') {
                c4 = lexgetc(L);
                if (!safe_isalpha(c4)) {
                    goto docomment;
                }
                lexungetc(L, c4);
            }
            lexungetc(L, c3);
        }
        lexungetc(L, c2);
        return c;
    }

    if (language == LANG_C) {
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
    if (!strncmp(commentLine, "#line ", 6)) {
        char *ptr = commentLine + 6;
        int lineno;
        lineno = strtol(ptr, &ptr, 10);
        if (lineno > 0) {
            while (*ptr == ' ') ptr++;
            L->lineCounter = lineno;
            L->fileName = getFileName(ptr);
        }
    } else if (!strncmp(commentLine, "#pragma ", 8)) {
        handlePragma(L, commentLine);
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
    if (language == LANG_SPIN) {
        return c == '{';
    }
    if (language == LANG_BASIC) {
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
    if (language == LANG_C) {
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
    if (language == LANG_SPIN) {
        return c == '}';
    }
    if (language == LANG_BASIC) {
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
    if (language == LANG_C) {
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

    if (language == LANG_BASIC) {
      eoln_token = BAS_EOLN;
      eof_token = BAS_EOF;
    } else if (language == LANG_C) {
        if (L->in_block == SP_ASM) {
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
        if (language == LANG_SPIN) {
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
	        if (doccomment && language == LANG_SPIN) {
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
            if (startcol > 1 && startline > L->block_firstline && (L->in_block == SP_PUB || L->in_block == SP_PRI)) {
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
        c = lexgetc(L);
        goto again;
    }

    if (L->eoln && language == LANG_SPIN && (L->in_block == SP_PUB || L->in_block == SP_PRI)) {
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
    if (language == LANG_BASIC) {
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


static char operator_chars[] = "-+*/|<>=!@~#^.?";

int
getSpinToken(LexStream *L, AST **ast_ptr)
{
//    int base = 10;
    int c;
    AST *ast = NULL;
    int at_startofline = (L->eoln == 1);
    int peekc;
    
    c = skipSpace(L, &ast, LANG_SPIN);

    if (c == EOF) {
      c = SP_EOF;
    }
//    printf("L->linecounter=%d\n", L->lineCounter);
    if (c >= 127) {
        *ast_ptr = last_ast = ast;
        return c;
    } else if (safe_isdigit(c)) {
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
        } else if (!gl_p2 && isIdentifierStart(peekc) && InDatBlock(L)) {
            lexungetc(L, peekc);
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
        for (i = 1; i < sizeof(op)-1; i++) {
            c = lexgetc(L);
            if (c >= 128 || c < 0 || strchr(operator_chars, c) == NULL) {
                lexungetc(L, c);
                break;
            }
            op[i] = c;
            op[i+1] = 0;
            sym = FindSymbol(&spinReservedWords, op);
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
    { "and", SP_AND },
    { "asm", SP_ASM },
    { "byte", SP_BYTE },

    { "case", SP_CASE },
    { "cognew", SP_COGNEW },
    { "coginit", SP_COGINIT },
    { "con", SP_CON },
    { "constant", SP_CONSTANT },

    { "dat", SP_DAT },

    { "else", SP_ELSE },
    { "elseif", SP_ELSEIF },
    { "elseifnot", SP_ELSEIFNOT },
    { "endasm", SP_ENDASM },

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
    { "org", SP_ORG },
    { "orgh", SP_ORGH },
    { "orgf", SP_ORGF },
    { "other", SP_OTHER },

    { "quit", SP_QUIT },
    { "pri", SP_PRI },
    { "pub", SP_PUB },
    { "repeat", SP_REPEAT },
    { "res", SP_RES },
    { "rev", SP_REV },
    { "result", SP_RESULT },
    { "return", SP_RETURN },
    { "rol", SP_ROTL },
    { "ror", SP_ROTR },
    { "round", SP_ROUND },

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
    { "+/", SP_UNSDIV },
    { "+//", SP_UNSMOD },
    { "*", '*' },
    { "**", SP_HIGHMULT },
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

struct reservedword basic_keywords[] = {
  { "abs", BAS_ABS },
  { "and", BAS_AND },
  { "any", BAS_ANY },
  { "as", BAS_AS },
  { "asc", BAS_ASC },
  { "asm", BAS_ASM },
  { "byte", BAS_BYTE },
  { "case", BAS_CASE },
  { "catch", BAS_CATCH },
  { "class", BAS_CLASS },
  { "close", BAS_CLOSE },
  { "const", BAS_CONST },
  { "continue", BAS_CONTINUE },
  { "cpu", BAS_CPU },
  { "declare", BAS_DECLARE },
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
  { "get", BAS_GET },
  { "goto", BAS_GOTO },
  { "if", BAS_IF },
  { "input", BAS_INPUT },
  { "integer", BAS_INTEGER_KW },
  { "let", BAS_LET },
  { "long", BAS_LONG },
  { "loop", BAS_LOOP },
  { "mod", BAS_MOD },
  { "new", BAS_NEW },
  { "next", BAS_NEXT },
  { "not", BAS_NOT },
  { "nil", BAS_NIL },
  { "open", BAS_OPEN },
  { "option", BAS_OPTION },
  { "or", BAS_OR },
  { "output", BAS_OUTPUT },
  { "pointer", BAS_POINTER },
  { "print", BAS_PRINT },
  { "program", BAS_PROGRAM },
  { "ptr", BAS_PTR },
  { "put", BAS_PUT },
  { "return", BAS_RETURN },
  { "select", BAS_SELECT },
  { "self", BAS_SELF },
  { "shared", BAS_SHARED },
  { "shl", BAS_SHL },
  { "short", BAS_SHORT },
  { "shr", BAS_SHR },
  { "single", BAS_SINGLE },
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
  { "_Bool", C_BOOL },
  { "break", C_BREAK },
  { "__builtin_alloca", C_BUILTIN_ALLOCA },
  { "__builtin_printf", C_BUILTIN_PRINTF },
  { "__builtin_propeller_rev", C_BUILTIN_REV },
  { "__builtin_va_arg", C_BUILTIN_VA_ARG },
  { "__builtin_va_start", C_BUILTIN_VA_START },
  { "case", C_CASE },
//  { "__class", BAS_CLASS },
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
  { "inline", C_INLINE },
  { "int", C_INT },
  { "long", C_LONG },
  { "__restrict", C_RESTRICT },
  { "return", C_RETURN },
  { "short", C_SHORT },
  { "signed", C_SIGNED },
  { "sizeof", C_SIZEOF },
  { "static", C_STATIC },
  { "struct", C_STRUCT },
  { "switch", C_SWITCH },
  { "typedef", C_TYPEDEF },
  { "union", C_UNION },
  { "unsigned", C_UNSIGNED },
  { "__using", C_USING },
  { "void", C_VOID },
  { "volatile", C_VOLATILE },
  { "while", C_WHILE },
};

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
// "spin name", numparameters, outputfunc, cname, gasname, extradata, parsehook

// the C version of the name
// an alternate form to use
Builtin builtinfuncs[] = {
    { "clkfreq", 0, defaultVariable, "CLKFREQ", NULL, 0, NULL },
    { "clkmode", 0, defaultVariable, "CLKMODE", NULL, 0, NULL },
    { "clkset", 2, defaultBuiltin, "clkset", "_clkset", 0, NULL },

    { "cogstop", 1, defaultBuiltin, "cogstop", "__builtin_propeller_cogstop", 0, NULL },
    { "cogid", 0, defaultBuiltin, "cogid", "__builtin_propeller_cogid", 0, NULL },

    { "locknew", 0, defaultBuiltin, "locknew", "__builtin_propeller_locknew", 0, lockhook },
    { "lockset", 1, defaultBuiltin, "lockset", "__builtin_propeller_lockset", 0, lockhook },
    { "lockclr", 1, defaultBuiltin, "lockclr", "__builtin_propeller_lockclr", 0, lockhook },
    { "lockret", 1, defaultBuiltin, "lockret", "__builtin_propeller_lockret", 0, lockhook },

    { "strsize", 1, str1Builtin, "strlen", NULL, 0, NULL },
    { "strcomp", 2, strcompBuiltin, "strcmp", NULL, 0, NULL },
    { "waitcnt", 1, defaultBuiltin, "waitcnt", "_waitcnt", 0, NULL },
    { "waitpeq", 3, waitpeqBuiltin, "waitpeq", "__builtin_propeller_waitpeq", 0, NULL },
    { "waitpne", 3, waitpeqBuiltin, "waitpne", "__builtin_propeller_waitpne", 0, NULL },

    { "reboot", 0, rebootBuiltin, "reboot", NULL, 0, NULL },

    { "longfill", 3, memFillBuiltin, "memset", NULL, 4, NULL },
    { "longmove", 3, memBuiltin, "memmove", NULL, 4, NULL },
    { "wordfill", 3, memFillBuiltin, "memset", NULL, 2, NULL },
    { "wordmove", 3, memBuiltin, "memmove", NULL, 2, NULL },
    { "bytefill", 3, memBuiltin, "memset", NULL, 1, NULL },
    { "bytemove", 3, memBuiltin, "memcpy", NULL, 1, NULL },

    { "getcnt", 0, defaultBuiltin, "getcnt", NULL, 0, NULL },

    // BASIC compiler builtins
    { "_basic_print_nl", 1, defaultBuiltin, "basic_print_nl", NULL, 0, NULL },
    { "_basic_print_string", 3, defaultBuiltin, "basic_print_string", NULL, 0, NULL },
    { "_basic_print_integer", 3, defaultBuiltin, "basic_print_integer", NULL, 0, NULL },
    { "_basic_print_unsigned", 3, defaultBuiltin, "basic_print_unsigned", NULL, 0, NULL },
    { "_basic_print_float", 3, defaultBuiltin, "basic_print_float", NULL, 0, NULL },
    { "_basic_print_fixed", 3, defaultBuiltin, "basic_print_fixed", NULL, 0, NULL },
    { "_basic_print_char", 2, defaultBuiltin, "basic_print_char", NULL, 0, NULL },
    { "_basic_put", 3, defaultBuiltin, "basic_put", NULL, 0, NULL },

    { "_string_cmp", 2, defaultBuiltin, "strcmp", NULL, 0, NULL },

    { "_string_concat", 2, defaultBuiltin, "_string_concat", NULL, 0, NULL },

    { "_fixed_div", 3, defaultBuiltin, "fixed_div", NULL, 0, NULL },
    { "_fixed_mul", 2, defaultBuiltin, "fixed_mul", NULL, 0, NULL },
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
    { "_Z_ne_c",    SYM_CONSTANT, 0x6 },
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
};

void InitPreprocessor()
{
    pp_init(&gl_pp);
    SetPreprocessorLanguage(LANG_SPIN);
}

void SetPreprocessorLanguage(int language)
{
    if (language == LANG_BASIC) {
        pp_setcomments(&gl_pp, "\'", "/'", "'/");
        pp_setlinedirective(&gl_pp, "/'#line %d %s'/");   
        //pp_setlinedirective(&gl_pp, "");   
    } else if (language == LANG_C) {
        pp_setcomments(&gl_pp, "//", "/*", "*/");
        pp_setlinedirective(&gl_pp, "/*#line %d %s*/");   
        //pp_setlinedirective(&gl_pp, "");   
    } else {
        pp_setcomments(&gl_pp, "\'", "{", "}");
        pp_setlinedirective(&gl_pp, "{#line %d %s}");   
    }
}

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

void
initSpinLexer(int flags)
{
    int i;

    /* add our reserved words */
    for (i = 0; i < N_ELEMENTS(init_spin_words); i++) {
        AddSymbol(&spinReservedWords, init_spin_words[i].name, SYM_RESERVED, (void *)init_spin_words[i].val);
    }
    for (i = 0; i < N_ELEMENTS(basic_keywords); i++) {
        AddSymbol(&basicReservedWords, basic_keywords[i].name, SYM_RESERVED, (void *)basic_keywords[i].val);
    }
    for (i = 0; i < N_ELEMENTS(c_keywords); i++) {
        AddSymbol(&cReservedWords, c_keywords[i].name, SYM_RESERVED, (void *)c_keywords[i].val);
    }
    
    if (gl_p2) {
        AddSymbol(&spinReservedWords, "alignl", SYM_RESERVED, (void *)SP_ALIGNL);
        AddSymbol(&spinReservedWords, "alignw", SYM_RESERVED, (void *)SP_ALIGNW);
    }
    /* add builtin functions */
    for (i = 0; i < N_ELEMENTS(builtinfuncs); i++) {
        AddSymbol(&spinReservedWords, NormalizedName(builtinfuncs[i].name), SYM_BUILTIN, (void *)&builtinfuncs[i]);
    }

    /* and builtin constants */
    if (gl_p2) {
        for (i = 0; i < N_ELEMENTS(p2_constants); i++) {
            AddSymbol(&spinReservedWords, NormalizedName(p2_constants[i].name), p2_constants[i].type, AstInteger(p2_constants[i].val));
        }
    } else {
        for (i = 0; i < N_ELEMENTS(p1_constants); i++) {
            AddSymbol(&spinReservedWords, NormalizedName(p1_constants[i].name), p1_constants[i].type, AstInteger(p1_constants[i].val));
        }
    }
    
    /* C keywords */
    for (i = 0; i < N_ELEMENTS(c_words); i++) {
        AddSymbol(&ckeywords, c_words[i], SYM_RESERVED, NULL);
    }

    /* add the PASM instructions */
    InitPasm(flags);
}

int
IsReservedWord(const char *name)
{
    return FindSymbol(&spinReservedWords, name) != 0;
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
    { "jmp",    0x5c000000, SRC_OPERAND_ONLY, OPC_JUMP, FLAG_P1_STD },
    { "jmpret", 0x5c800000, JMPRET_OPERANDS, OPC_JMPRET, FLAG_P1_STD },

    { "lockclr",0x0c400007, DST_OPERAND_ONLY, OPC_GENERIC_NR, FLAG_P1_STD },
    { "locknew",0x0cc00004, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P1_STD },
    { "lockret",0x0c400005, DST_OPERAND_ONLY, OPC_GENERIC_NR, FLAG_P1_STD },
    { "lockset",0x0c400006, DST_OPERAND_ONLY, OPC_GENERIC_NR, FLAG_P1_STD },

    { "max",    0x4c800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
    { "maxs",   0x44800000, TWO_OPERANDS, OPC_MAXS, FLAG_P1_STD },
    { "min",    0x48800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P1_STD },
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
    { "tjnz",   0xe8000000, JMPRET_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P1_STD },
    { "tjz",    0xec000000, JMPRET_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P1_STD },

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

    { "fge",    0x03000000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "fle",    0x03200000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "fges",   0x03400000, TWO_OPERANDS, OPC_MINS, FLAG_P2_STD },
    { "fles",   0x03600000, TWO_OPERANDS, OPC_MAXS, FLAG_P2_STD },

    { "sumc",   0x03800000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "sumnc",  0x03a00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "sumz",   0x03c00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "sumnz",  0x03e00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },

    { "testb",   0x04000000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_CZTEST },
    { "testbn",  0x04200000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_CZTEST },

    { "bitl",   0x04000000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bith",   0x04200000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitc",   0x04400000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitnc",  0x04600000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitz",   0x04800000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitnz",  0x04a00000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitrnd", 0x04c00000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },
    { "bitnot", 0x04e00000, TWO_OPERANDS, OPC_GENERIC, FLAG_WCZ },

    { "and",    0x05000000, TWO_OPERANDS, OPC_AND, FLAG_P2_STD },
    { "andn",   0x05200000, TWO_OPERANDS, OPC_ANDN, FLAG_P2_STD },
    { "or",     0x05400000, TWO_OPERANDS, OPC_OR, FLAG_P2_STD },
    { "xor",    0x05600000, TWO_OPERANDS, OPC_XOR, FLAG_P2_STD },
    { "muxc",   0x05800000, TWO_OPERANDS, OPC_MUXC, FLAG_P2_STD },
    { "muxnc",  0x05a00000, TWO_OPERANDS, OPC_MUXNC, FLAG_P2_STD },
    { "muxz",   0x05c00000, TWO_OPERANDS, OPC_MUXZ, FLAG_P2_STD },
    { "muxnz",  0x05e00000, TWO_OPERANDS, OPC_MUXNZ, FLAG_P2_STD },

    { "mov",    0x06000000, TWO_OPERANDS, OPC_MOV, FLAG_P2_STD },
    { "not",    0x06200000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },
    { "abs",    0x06400000, TWO_OPERANDS_OPTIONAL, OPC_ABS, FLAG_P2_STD },
    { "neg",    0x06600000, TWO_OPERANDS_OPTIONAL, OPC_NEG, FLAG_P2_STD },
    { "negc",   0x06800000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },
    { "negnc",  0x06a00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },
    { "negz",   0x06c00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },
    { "negnz",  0x06e00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },

    { "incmod", 0x07000000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "decmod", 0x07200000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "zerox",  0x07400000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
    { "signx",  0x07600000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },

    { "encod",  0x07800000, TWO_OPERANDS_OPTIONAL, OPC_ENCODE, FLAG_P2_STD },
    { "ones",   0x07a00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, FLAG_P2_STD },

    { "test",   0x07c00000, TWO_OPERANDS_OPTIONAL, OPC_TEST, FLAG_P2_STD|FLAG_WARN_NOTUSED },
    { "testn",  0x07e00000, TWO_OPERANDS, OPC_GENERIC_NR, FLAG_P2_STD|FLAG_WARN_NOTUSED },

    { "setnib", 0x08000000, THREE_OPERANDS_NIBBLE, OPC_GENERIC, 0 },
    { "getnib", 0x08400000, THREE_OPERANDS_NIBBLE, OPC_GENERIC, 0 },
    { "rolnib", 0x08800000, THREE_OPERANDS_NIBBLE, OPC_GENERIC, 0 },
    { "setbyte", 0x08c00000, THREE_OPERANDS_BYTE, OPC_GENERIC, 0 },
    { "getbyte", 0x08e00000, THREE_OPERANDS_BYTE, OPC_GENERIC, 0 },
    { "rolbyte", 0x09000000, THREE_OPERANDS_BYTE, OPC_GENERIC, 0 },
    { "setword", 0x09200000, THREE_OPERANDS_WORD, OPC_GENERIC, 0 },
    { "getword", 0x09300000, THREE_OPERANDS_WORD, OPC_GENERIC, 0 },
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

    { "decod",  0x09c00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, 0 },
    { "bmask",  0x09c80000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC, 0 },
    
    { "crcbit", 0x09d00000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "crcnib", 0x09d80000, TWO_OPERANDS, OPC_GENERIC, 0 },
    
    { "muxnits", 0x09e00000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "muxnibs", 0x09e80000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "muxq",   0x09f00000, TWO_OPERANDS, OPC_GENERIC, 0 },
    { "movbyts", 0x09f80000, TWO_OPERANDS, OPC_GENERIC, 0 },

    { "mul",    0x0a000000, TWO_OPERANDS, OPC_GENERIC, FLAG_WZ },
    { "muls",   0x0a100000, TWO_OPERANDS, OPC_GENERIC, FLAG_WZ },
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
    { "rdlut",  0x0aa00000, TWO_OPERANDS, OPC_GENERIC, FLAG_P2_STD },
  
    { "rdbyte", 0x0ac00000, P2_RDWR_OPERANDS, OPC_RDBYTE, FLAG_P2_STD },
    { "rdword", 0x0ae00000, P2_RDWR_OPERANDS, OPC_RDWORD, FLAG_P2_STD },
    { "rdlong", 0x0b000000, P2_RDWR_OPERANDS, OPC_RDLONG, FLAG_P2_STD },

    // some aliases from rdlong x, ++ptra
    { "popa",  0x0b00015f, TWO_OPERANDS_DEFZ, OPC_GENERIC, FLAG_P2_STD },
    { "popb",  0x0b0001df, TWO_OPERANDS_DEFZ, OPC_GENERIC, FLAG_P2_STD },
    
    { "calld",  0x0b200000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "reti0",  0x0b3bffff, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "reti1",  0x0b3bfff5, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "reti2",  0x0b3bfff3, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "reti3",  0x0b3bfff1, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "resi0",  0x0b3bfdff, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "resi1",  0x0b3be9f5, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "resi2",  0x0b3be5f3, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "resi3",  0x0b3be1f1, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },

    { "callpa", 0x0b400000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "callpb", 0x0b500000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },

    { "djz",    0x0b600000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "djnz",   0x0b680000, P2_TJZ_OPERANDS, OPC_DJNZ, 0 },
    { "djf",    0x0b700000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "djnf",   0x0b780000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "ijz",    0x0b800000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "ijnz",   0x0b880000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "tjz",    0x0b900000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "tjnz",   0x0b980000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "tjf",    0x0ba00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "tjnf",   0x0ba80000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "tjs",    0x0bb00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "tjns",   0x0bb80000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },
    { "tjv",    0x0bc00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH, 0 },

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

    { "setpat", 0x0bf00000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "wrpin",  0x0c000000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "akpin",  0x0c080200, SRC_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "wxpin",  0x0c100000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "wypin",  0x0c200000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "wrlut",  0x0c300000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },

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
    { "qmul",   0x0d000000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "qdiv",   0x0d100000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
    { "qfrac",  0x0d200000, P2_TWO_OPERANDS, OPC_GENERIC, 0 },
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

    { "getqx",  0x0d600018, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "getqy",  0x0d600019, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "getct",  0x0d60001a, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "getrnd", 0x0d60001b, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },

    { "setdacs",0x0d60001c, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setxfrq",0x0d60001d, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "getxacc",0x0d60001e, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "waitx",  0x0d60001f, P2_DST_CONST_OK, OPC_GENERIC, FLAG_P2_STD },
  
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
  
    { "setint1",0x0d600025, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setint2",0x0d600026, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setint3",0x0d600027, P2_DST_CONST_OK, OPC_GENERIC, 0 },

    { "setq",   0x0d600028, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },
    { "setq2",  0x0d600029, P2_DST_CONST_OK, OPC_GENERIC_NR, 0 },

    { "push",   0x0d60002a, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "pop",    0x0d60002b, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },

  // indirect jumps via register
  // normally the user will write "jmp x" and the assembler will
  // recognize if x is a register and rewrite it as "jmp.ind x"
    { "jmp.ind", 0x0d60002c, DST_OPERAND_ONLY, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "call.ind",0x0d60002d, DST_OPERAND_ONLY, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "ret",    0x0d64002d, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "calla.ind",0x0d60002e, DST_OPERAND_ONLY, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "reta",   0x0d64002e, NO_OPERANDS, OPC_RET, FLAG_P2_STD },
    { "callb.ind",0x0d60002f, DST_OPERAND_ONLY, OPC_GENERIC_BRANCH, FLAG_P2_STD },
    { "retb",   0x0d64002f, NO_OPERANDS, OPC_GENERIC_BRANCH, FLAG_P2_STD },

    { "jmprel", 0x0d600030, P2_DST_CONST_OK, OPC_GENERIC_BRANCH, 0 },
  
    { "skip",   0x0d600031, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "skipf",  0x0d600032, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "execf",  0x0d600033, P2_DST_CONST_OK, OPC_GENERIC, 0 },

    { "getptr", 0x0d600034, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "getbrk", 0x0d600035, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "brk",    0x0d600036, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setluts",0x0d600037, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "lutsoff",0x0d640037, NO_OPERANDS, OPC_GENERIC, 0 },
    { "lutson", 0x0d640237, NO_OPERANDS, OPC_GENERIC, 0 },
  
    { "setcy",  0x0d600038, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setci",  0x0d600039, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setcq",  0x0d60003a, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setcfrq",0x0d60003b, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setcmod",0x0d60003c, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setpix", 0x0d60003d, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "setpiv", 0x0d60003e, P2_DST_CONST_OK, OPC_GENERIC, 0 },
    { "cogatn", 0x0d60003f, P2_DST_CONST_OK, OPC_GENERIC, 0 },

    { "testp",  0x0d600040, P2_DST_CONST_OK, OPC_GENERIC, FLAG_P2_CZTEST },
    { "testpn", 0x0d600041, P2_DST_CONST_OK, OPC_GENERIC, FLAG_P2_CZTEST },

    { "dirl",   0x0d600040, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "dirh",   0x0d600041, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "dirc",   0x0d600042, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "dirnc",  0x0d600043, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "dirz",   0x0d600044, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "dirnz",  0x0d600045, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "dirrnd", 0x0d600046, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "dirnot", 0x0d600047, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
  
    { "outl",   0x0d600048, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "outh",   0x0d600049, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "outc",   0x0d60004a, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "outnc",  0x0d60004b, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "outz",   0x0d60004c, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "outnz",  0x0d60004d, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "outrnd", 0x0d60004e, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "outnot", 0x0d60004f, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },

    { "fltl",   0x0d600050, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "flth",   0x0d600051, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "fltc",   0x0d600052, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "fltnc",  0x0d600053, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "fltz",   0x0d600054, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "fltnz",  0x0d600055, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "fltrnd", 0x0d600056, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "fltnot", 0x0d600057, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
  
    { "drvl",   0x0d600058, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "drvh",   0x0d600059, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "drvc",   0x0d60005a, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "drvnc",  0x0d60005b, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "drvz",   0x0d60005c, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "drvnz",  0x0d60005d, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "drvrnd", 0x0d60005e, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },
    { "drvnot", 0x0d60005f, P2_DST_CONST_OK, OPC_GENERIC, FLAG_WCZ },

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
    { "wrc",    0x0d60006c, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "wrnc",   0x0d60006d, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "wrz",    0x0d60006e, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "wrnz",   0x0d60006f, DST_OPERAND_ONLY, OPC_GENERIC, 0 },
    { "modcz",  0x0d64006f, P2_MODCZ, OPC_GENERIC, FLAG_P2_STD | FLAG_WARN_NOTUSED },
    { "modc",   0x0d64006f, P2_MODCZ, OPC_GENERIC, FLAG_WC | FLAG_WARN_NOTUSED },
    { "modz",   0x0d64006f, P2_MODCZ, OPC_GENERIC, FLAG_WZ | FLAG_WARN_NOTUSED },
    { "rfvar",  0x0d600070, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    { "rfvars", 0x0d600071, DST_OPERAND_ONLY, OPC_GENERIC, FLAG_P2_STD },
    
  // long jumps
    { "jmp" ,   0x0d800000, P2_JUMP, OPC_JUMP, 0 },
    { "call",   0x0da00000, P2_JUMP, OPC_GENERIC_BRANCH, 0 },
    { "calla",  0x0dc00000, P2_JUMP, OPC_CALL, 0 },
    { "callb",  0x0de00000, P2_JUMP, OPC_GENERIC_BRANCH, 0 },

    { "calld.loc",  0x0e000000, P2_LOC, OPC_GENERIC_BRANCH, 0 },
    { "loc",    0x0e800000, P2_LOC, OPC_GENERIC, 0 },

    { "augs",   0x0f000000, P2_AUG, OPC_GENERIC, 0 },
    { "augd",   0x0f800000, P2_AUG, OPC_GENERIC, 0 },

    { NULL, 0, NO_OPERANDS, OPC_GENERIC},
};

HwReg hwreg_p1[] = {
    { "par", 0x1f0, "PAR" },
    { "cnt", 0x1f1, "CNT" },
    { "ina", 0x1f2, "INA" },
    { "inb", 0x1f3, "INB" },
    { "outa", 0x1f4, "OUTA" },
    { "outb", 0x1f5, "OUTB" },
    { "dira", 0x1f6, "DIRA" },
    { "dirb", 0x1f7, "DIRB" },
    { "ctra", 0x1f8, "CTRA" },
    { "ctrb", 0x1f9, "CTRB" },

    { "frqa", 0x1fa, "FRQA" },
    { "frqb", 0x1fb, "FRQB" },
    { "phsa", 0x1fc, "PHSA" },
    { "phsb", 0x1fd, "PHSB" },

    { "vcfg", 0x1fe, "VCFG" },
    { "vscl", 0x1ff, "VSCL" },

    { NULL, 0, NULL },
};

HwReg hwreg_p2[] = {
    { "ijmp3", 0x1f0, "IJMP3" },
    { "iret3", 0x1f1, "IRET3" },
    { "ijmp2", 0x1f2, "IJMP2" },
    { "iret2", 0x1f3, "IRET2" },
    { "ijmp1", 0x1f4, "IJMP1" },
    { "iret1", 0x1f5, "IRET1" },
    { "pa", 0x1f6, "PA" },
    { "pb", 0x1f7, "PB" },
    { "ptra", 0x1f8, "PTRA" },
    { "ptrb", 0x1f9, "PTRB" },

    { "dira", 0x1fa, "DIRA" },
    { "dirb", 0x1fb, "DIRB" },
    { "outa", 0x1fc, "OUTA" },
    { "outb", 0x1fd, "OUTB" },

    { "ina", 0x1fe, "INA" },
    { "inb", 0x1ff, "INB" },

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

    { "if_nc_and_z",    COND_MASK_P2 | (0x2<<28), 0 },
    { "if_z_and_nc",    COND_MASK_P2 | (0x2<<28), 0 },

    { "if_ae",     COND_MASK_P2 | (0x3<<28), 0 },
    { "if_ge",     COND_MASK_P2 | (0x3<<28), 0 },
    { "if_nc",     COND_MASK_P2 | (0x3<<28), 0 },

    { "if_c_and_nz",    COND_MASK_P2 | (0x4<<28), 0 },
    { "if_nz_and_c",    COND_MASK_P2 | (0x4<<28), 0 },

    { "if_ne",     COND_MASK_P2 | (0x5<<28), 0 },
    { "if_nz",     COND_MASK_P2 | (0x5<<28), 0 },

    { "if_c_ne_z", COND_MASK_P2 | (0x6<<28), 0 },
    { "if_z_ne_c", COND_MASK_P2 | (0x6<<28), 0 },

    { "if_nc_or_nz", COND_MASK_P2 | (0x7<<28), 0 },
    { "if_nz_or_nc", COND_MASK_P2 | (0x7<<28), 0 },

    { "if_c_and_z", COND_MASK_P2 | (0x8<<28), 0 },
    { "if_z_and_c", COND_MASK_P2 | (0x8<<28), 0 },

    { "if_c_eq_z", COND_MASK_P2 | (0x9<<28), 0 },
    { "if_z_eq_c", COND_MASK_P2 | (0x9<<28), 0 },

    { "if_e",      COND_MASK_P2 | (0xa<<28), 0 },
    { "if_z",      COND_MASK_P2 | (0xa<<28), 0 },

    { "if_nc_or_z", COND_MASK_P2 | (0xb<<28), 0 },
    { "if_z_or_nc", COND_MASK_P2 | (0xb<<28), 0 },

    { "if_b",      COND_MASK_P2 | (0xc<<28), 0 },
    { "if_c",      COND_MASK_P2 | (0xc<<28), 0 },
    { "if_lt",      COND_MASK_P2 | (0xc<<28), 0 },

    { "if_c_or_nz", COND_MASK_P2 | (0xd<<28), 0 },
    { "if_nz_or_c", COND_MASK_P2 | (0xd<<28), 0 },

    { "if_be",     COND_MASK_P2 | (0xe<<28), 0 },
    { "if_le",     COND_MASK_P2 | (0xe<<28), 0 },
    { "if_c_or_z", COND_MASK_P2 | (0xe<<28), 0 },
    { "if_z_or_c", COND_MASK_P2 | (0xe<<28), 0 },


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
    

    /* add hardware registers */
    for (i = 0; hwreg[i].name != NULL; i++) {
        AddSymbol(&spinReservedWords, hwreg[i].name, SYM_HWREG, (void *)&hwreg[i]);
        AddSymbol(&basicReservedWords, hwreg[i].name, SYM_HWREG, (void *)&hwreg[i]);
        AddSymbol(&cReservedWords, hwreg[i].cname, SYM_HWREG, (void *)&hwreg[i]);
    }

    /* add instructions */
    for (i = 0; instr[i].name != NULL; i++) {
        AddSymbol(&pasmWords, instr[i].name, SYM_INSTR, (void *)&instr[i]);
    }

    /* instruction modifiers */
    for (i = 0; modifiers[i].name != NULL; i++) {
        AddSymbol(&pasmWords, modifiers[i].name, SYM_INSTRMODIFIER, (void *)&modifiers[i]);
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
    if (s && !strcmp(name, s->name))
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
    saved_spinyychar = c = getSpinToken(&current->L, yval);
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
    bool forceLower = !gl_caseSensitive;
    
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
    if (c == '$' || c == '#' || c == '%') {
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
            if (sym->type == SYM_INSTR) {
                ast = NewAST(AST_INSTR, NULL, NULL);
                ast->d.ptr = sym->val;
                if (comment_chain) {
                    ast = AddToList(comment_chain, ast);
                    comment_chain = NULL;
                }
                *ast_ptr = ast;
                return BAS_INSTR;
            }
            if (sym->type == SYM_INSTRMODIFIER) {
                ast = NewAST(AST_INSTRMODIFIER, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return BAS_INSTRMODIFIER;
            }
            fprintf(stderr, "Internal error: Unknown pasm symbol type %d\n", sym->type);
        }
    }

    // check for keywords
    sym = FindSymbol(&basicReservedWords, idstr);
    if (sym != NULL) {
      if (sym->type == SYM_RESERVED) {
	c = INTVAL(sym);
        /* check for special handling */
        switch(c) {
        case BAS_ASM:
            if (InDatBlock(L)) {
                // leave the inline assembly
                L->in_block = SP_PUB;
            } else {
                L->in_block = SP_ASM;
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
            if (sym->type == SYM_VARIABLE) {
                ast = (AST *)sym->val;
                // check for an abstract object declaration
                if (ast->left && ast->left->kind == AST_OBJDECL && ast->left->left->kind == AST_IDENTIFIER && !strcmp(idstr, ast->left->left->d.string)) {
                    *ast_ptr = ast;
                    last_ast = AstIdentifier(idstr);
                    return BAS_TYPENAME;
                }
            } else if (sym->type == SYM_TYPEDEF) {
                ast = (AST *)sym->val;
                *ast_ptr = ast;
                last_ast = AstIdentifier(idstr);
                return BAS_TYPENAME;
            }
        }
    }
    // it's an identifier
    ast = NewAST(AST_IDENTIFIER, NULL, NULL);
    ast->d.string = idstr;
    *ast_ptr = last_ast = ast;
    return BAS_IDENTIFIER;
}

static int
getBasicToken(LexStream *L, AST **ast_ptr)
{
    int c;
    AST *ast = NULL;
    
    c = skipSpace(L, &ast, LANG_BASIC);

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
        } else {
            lexungetc(L, c2);
        }
    } else if (safe_isdigit(c)) {
        lexungetc(L,c);
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = parseNumber(L, 10, &ast->d.ival);
        if (c == SP_FLOATNUM) {
            ast->kind = AST_FLOAT;
	    c = BAS_FLOAT;
	} else if (ast->d.ival == 0) {
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
            c = BAS_INTEGER;
        }
    } else if (isIdentifierStart(c)) {
        lexungetc(L, c);
        c = parseBasicIdentifier(L, &ast);
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
    }
    *ast_ptr = last_ast = ast;
    return c;
}

int saved_basicyychar;

int
basicyylex(BASICYYSTYPE *yval)
{
    int c;
    saved_basicyychar = c = getBasicToken(&current->L, yval);
    if (c == BAS_EOF || c == EOF)
        return 0;
    return c;
}

static int
parseCIdentifier(LexStream *L, AST **ast_ptr)
{
    int c;
    struct flexbuf fb;
    Symbol *sym;
    AST *ast = NULL;
    char *idstr;
    
    flexbuf_init(&fb, INCSTR);
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
            if (sym->type == SYM_INSTR) {
                ast = NewAST(AST_INSTR, NULL, NULL);
                ast->d.ptr = sym->val;
                if (comment_chain) {
                    ast = AddToList(comment_chain, ast);
                    comment_chain = NULL;
                }
                *ast_ptr = ast;
                return C_INSTR;
            }
            if (sym->type == SYM_INSTRMODIFIER) {
                ast = NewAST(AST_INSTRMODIFIER, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return C_INSTRMODIFIER;
            }
            fprintf(stderr, "Internal error: Unknown pasm symbol type %d\n", sym->type);
        }
    }

    // check for keywords
    sym = FindSymbol(&cReservedWords, idstr);
    if (sym != NULL) {
      if (sym->type == SYM_RESERVED) {
	c = INTVAL(sym);
        /* check for special handling */
        switch(c) {
        case C_ASM:
            if (InDatBlock(L)) {
                // leave the inline assembly
                L->in_block = SP_PUB;
            } else {
                L->in_block = SP_ASM;
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
            if (sym->type == SYM_TYPEDEF) {
                ast = (AST *)sym->val;
                *ast_ptr = ast;
                last_ast = AstIdentifier(idstr);
                return C_TYPE_NAME;
            }
        }
    }
    // it's an identifier
    ast = AstIdentifier(idstr);
    *ast_ptr = last_ast = ast;
    return C_IDENTIFIER;
}

int saved_cgramyychar;

static int
getCToken(LexStream *L, AST **ast_ptr)
{
    int c, c2;
    AST *ast = NULL;
    
    c = skipSpace(L, &ast, LANG_C);

    // printf("c=%d L->linecounter=%d\n", c, L->lineCounter);
    if (c >= 127) {
        *ast_ptr = ast;
        return c;
    }
    if (safe_isdigit(c)) {
        lexungetc(L,c);
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = parseNumber(L, 10, &ast->d.ival);
        if (c == SP_FLOATNUM) {
            ast->kind = AST_FLOAT;
	    c = C_CONSTANT;
	} else if (ast->d.ival == 0) {
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
            c = C_CONSTANT;
	} else {
            c = C_CONSTANT;
        }
    } else if (isIdentifierStart(c)) {
        lexungetc(L, c);
        c = parseCIdentifier(L, &ast);
    } else if (c == '"') {
        parseCString(L, &ast);
	c = C_STRING_LITERAL;
    } else if (c == '}' && InDatBlock(L)) {
        L->in_block = SP_PUB;
    } else if (c == '\'') {
        c = lexgetc(L);
        if (c == '\\') {
            c = getEscapedChar(L);
        }
        c2  = lexgetc(L);
        if (c2 != '\'') {
            SYNTAX_ERROR("expected closing \' ");
        }
        ast = AstInteger(c);
        c = C_CONSTANT;
    } else if (c == '.') {
        c2 = lexgetc(L);
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
    } else if (c == '%') {
        c2 = lexgetc(L);
        if (c2 == '=') {
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
    saved_cgramyychar = c = getCToken(&current->L, yval);
    if (c == EOF)
        return 0;
    return c;
}
