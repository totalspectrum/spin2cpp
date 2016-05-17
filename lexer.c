//
// Simple lexical analyzer for a language where indentation
// is significant
//
// Copyright (c) 2011-2016 Total Spectrum Software Inc.
//
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "spinc.h"
#include "preprocess.h"

// used for error messages
AST *last_ast;

/* flag: if set, run the  preprocessor */
int gl_preprocess = 1;

static inline int
safe_isalpha(unsigned int x) {
    return (x < 255) ? isalpha(x) : 0;
}
static inline int
safe_isdigit(unsigned int x) {
    return (x < 255) ? isdigit(x) : 0;
}

SymbolTable reservedWords;
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
    return (c == 0) ? T_EOF : c;
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
    flexbuf_init(&L->lines, 1024);
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

    return (c >= 0) ? c : T_EOF;
}

static int 
filegetwc(LexStream *L)
{
    FILE *f;
    int c1, c2;

    f = (FILE *)L->ptr;
again:
    c1 = fgetc(f);
    if (c1 < 0) return T_EOF;
    c2 = fgetc(f);
    if (c2 != 0) {
        if (c2 < 0) return T_EOF;
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
    flexbuf_init(&L->lines, 1024);
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

#define TAB_STOP 8
/*
 *
 */
int
lexgetc(LexStream *L)
{
    int c;
    char *linedata;
    if (L->ungot_ptr) {
        --L->ungot_ptr;
        return L->ungot[L->ungot_ptr];
    }
    if (L->pendingLine) {
      flexbuf_addchar(&L->curLine, 0); // 0 terminate the line
      linedata = flexbuf_get(&L->curLine);
      flexbuf_addmem(&L->lines, (char *)&linedata, sizeof(linedata));
      L->lineCounter ++;
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
    if (c == T_EOF) {
      flexbuf_addchar(&L->curLine, 0); // 0 terminate the line
      linedata = flexbuf_get(&L->curLine);
      flexbuf_addmem(&L->lines, (char *)&linedata, sizeof(linedata));
    } else {
        flexbuf_addchar(&L->curLine, c);
    }
    return c;
}

void
lexungetc(LexStream *L, int c)
{
    if (L->ungot_ptr == UNGET_MAX-1) {
        fprintf(stderr, "ERROR: unget limit exceeded\n");
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
// if the line is indented more than this, a T_INDENT will
// be emitted
//
void
EstablishIndent(LexStream *L, int level)
{
    AST *dummy = NewAST(AST_UNKNOWN, NULL, NULL);
    if (L->indentsp >= MAX_INDENTS-1) {
        ERROR(dummy, "too many nested indentation levels");
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
    int kind = T_NUM;

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
        uval = floatAsInt(f);
        kind = T_FLOATNUM;
    }
donefloat:
    lexungetc(L, c);
    *num = uval;
    return sawdigit ? kind : ((base == 16) ? T_HERE : '%');
}

/* check for DAT or ASM blocks */
static int InDatBlock(LexStream *L)
{
    return L->in_block == T_DAT || L->in_block == T_ASM;
}

/* parse an identifier */
static int
parseIdentifier(LexStream *L, AST **ast_ptr, const char *prefix)
{
    int c;
    struct flexbuf fb;
    Symbol *sym;
    AST *ast = NULL;
    int startColumn = L->colCounter - 1;
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
        //flexbuf_addchar(&fb, tolower(c));
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
                *ast_ptr = ast;
                return T_INSTR;
            }
            if (sym->type == SYM_INSTRMODIFIER) {
                ast = NewAST(AST_INSTRMODIFIER, NULL, NULL);
                ast->d.ptr = sym->val;
                *ast_ptr = ast;
                return T_INSTRMODIFIER;
            }
            fprintf(stderr, "Internal error: Unknown pasm symbol type %d\n", sym->type);
        }
    }
    sym = FindSymbol(&reservedWords, idstr);
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
            case T_PUB:
            case T_PRI:
            case T_DAT:
            case T_OBJ:
            case T_VAR:
            case T_CON:
                L->in_block = c;
                L->block_firstline = L->lineCounter;
                //EstablishIndent(L, 1);
                break;
	    case T_ASM:
	        if (L->in_block == T_ASM) {
		    fprintf(stderr, "WARNING: ignoring nested asm\n");
		} else {
		    L->save_block = L->in_block;
		}
		L->in_block = c;
		break;
	    case T_ENDASM:
	        L->in_block = L->save_block;
	        break;
            case T_IF:
            case T_IFNOT:
            case T_ELSE:
            case T_ELSEIF:
            case T_ELSEIFNOT:
            case T_REPEAT:
            case T_CASE:
                EstablishIndent(L, startColumn);
                break;
            default:
                break;
            }
            if (!ast)
                ast = GetComments();
            *ast_ptr = ast;
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
    /* make sure identifiers do not conflict with C keywords */
    if (gl_normalizeIdents || Is_C_Reserved(idstr)) {
        NormalizeIdentifier(idstr);
    }
    ast->d.string = idstr;
    *ast_ptr = ast;
    return T_IDENTIFIER;
}

/* parse a string */
static int
parseString(LexStream *L, AST **ast_ptr)
{
    int c;
    struct flexbuf fb;
    AST *ast;

    flexbuf_init(&fb, INCSTR);
    c = lexgetc(L);
    while (c != '"' && c > 0 && c < 256) {
        flexbuf_addchar(&fb, c);
        c = lexgetc(L);
    }
    flexbuf_addchar(&fb, '\0');

    ast = NewAST(AST_STRING, NULL, NULL);
    ast->d.string = flexbuf_get(&fb);
    *ast_ptr = ast;
    return T_STRING;
}

//
// keep track of accumulated comments
//

AST *comment_chain;

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

//
// skip over comments and spaces
// return first non-comment non-space character
// if we are inside a function, emit T_INDENT when
// we increase the indent level, T_OUTDENT when we
// decrease it
//

int
skipSpace(LexStream *L, AST **ast_ptr)
{
    int c;
    int commentNest;
    int start_indent;
    struct flexbuf cb;
    AST *ast;
    int startcol = 0;
    int startline = 0;
    
    flexbuf_init(&cb, INCSTR);
    c = lexgetc(L);
again:
    while (c == ' ' || c == '\t') {
        c = lexgetc(L);
    }

    /* ignore completely empty lines or ones with just comments */
    if (c == '\'') {
        while (c == '\'') c = lexgetc(L);
        while (c != '\n' && c != T_EOF) {
            flexbuf_addchar(&cb, c);
            c = lexgetc(L);
        }
        flexbuf_addchar(&cb, '\n');
        flexbuf_addchar(&cb, 0);
        ast = NewAST(AST_COMMENT, NULL, NULL);
        ast->d.string = flexbuf_get(&cb);
        comment_chain = AddToList(comment_chain, ast);
    }
    if (c == '{') {
        struct flexbuf anno;
        int annotate = 0;
        int directive = 0;
	int doccomment = 0;
        
        startcol = L->colCounter;
        startline = L->lineCounter;
        flexbuf_init(&anno, INCSTR);
        commentNest = 1;
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
        } else if (c == '{') {
	    c = lexgetc(L);
	    doccomment = 1;
	}
        lexungetc(L, c);
        for(;;) {
            c = lexgetc(L);
            if (c == '{' && !doccomment)
                commentNest++;
            else if (c == '}') {
	        if (doccomment) {
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
            if (commentNest <= 0 || c == T_EOF) {
                break;
            }
            if (annotate || directive) {
                flexbuf_addchar(&anno, c);
            } else {
                flexbuf_addchar(&cb, c);
            }
        }
        if (c == T_EOF) {
	    if (commentNest > 0)
	        fprintf(stderr, "WARNING: EOF seen inside comment\n");
            return c;
	}
        if (annotate) {
            AST *ast = NewAST(AST_ANNOTATION, NULL, NULL);
            flexbuf_addchar(&anno, '\0');
            ast->d.string = flexbuf_get(&anno);
            *ast_ptr = ast;
            // if this is indented and inside a PUB or PRI,
            // then treat it as inline C code
            if (startcol > 1 && startline > L->block_firstline && (L->in_block == T_PUB || L->in_block == T_PRI)) {
                return T_INLINECCODE;
            }
            return T_ANNOTATION;
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
                    L->fileName = strdup(ptr);
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

    if (L->eoln && (L->in_block == T_PUB || L->in_block == T_PRI)) {
        if (c == '\n') {
            c = lexgetc(L);
            goto again;
        }
        /* if there is a pending indent, send it back */
        if (L->pending_indent) {
            lexungetc(L, c);
            --L->pending_indent;
            return T_INDENT;
        }
        /* on EOF send as many OUTDENTS as we need */
        if (c == T_EOF) {
            if (L->indentsp > 0) {
                lexungetc(L, c);
                --L->indentsp;
                return T_OUTDENT;
            }
        }
        /* if our indentation is <= the start value, send back an outdent */
        start_indent = L->colCounter-1;
        if (start_indent <= L->indent[L->indentsp] && L->indentsp > 0) {
            lexungetc(L, c);
            --L->indentsp;
            return T_OUTDENT;
        }
    }
    // force an end-of line at EOF
    if (c == T_EOF && !L->eoln && !L->eof) {
        L->eof = L->eoln = 1;
        return T_EOLN;
    }
    if (L->eoln) {
        L->eoln = 0;
        L->firstNonBlank = L->colCounter-1;
    }
    if (c == '\n') {
        L->eoln = 1;
        return T_EOLN;
    }
    if (current && !current->sawToken) {
        current->sawToken = 1;
        current->topcomment = GetComments();
    }
    return c;
}


static char operator_chars[] = "-+*/|<>=!@~#^.?";

int
getToken(LexStream *L, AST **ast_ptr)
{
//    int base = 10;
    int c;
    AST *ast = NULL;
    int at_startofline = (L->eoln == 1);
    int peekc;
    c = skipSpace(L, &ast);

    if (c >= 127) {
        *ast_ptr = last_ast = ast;
        return c;
    } else if (safe_isdigit(c)) {
        lexungetc(L,c);
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = parseNumber(L, 10, &ast->d.ival);
        if (c == T_FLOATNUM)
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
        c = parseIdentifier(L, &ast, NULL);
        /* if in pasm, and at start of line, restart temporary
           labels */
        if (c == T_IDENTIFIER && InDatBlock(L) && at_startofline) {
            L->lastGlobal = ast->d.string;
        }
    } else if (c == ':') {
        peekc = lexgetc(L);
        if (peekc == '=') {
            c = T_ASSIGN;
        } else if (!gl_p2 && isIdentifierStart(peekc) && InDatBlock(L)) {
            lexungetc(L, peekc);
            c = parseIdentifier(L, &ast, L->lastGlobal ? L->lastGlobal : "");
        } else {
            lexungetc(L, peekc);
        }
    } else if (gl_p2 && c == '.' && isIdentifierStart(lexpeekc(L)) && InDatBlock(L)) {
            c = parseIdentifier(L, &ast, L->lastGlobal ? L->lastGlobal : "");
    } else if (strchr(operator_chars, c) != NULL) {
        char op[6];
        int i;
        int token;
        Symbol *sym = NULL;

        op[0] = token = c;
        for (i = 1; i < sizeof(op)-1; i++) {
            c = lexgetc(L);
            if (c >= 128 || strchr(operator_chars, c) == NULL) {
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
    } else if (c == '"') {
        c = parseString(L, &ast);
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
} init_words[] = {
    { "abort", T_ABORT },
    { "and", T_AND },
    { "asm", T_ASM },
    { "byte", T_BYTE },

    { "case", T_CASE },
    { "cognew", T_COGNEW },
    { "coginit", T_COGINIT },
    { "con", T_CON },
    { "constant", T_CONSTANT },

    { "dat", T_DAT },

    { "else", T_ELSE },
    { "elseif", T_ELSEIF },
    { "elseifnot", T_ELSEIFNOT },
    { "endasm", T_ENDASM },

    { "file", T_FILE },
    { "fit", T_FIT },
    { "float", T_FLOAT },
    { "from", T_FROM },

    { "if", T_IF },
    { "ifnot", T_IFNOT },

    { "long", T_LONG },
    { "lookdown", T_LOOKDOWN },
    { "lookdownz", T_LOOKDOWNZ },
    { "lookup", T_LOOKUP },
    { "lookupz", T_LOOKUPZ },

    { "next", T_NEXT },
    { "not", T_NOT },

    { "obj", T_OBJ },
    { "or", T_OR },
    { "org", T_ORG },
    { "orgh", T_ORGH },
    { "other", T_OTHER },

    { "quit", T_QUIT },
    { "pri", T_PRI },
    { "pub", T_PUB },
    { "repeat", T_REPEAT },
    { "res", T_RES },
    { "result", T_RESULT },
    { "return", T_RETURN },
    { "round", T_ROUND },

    { "spr", T_SPR },
    { "step", T_STEP },
    { "string", T_STRINGPTR },
    { "to", T_TO },
    { "trunc", T_TRUNC },
    { "then", T_THEN },

    { "until", T_UNTIL },

    { "var", T_VAR },

    { "while", T_WHILE },
    { "word", T_WORD },

    /* operators */
    { "+", '+' },
    { "-", '-' },
    { "/", '/' },
    { "?", '?' },
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

    { "><", T_REV },
    { "->", T_ROTR },
    { "<-", T_ROTL },

    { "<<", T_SHL },
    { ">>", T_SHR },
    { "~>", T_SAR },

    { "..", T_DOTS },
    { "|<", T_DECODE },
    { ">|", T_ENCODE },
    { "||", T_ABS },
    { "#>", T_LIMITMIN },
    { "<#", T_LIMITMAX },

    { "~~", T_DOUBLETILDE },
    { "++", T_INCREMENT },
    { "--", T_DECREMENT },
    { "^^", T_SQRT },

    { "@@", T_DOUBLEAT },
    { "@@@", T_TRIPLEAT },
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
static void lockhook(Builtin *dummy) { current->needsLockFuncs = 1; }

Builtin builtinfuncs[] = {
    { "clkfreq", 0, defaultVariable, "CLKFREQ", 0, NULL },
    { "clkmode", 0, defaultVariable, "CLKMODE", 0, NULL },
    { "clkset", 2, defaultBuiltin, "clkset", 0, NULL },

    { "cogstop", 1, defaultBuiltin, "cogstop", 0, NULL },
    { "cogid", 0, defaultBuiltin, "cogid", 0, NULL },

    { "locknew", 0, defaultBuiltin, "locknew", 0, lockhook },
    { "lockset", 1, defaultBuiltin, "lockset", 0, lockhook },
    { "lockclr", 1, defaultBuiltin, "lockclr", 0, lockhook },
    { "lockret", 1, defaultBuiltin, "lockret", 0, lockhook },

    { "strsize", 1, str1Builtin, "strlen", 0, NULL },
    { "strcomp", 2, strcompBuiltin, "strcmp", 0, NULL },
    { "waitcnt", 1, defaultBuiltin, "waitcnt", 0, NULL },
    { "waitpeq", 3, waitpeqBuiltin, "waitpeq", 0, NULL },
    { "waitpne", 3, waitpeqBuiltin, "waitpne", 0, NULL },

    { "reboot", 0, rebootBuiltin, "reboot", 0, NULL },

    { "longfill", 3, memFillBuiltin, "memset", 4, NULL },
    { "longmove", 3, memBuiltin, "memmove", 4, NULL },
    { "wordfill", 3, memFillBuiltin, "memset", 2, NULL },
    { "wordmove", 3, memBuiltin, "memmove", 2, NULL },
    { "bytefill", 3, memBuiltin, "memset", 1, NULL },
    { "bytemove", 3, memBuiltin, "memcpy", 1, NULL },

    { "getcnt", 0, defaultBuiltin, "getcnt", 0, NULL },
};

struct constants {
    const char *name;
    int     type;
    int32_t val;
} constants[] = {
    { "CHIPVER", SYM_CONSTANT, 1 },
    { "TRUE", SYM_CONSTANT, -1 },
    { "FALSE", SYM_CONSTANT, 0 },
    { "POSX", SYM_CONSTANT, 0x7fffffff },
    { "NEGX", SYM_CONSTANT, 0x80000000U },
    { "RCFAST", SYM_CONSTANT, 0x00000001 },
    { "RCSLOW", SYM_CONSTANT, 0x00000002 },
    { "XINPUT", SYM_CONSTANT, 0x00000004 },
    { "XTAL1", SYM_CONSTANT, 0x00000008 },
    { "XTAL2", SYM_CONSTANT, 0x00000010 },
    { "XTAL3", SYM_CONSTANT, 0x00000020 },
    { "PLL1X", SYM_CONSTANT, 0x00000040 },
    { "PLL2X", SYM_CONSTANT, 0x00000080 },
    { "PLL4X", SYM_CONSTANT, 0x00000100 },
    { "PLL8X", SYM_CONSTANT, 0x00000200 },
    { "PLL16X", SYM_CONSTANT, 0x00000400 },
    { "PI", SYM_FLOAT_CONSTANT, 0x40490fdb },
};

void
initLexer(int flags)
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

    /* and builtin constants */
    for (i = 0; i < N_ELEMENTS(constants); i++) {
        AddSymbol(&reservedWords, constants[i].name, constants[i].type, AstInteger(constants[i].val));
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
    return FindSymbol(&reservedWords, name) != 0;
}

Instruction *instr;

Instruction
instr_p1[] = {
  { "abs",    0xa8800000, TWO_OPERANDS, OPC_ABS },
  { "absneg", 0xac800000, TWO_OPERANDS, OPC_GENERIC },
  { "add",    0x80800000, TWO_OPERANDS, OPC_ADD },
  { "addabs", 0x88800000, TWO_OPERANDS, OPC_GENERIC },
  { "adds",   0xd0800000, TWO_OPERANDS, OPC_GENERIC },
  { "addsx",  0xd8800000, TWO_OPERANDS, OPC_GENERIC },
  { "addx",   0xc8800000, TWO_OPERANDS, OPC_GENERIC },
  { "and",    0x60800000, TWO_OPERANDS, OPC_AND },
  { "andn",   0x64800000, TWO_OPERANDS, OPC_ANDN },

  { "call",   0x5cc00000, CALL_OPERAND, OPC_CALL },
  { "clkset", 0x0c400000, DST_OPERAND_ONLY, OPC_GENERIC },
  { "cmp",    0x84000000, TWO_OPERANDS, OPC_CMP },
  { "cmps",   0xc0000000, TWO_OPERANDS, OPC_CMPS },
  { "cmpsub", 0xe0800000, TWO_OPERANDS, OPC_GENERIC },
  { "cmpsx",  0xc4000000, TWO_OPERANDS, OPC_GENERIC },
  { "cmpx",   0xcc000000, TWO_OPERANDS, OPC_GENERIC },

  { "cogid",   0x0cc00001, DST_OPERAND_ONLY, OPC_COGID },
  { "coginit", 0x0c400002, DST_OPERAND_ONLY, OPC_GENERIC },
  { "cogstop", 0x0c400003, DST_OPERAND_ONLY, OPC_COGSTOP },

  { "djnz",   0xe4800000, JMPRET_OPERANDS, OPC_DJNZ },
  { "hubop",  0x0c000000, TWO_OPERANDS, OPC_GENERIC },
  { "jmp",    0x5c000000, SRC_OPERAND_ONLY, OPC_JUMP },
  { "jmpret", 0x5c800000, JMPRET_OPERANDS, OPC_GENERIC_BRANCH },

  { "lockclr",0x0c400007, DST_OPERAND_ONLY, OPC_GENERIC_NR },
  { "locknew",0x0cc00004, DST_OPERAND_ONLY, OPC_GENERIC },
  { "lockret",0x0c400005, DST_OPERAND_ONLY, OPC_GENERIC_NR },
  { "lockset",0x0c400006, DST_OPERAND_ONLY, OPC_GENERIC_NR },

  { "max",    0x4c800000, TWO_OPERANDS, OPC_GENERIC },
  { "maxs",   0x44800000, TWO_OPERANDS, OPC_MAXS },
  { "min",    0x48800000, TWO_OPERANDS, OPC_GENERIC },
  { "mins",   0x40800000, TWO_OPERANDS, OPC_MINS },
  { "mov",    0xa0800000, TWO_OPERANDS, OPC_MOV },
  { "movd",   0x54800000, TWO_OPERANDS, OPC_MOVD },
  { "movi",   0x58800000, TWO_OPERANDS, OPC_GENERIC },
  { "movs",   0x50800000, TWO_OPERANDS, OPC_MOVS },

  { "muxc",   0x70800000, TWO_OPERANDS, OPC_MUXC },
  { "muxnc",  0x74800000, TWO_OPERANDS, OPC_MUXNC },
  { "muxz",   0x78800000, TWO_OPERANDS, OPC_MUXZ },
  { "muxnz",  0x7c800000, TWO_OPERANDS, OPC_MUXNZ },

  { "neg",    0xa4800000, TWO_OPERANDS, OPC_NEG },
  { "negc",   0xb0800000, TWO_OPERANDS, OPC_GENERIC },
  { "negnc",  0xb4800000, TWO_OPERANDS, OPC_GENERIC },
  { "negnz",  0xbc800000, TWO_OPERANDS, OPC_GENERIC },
  { "negz",   0xb8800000, TWO_OPERANDS, OPC_GENERIC },
  { "nop",    0x00000000, NO_OPERANDS, OPC_NOP },

  { "or",     0x68800000, TWO_OPERANDS, OPC_OR },

  { "rcl",    0x34800000, TWO_OPERANDS, OPC_GENERIC },
  { "rcr",    0x30800000, TWO_OPERANDS, OPC_GENERIC },
  { "rdbyte", 0x00800000, TWO_OPERANDS, OPC_RDBYTE },
  { "rdlong", 0x08800000, TWO_OPERANDS, OPC_RDLONG },
  { "rdword", 0x04800000, TWO_OPERANDS, OPC_RDWORD },
  { "ret",    0x5c400000, NO_OPERANDS, OPC_RET },
  { "rev",    0x3c800000, TWO_OPERANDS, OPC_REV_P1 },
  { "rol",    0x24800000, TWO_OPERANDS, OPC_ROL },
  { "ror",    0x20800000, TWO_OPERANDS, OPC_ROR },

  { "sar",    0x38800000, TWO_OPERANDS, OPC_SAR },
  { "shl",    0x2c800000, TWO_OPERANDS, OPC_SHL },
  { "shr",    0x28800000, TWO_OPERANDS, OPC_SHR },
  { "sub",    0x84800000, TWO_OPERANDS, OPC_SUB },
  { "subabs", 0x8c800000, TWO_OPERANDS, OPC_GENERIC },
  { "subs",   0xc0800000, TWO_OPERANDS, OPC_GENERIC },
  { "subsx",  0xc4800000, TWO_OPERANDS, OPC_GENERIC },
  { "subx",   0xcc800000, TWO_OPERANDS, OPC_GENERIC },
  { "sumc",   0x90800000, TWO_OPERANDS, OPC_GENERIC },
  { "sumnc",  0x94800000, TWO_OPERANDS, OPC_GENERIC },
  { "sumnz",  0x9c800000, TWO_OPERANDS, OPC_GENERIC },
  { "sumz",   0x98800000, TWO_OPERANDS, OPC_GENERIC },

  { "test",   0x60000000, TWO_OPERANDS, OPC_TEST },
  { "testn",  0x64000000, TWO_OPERANDS, OPC_TESTN },
  { "tjnz",   0xe8000000, JMPRET_OPERANDS, OPC_GENERIC_BRANCH },
  { "tjz",    0xec000000, JMPRET_OPERANDS, OPC_GENERIC_BRANCH },

  { "waitcnt", 0xf8800000, TWO_OPERANDS, OPC_WAITCNT },
  { "waitpeq", 0xf0000000, TWO_OPERANDS, OPC_GENERIC_NR },
  { "waitpne", 0xf4000000, TWO_OPERANDS, OPC_GENERIC_NR },
  { "waitvid", 0xfc000000, TWO_OPERANDS, OPC_GENERIC_NR },

  { "wrbyte", 0x00000000, TWO_OPERANDS, OPC_WRBYTE },
  { "wrlong", 0x08000000, TWO_OPERANDS, OPC_WRLONG },
  { "wrword", 0x04000000, TWO_OPERANDS, OPC_WRWORD },

  { "xor",    0x6c800000, TWO_OPERANDS, OPC_XOR },

  { NULL, 0, NO_OPERANDS, OPC_GENERIC},
};

Instruction
instr_p2[] = {
  { "nop",    0x00000000, NO_OPERANDS,  OPC_NOP },
  { "ror",    0x00000000, TWO_OPERANDS, OPC_ROR },
  { "rol",    0x00200000, TWO_OPERANDS, OPC_ROL },
  { "shr",    0x00400000, TWO_OPERANDS, OPC_SHR },
  { "shl",    0x00600000, TWO_OPERANDS, OPC_SHL },
  { "rcr",    0x00800000, TWO_OPERANDS, OPC_GENERIC },
  { "rcl",    0x00a00000, TWO_OPERANDS, OPC_GENERIC },
  { "sar",    0x00c00000, TWO_OPERANDS, OPC_SAR },
  { "sal",    0x00e00000, TWO_OPERANDS, OPC_GENERIC },
  { "add",    0x01000000, TWO_OPERANDS, OPC_ADD },
  { "adds",   0x01400000, TWO_OPERANDS, OPC_GENERIC },
  { "addx",   0x01200000, TWO_OPERANDS, OPC_GENERIC },
  { "addsx",  0x01600000, TWO_OPERANDS, OPC_GENERIC },
  { "sub",    0x01800000, TWO_OPERANDS, OPC_SUB },
  { "subx",   0x01a00000, TWO_OPERANDS, OPC_GENERIC },
  { "subs",   0x01c00000, TWO_OPERANDS, OPC_GENERIC },
  { "subsx",  0x01e00000, TWO_OPERANDS, OPC_GENERIC },
  { "cmp",    0x02000000, TWO_OPERANDS, OPC_CMP },
  { "cmpx",   0x02200000, TWO_OPERANDS, OPC_GENERIC },
  { "cmps",   0x02400000, TWO_OPERANDS, OPC_CMPS },
  { "cmpsx",  0x02600000, TWO_OPERANDS, OPC_GENERIC },
  { "cmpr",   0x02800000, TWO_OPERANDS, OPC_GENERIC },
  { "cmpm",   0x02a00000, TWO_OPERANDS, OPC_GENERIC },
  { "subr",   0x02c00000, TWO_OPERANDS, OPC_GENERIC },
  { "cmpsub", 0x02e00000, TWO_OPERANDS, OPC_GENERIC },

  { "min",    0x03000000, TWO_OPERANDS, OPC_GENERIC },
  { "max",    0x03200000, TWO_OPERANDS, OPC_GENERIC },
  { "mins",   0x03400000, TWO_OPERANDS, OPC_MINS },
  { "maxs",   0x03600000, TWO_OPERANDS, OPC_MAXS },
  { "sumc",   0x03800000, TWO_OPERANDS, OPC_GENERIC },
  { "sumnc",  0x03a00000, TWO_OPERANDS, OPC_GENERIC },
  { "sumz",   0x03c00000, TWO_OPERANDS, OPC_GENERIC },
  { "sumnz",  0x03e00000, TWO_OPERANDS, OPC_GENERIC },

  { "isob",   0x04000000, TWO_OPERANDS, OPC_GENERIC },
  { "notb",   0x04200000, TWO_OPERANDS, OPC_GENERIC },
  { "clrb",   0x04400000, TWO_OPERANDS, OPC_GENERIC },
  { "setb",   0x04600000, TWO_OPERANDS, OPC_GENERIC },
  { "setbc",  0x04800000, TWO_OPERANDS, OPC_GENERIC },
  { "setbnc", 0x04a00000, TWO_OPERANDS, OPC_GENERIC },
  { "setbz",  0x04c00000, TWO_OPERANDS, OPC_GENERIC },
  { "setbnz", 0x04e00000, TWO_OPERANDS, OPC_GENERIC },

  { "andn",   0x05000000, TWO_OPERANDS, OPC_ANDN },
  { "and",    0x05200000, TWO_OPERANDS, OPC_AND },
  { "or",     0x05400000, TWO_OPERANDS, OPC_OR },
  { "xor",    0x05600000, TWO_OPERANDS, OPC_XOR },
  { "muxc",   0x05800000, TWO_OPERANDS, OPC_MUXC },
  { "muxnc",  0x05a00000, TWO_OPERANDS, OPC_MUXNC },
  { "muxz",   0x05c00000, TWO_OPERANDS, OPC_MUXZ },
  { "muxnz",  0x05e00000, TWO_OPERANDS, OPC_MUXNZ },

  { "mov",    0x06000000, TWO_OPERANDS, OPC_MOV },
  { "not",    0x06200000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "abs",    0x06400000, TWO_OPERANDS_OPTIONAL, OPC_ABS },
  { "neg",    0x06600000, TWO_OPERANDS_OPTIONAL, OPC_NEG },
  { "negc",   0x06800000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "negnc",  0x06a00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "negz",   0x06c00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "negnz",  0x06e00000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },

  { "alti",   0x07000000, TWO_OPERANDS, OPC_GENERIC },
  { "altr",   0x07080000, TWO_OPERANDS, OPC_GENERIC },
  { "altd",   0x07100000, TWO_OPERANDS, OPC_GENERIC },
  { "alts",   0x07180000, TWO_OPERANDS, OPC_GENERIC },
  { "decod",  0x07200000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "topone", 0x07400000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "botone", 0x07600000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "incmod", 0x07800000, TWO_OPERANDS, OPC_GENERIC },
  { "decmod", 0x07a00000, TWO_OPERANDS, OPC_GENERIC },
  { "mul",    0x07c00000, TWO_OPERANDS, OPC_GENERIC },
  { "muls",   0x07e00000, TWO_OPERANDS, OPC_GENERIC },
  
  { "setbyts", 0x09500000, TWO_OPERANDS, OPC_GENERIC },
  { "movbyts", 0x09580000, TWO_OPERANDS, OPC_GENERIC },
  { "splitb", 0x09600000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "mergeb", 0x09680000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "splitw", 0x09700000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "mergew", 0x09780000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "seussf", 0x09800000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "seussr", 0x09880000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "rgbsqz", 0x09900000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "rgbexp", 0x09980000, TWO_OPERANDS_OPTIONAL, OPC_GENERIC },
  { "rev",    0x09a00000, TWO_OPERANDS_OPTIONAL, OPC_REV_P2 },
  { "seti",   0x09a80000, TWO_OPERANDS, OPC_GENERIC },
  { "setd",   0x09b00000, TWO_OPERANDS, OPC_GENERIC },
  { "sets",   0x09b80000, TWO_OPERANDS, OPC_GENERIC },
  
  { "djz",    0x09c00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH },
  { "djnz",   0x09c80000, P2_TJZ_OPERANDS, OPC_DJNZ },
  { "djs",    0x09d00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH },
  { "djns",   0x09d80000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH },
  { "tjz",    0x09e00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH },
  { "tjnz",   0x09e80000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH },
  { "tjs",    0x09f00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH },
  { "tjns",   0x09f80000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH },

  { "testn",  0x0a000000, TWO_OPERANDS, OPC_GENERIC_NR },
  { "test",   0x0a200000, TWO_OPERANDS, OPC_TEST },
  { "anyb",   0x0a400000, TWO_OPERANDS, OPC_GENERIC_NR },
  { "testb",  0x0a600000, TWO_OPERANDS, OPC_GENERIC_NR },
  { "addct1", 0x0a800000, TWO_OPERANDS, OPC_ADDCT1 },
  { "addct2", 0x0a880000, TWO_OPERANDS, OPC_GENERIC },
  { "addct3", 0x0a900000, TWO_OPERANDS, OPC_GENERIC },
  { "wmlong", 0x0a980000, P2_RDWR_OPERANDS, OPC_GENERIC },

  { "calld",  0x0aa00000, P2_TJZ_OPERANDS, OPC_GENERIC_BRANCH },

  { "pingetz",0x0ac00000, TWO_OPERANDS, OPC_GENERIC },
  { "rdlut",  0x0ae00000, TWO_OPERANDS, OPC_GENERIC },
  
  { "rdbyte", 0x0b000000, P2_RDWR_OPERANDS, OPC_RDBYTE },
  { "rdword", 0x0b200000, P2_RDWR_OPERANDS, OPC_RDWORD },
  { "rdlong", 0x0b400000, P2_RDWR_OPERANDS, OPC_RDLONG },
  { "addpix", 0x0b600000, TWO_OPERANDS, OPC_GENERIC },
  { "mulpix", 0x0b680000, TWO_OPERANDS, OPC_GENERIC },
  { "blnpix", 0x0b700000, TWO_OPERANDS, OPC_GENERIC },
  { "mixpix", 0x0b780000, TWO_OPERANDS, OPC_GENERIC },

  { "pinsetm",0x0b800000, P2_TWO_OPERANDS, OPC_GENERIC },
  { "pinack", 0x0b880000, SRC_OPERAND_ONLY, OPC_GENERIC }, // alias for pinsetm #0,d
  { "pinsetx",0x0b900000, P2_TWO_OPERANDS, OPC_GENERIC },
  { "pinsety",0x0ba00000, P2_TWO_OPERANDS, OPC_GENERIC },
  { "wrlut",  0x0bb00000, P2_TWO_OPERANDS, OPC_GENERIC },

  { "wrbyte", 0x0c400000, P2_RDWR_OPERANDS, OPC_WRBYTE },
  { "wrword", 0x0c500000, P2_RDWR_OPERANDS, OPC_WRWORD },
  { "wrlong", 0x0c600000, P2_RDWR_OPERANDS, OPC_WRLONG },

  { "coginit",0x0ce00000, P2_TWO_OPERANDS, OPC_GENERIC },
  { "qmul",   0x0d000000, P2_TWO_OPERANDS, OPC_GENERIC },
  { "qdiv",   0x0d100000, P2_TWO_OPERANDS, OPC_GENERIC },
  { "qfrac",  0x0d200000, P2_TWO_OPERANDS, OPC_GENERIC },
  { "qsqrt",  0x0d300000, P2_TWO_OPERANDS, OPC_GENERIC },
  { "qrotate",0x0d400000, P2_TWO_OPERANDS, OPC_GENERIC },
  { "qvector",0x0d500000, P2_TWO_OPERANDS, OPC_GENERIC },

  { "clkset", 0x0d600000, P2_DST_CONST_OK,  OPC_GENERIC },
  { "cogid",  0x0d600001, DST_OPERAND_ONLY, OPC_COGID },
  { "cogstop",0x0d600003, P2_DST_CONST_OK,  OPC_COGSTOP },
  { "locknew",0x0d600004, DST_OPERAND_ONLY, OPC_GENERIC },
  { "lockret",0x0d600005, P2_DST_CONST_OK, OPC_GENERIC_NR },
  { "lockclr",0x0d600006, P2_DST_CONST_OK, OPC_GENERIC_NR },
  { "lockset",0x0d600007, P2_DST_CONST_OK, OPC_GENERIC_NR },

  { "setq",   0x0d600016, P2_DST_CONST_OK, OPC_GENERIC_NR },
  { "setq2",  0x0d600017, P2_DST_CONST_OK, OPC_GENERIC_NR },
  { "getqx",  0x0d600018, DST_OPERAND_ONLY, OPC_GENERIC },
  { "getqy",  0x0d600019, DST_OPERAND_ONLY, OPC_GENERIC },
  { "getct",  0x0d60001a, DST_OPERAND_ONLY, OPC_GENERIC },

  // indirect jumps via register
  // normally the user will write "jmp x" and the assembler
  // will recognize x is a register and rewrite it as "jmp.i x"
  { "jmp.i" , 0x0d60002c, DST_OPERAND_ONLY, OPC_GENERIC },
  { "call.i", 0x0d60002d, DST_OPERAND_ONLY, OPC_GENERIC },
  { "calla.i",0x0d60002e, DST_OPERAND_ONLY, OPC_GENERIC },
  { "callb.i",0x0d60002f, DST_OPERAND_ONLY, OPC_GENERIC },

  { "ret",    0x0d600031, NO_OPERANDS, OPC_GENERIC_BRANCH },
  { "reta",   0x0d600032, NO_OPERANDS, OPC_RET },
  { "retb",   0x0d600033, NO_OPERANDS, OPC_GENERIC_BRANCH },

  { "waitint",0x0d602024, NO_OPERANDS, OPC_GENERIC },
  { "waitct1",0x0d602224, NO_OPERANDS, OPC_GENERIC },
  { "waitct2",0x0d602424, NO_OPERANDS, OPC_GENERIC },
  { "waitct3",0x0d602624, NO_OPERANDS, OPC_GENERIC },
  { "waitpat",0x0d602824, NO_OPERANDS, OPC_GENERIC },
  { "waitedg",0x0d602a24, NO_OPERANDS, OPC_GENERIC },
  { "waitrdl",0x0d602c24, NO_OPERANDS, OPC_GENERIC },
  { "waitwrl",0x0d602e24, NO_OPERANDS, OPC_GENERIC },
  { "waithlk",0x0d603024, NO_OPERANDS, OPC_GENERIC },
  { "waitxmt",0x0d603224, NO_OPERANDS, OPC_GENERIC },
  { "waitxfi",0x0d603424, NO_OPERANDS, OPC_GENERIC },
  { "waitxro",0x0d603624, NO_OPERANDS, OPC_GENERIC },
  { "waitfbw",0x0d603824, NO_OPERANDS, OPC_GENERIC },
  { "waitrle",0x0d603a24, NO_OPERANDS, OPC_GENERIC },

  { "allowi", 0x0d604024, NO_OPERANDS, OPC_GENERIC },
  { "stalli", 0x0d604224, NO_OPERANDS, OPC_GENERIC },

  { "setint1",0x0d600025, P2_DST_CONST_OK, OPC_GENERIC },
  { "setint2",0x0d600026, P2_DST_CONST_OK, OPC_GENERIC },
  { "setint3",0x0d600027, P2_DST_CONST_OK, OPC_GENERIC },
  { "waitx",  0x0d600028, P2_DST_CONST_OK, OPC_GENERIC },
  { "setcz",  0x0d600029, P2_DST_CONST_OK, OPC_GENERIC },
  { "push",   0x0d60002a, P2_DST_CONST_OK, OPC_GENERIC },
  { "pop",    0x0d60002b, DST_OPERAND_ONLY, OPC_GENERIC },
  
  // long jumps
  { "jmp" ,   0x0d800000, P2_JUMP, OPC_JUMP },
  { "call",   0x0da00000, P2_JUMP, OPC_GENERIC_BRANCH },
  { "calla",  0x0dc00000, P2_JUMP, OPC_CALL },
  { "callb",  0x0de00000, P2_JUMP, OPC_GENERIC_BRANCH },
  
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
    { "adra", 0x1f6, "ADRA" },
    { "adrb", 0x1f7, "ADRB" },
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

#define IF_NEVER_P1 0xffc3ffff
#define IF_NEVER_P2 0x0fffffff

InstrModifier modifiers_p1[] = {
    { "if_never",  IF_NEVER_P1 },
    { "if_always", IF_NEVER_P1 | (0xf<<18) },

    { "if_a",           IF_NEVER_P1 | (0x1<<18) },
    { "if_nc_and_nz",   IF_NEVER_P1 | (0x1<<18) },
    { "if_nz_and_nc",   IF_NEVER_P1 | (0x1<<18) },

    { "if_nc_and_z",    IF_NEVER_P1 | (0x2<<18) },
    { "if_z_and_nc",    IF_NEVER_P1 | (0x2<<18) },

    { "if_ae",     IF_NEVER_P1 | (0x3<<18) },
    { "if_nc",     IF_NEVER_P1 | (0x3<<18) },

    { "if_c_and_nz",    IF_NEVER_P1 | (0x4<<18) },
    { "if_nz_and_c",    IF_NEVER_P1 | (0x4<<18) },

    { "if_ne",     IF_NEVER_P1 | (0x5<<18) },
    { "if_nz",     IF_NEVER_P1 | (0x5<<18) },

    { "if_c_ne_z", IF_NEVER_P1 | (0x6<<18) },
    { "if_z_ne_c", IF_NEVER_P1 | (0x6<<18) },

    { "if_nc_or_nz", IF_NEVER_P1 | (0x7<<18) },
    { "if_nz_or_nc", IF_NEVER_P1 | (0x7<<18) },

    { "if_c_and_z", IF_NEVER_P1 | (0x8<<18) },
    { "if_z_and_c", IF_NEVER_P1 | (0x8<<18) },

    { "if_c_eq_z", IF_NEVER_P1 | (0x9<<18) },
    { "if_z_eq_c", IF_NEVER_P1 | (0x9<<18) },

    { "if_e",      IF_NEVER_P1 | (0xa<<18) },
    { "if_z",      IF_NEVER_P1 | (0xa<<18) },

    { "if_nc_or_z", IF_NEVER_P1 | (0xb<<18) },
    { "if_z_or_nc", IF_NEVER_P1 | (0xb<<18) },

    { "if_b",      IF_NEVER_P1 | (0xc<<18) },
    { "if_c",      IF_NEVER_P1 | (0xc<<18) },

    { "if_c_or_nz", IF_NEVER_P1 | (0xd<<18) },
    { "if_nz_or_c", IF_NEVER_P1 | (0xd<<18) },

    { "if_be",     IF_NEVER_P1 | (0xe<<18) },
    { "if_c_or_z", IF_NEVER_P1 | (0xe<<18) },
    { "if_z_or_c", IF_NEVER_P1 | (0xe<<18) },


    { "wz", (1<<25) },
    { "wc", (1<<24) },
    { "wr", (1<<23) },
    { "nr", ~(1<<23) },

    { NULL, 0 }
};

InstrModifier modifiers_p2[] = {
    { "if_never",  IF_NEVER_P2 },
    { "if_always", IF_NEVER_P2 | (0xf<<28) },

    { "if_a",           IF_NEVER_P2 | (0x1<<28) },
    { "if_nc_and_nz",   IF_NEVER_P2 | (0x1<<28) },
    { "if_nz_and_nc",   IF_NEVER_P2 | (0x1<<28) },

    { "if_nc_and_z",    IF_NEVER_P2 | (0x2<<28) },
    { "if_z_and_nc",    IF_NEVER_P2 | (0x2<<28) },

    { "if_ae",     IF_NEVER_P2 | (0x3<<28) },
    { "if_nc",     IF_NEVER_P2 | (0x3<<28) },

    { "if_c_and_nz",    IF_NEVER_P2 | (0x4<<28) },
    { "if_nz_and_c",    IF_NEVER_P2 | (0x4<<28) },

    { "if_ne",     IF_NEVER_P2 | (0x5<<28) },
    { "if_nz",     IF_NEVER_P2 | (0x5<<28) },

    { "if_c_ne_z", IF_NEVER_P2 | (0x6<<28) },
    { "if_z_ne_c", IF_NEVER_P2 | (0x6<<28) },

    { "if_nc_or_nz", IF_NEVER_P2 | (0x7<<28) },
    { "if_nz_or_nc", IF_NEVER_P2 | (0x7<<28) },

    { "if_c_and_z", IF_NEVER_P2 | (0x8<<28) },
    { "if_z_and_c", IF_NEVER_P2 | (0x8<<28) },

    { "if_c_eq_z", IF_NEVER_P2 | (0x9<<28) },
    { "if_z_eq_c", IF_NEVER_P2 | (0x9<<28) },

    { "if_e",      IF_NEVER_P2 | (0xa<<28) },
    { "if_z",      IF_NEVER_P2 | (0xa<<28) },

    { "if_nc_or_z", IF_NEVER_P2 | (0xb<<28) },
    { "if_z_or_nc", IF_NEVER_P2 | (0xb<<28) },

    { "if_b",      IF_NEVER_P2 | (0xc<<28) },
    { "if_c",      IF_NEVER_P2 | (0xc<<28) },

    { "if_c_or_nz", IF_NEVER_P2 | (0xd<<28) },
    { "if_nz_or_c", IF_NEVER_P2 | (0xd<<28) },

    { "if_be",     IF_NEVER_P2 | (0xe<<28) },
    { "if_c_or_z", IF_NEVER_P2 | (0xe<<28) },
    { "if_z_or_c", IF_NEVER_P2 | (0xe<<28) },


    { "wz", (1<<19) },
    { "wc", (1<<20) },

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
        AddSymbol(&reservedWords, hwreg[i].name, SYM_HWREG, (void *)&hwreg[i]);
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
