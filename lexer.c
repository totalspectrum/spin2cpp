//
// Simple lexical analyzer for a language where indentation
// is significant
//
// Copyright (c) 2011-2014 Total Spectrum Software Inc.
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
#include "flexbuf.h"

/* flag: if set, run the  preprocessor */
int gl_preprocess = 1;

extern int gl_prop2;

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
    if (L->ungot_ptr) {
        --L->ungot_ptr;
        return L->ungot[L->ungot_ptr];
    }
    if (L->pendingLine) {
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
    return c;
}

void
lexungetc(LexStream *L, int c)
{
    if (L->ungot_ptr == UNGET_MAX-1) {
        fprintf(stderr, "ERROR: unget limit exceeded\n");
    }
    L->ungot[L->ungot_ptr++] = c;
}


//
// establish an indent level
// if the line is indented more than this, a T_INDENT will
// be emitted
//
void
EstablishIndent(LexStream *L, int level)
{
    AST *dummy = NewAST(0, NULL, NULL);
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
    if (L->in_block == T_DAT) {
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
                ast = GetComments();
                //EstablishIndent(L, 1);
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
    while (c != '"') {
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

        flexbuf_init(&anno, INCSTR);
        commentNest = 1;
        /* check for special comments {++... } which indicate 
           C++ annotations
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


static char operator_chars[] = "-+*/|<>=!@~#^.";

int
getToken(LexStream *L, AST **ast_ptr)
{
//    int base = 10;
    int c;
    AST *ast = NULL;
    int at_startofline = (L->eoln == 1);

    c = skipSpace(L, &ast);

    if (c >= 127) {
        *ast_ptr = ast;
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
    { "abort", T_ABORT },
    { "and", T_AND },
    { "byte", T_BYTE },

    { "case", T_CASE },
    { "con", T_CON },
    { "constant", T_CONSTANT },

    { "dat", T_DAT },

    { "else", T_ELSE },
    { "elseif", T_ELSEIF },
    { "elseifnot", T_ELSEIFNOT },

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
    { "other", T_OTHER },

    { "quit", T_QUIT },
    { "pri", T_PRI },
    { "pub", T_PUB },
    { "repeat", T_REPEAT },
    { "res", T_RES },
    { "result", T_RESULT },
    { "return", T_RETURN },
    { "round", T_ROUND },

    { "step", T_STEP },
    { "string", T_STRINGPTR },
    { "to", T_TO },
    { "trunc", T_TRUNC },

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
    "malloc"
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

extern void defaultBuiltin(FILE *, Builtin *, AST *);
extern void defaultVariable(FILE *, Builtin *, AST *);
extern void memBuiltin(FILE *, Builtin *, AST *);
extern void memFillBuiltin(FILE *, Builtin *, AST *);
extern void str1Builtin(FILE *, Builtin *, AST *);
extern void strcompBuiltin(FILE *, Builtin *, AST *);
extern void waitBuiltin(FILE *, Builtin *, AST *);
extern void rebootBuiltin(FILE *, Builtin *, AST *);

/* hooks to be called when we recognize a builtin */
static void lockhook(Builtin *dummy) { current->needsLockFuncs = 1; }

Builtin builtinfuncs[] = {
    { "clkfreq", 0, defaultVariable, "CLKFREQ", NULL },
    { "clkmode", 0, defaultVariable, "CLKMODE", NULL },
    { "clkset", 2, defaultBuiltin, "clkset", NULL },

    { "cognew", 2, defaultBuiltin, "cognew", NULL },
    { "cogstop", 1, defaultBuiltin, "cogstop", NULL },
    { "cogid", 0, defaultBuiltin, "cogid", NULL },

    { "coginit", 3, defaultBuiltin, "coginit", NULL },

    { "locknew", 0, defaultBuiltin, "locknew", lockhook },
    { "lockset", 1, defaultBuiltin, "lockset", lockhook },
    { "lockclr", 1, defaultBuiltin, "lockclr", lockhook },
    { "lockret", 1, defaultBuiltin, "lockret", lockhook },

    { "strsize", 1, str1Builtin, "strlen", NULL },
    { "strcomp", 1, strcompBuiltin, "strcmp", NULL },
    { "waitcnt", 1, defaultBuiltin, "waitcnt", NULL },

    { "waitpne", 3, waitBuiltin, "waitpne", NULL },
    { "waitpeq", 3, waitBuiltin, "waitpeq", NULL },

    { "reboot", 0, rebootBuiltin, "reboot", NULL },

    { "longfill", 4, memFillBuiltin, "memset", NULL },
    { "longmove", 4, memBuiltin, "memmove", NULL },
    { "wordfill", 2, memFillBuiltin, "memset", NULL },
    { "wordmove", 2, memBuiltin, "memmove", NULL },
    { "bytefill", 1, memBuiltin, "memset", NULL },
    { "bytemove", 1, memBuiltin, "memcpy", NULL },

    { "getcnt", 0, defaultBuiltin, "getcnt", NULL },
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
initLexer(int prop2)
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
    InitPasm(prop2);
}

int
IsReservedWord(const char *name)
{
    return FindSymbol(&reservedWords, name) != 0;
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
    { "cmpsub", 0xe0800000, TWO_OPERANDS },
    { "cmpsx",  0xc4000000, TWO_OPERANDS },
    { "cmpx",   0xcc000000, TWO_OPERANDS },

    { "cogid",   0x0cc00001, DST_OPERAND_ONLY },
    { "coginit", 0x0c400002, DST_OPERAND_ONLY },
    { "cogstop", 0x0c400003, DST_OPERAND_ONLY },

    { "djnz",   0xe4800000, JMPRET_OPERANDS },
    { "hubop",  0x0c000000, TWO_OPERANDS },
    { "jmp",    0x5c000000, SRC_OPERAND_ONLY },
    { "jmpret", 0x5c800000, JMPRET_OPERANDS },

    { "lockclr",0x0c400007, DST_OPERAND_ONLY },
    { "locknew",0x0cc00004, DST_OPERAND_ONLY },
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
    { "nop",    0x00000000, NOP_OPERANDS },

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
    { "subabs", 0x8c800000, TWO_OPERANDS },
    { "subs",   0xc0800000, TWO_OPERANDS },
    { "subsx",  0xc4800000, TWO_OPERANDS },
    { "subx",   0xcc800000, TWO_OPERANDS },
    { "sumc",   0x90800000, TWO_OPERANDS },
    { "sumnc",  0x94800000, TWO_OPERANDS },
    { "sumnz",  0x9c800000, TWO_OPERANDS },
    { "sumz",   0x98800000, TWO_OPERANDS },

    { "test",   0x60000000, TWO_OPERANDS },
    { "testn",  0x64000000, TWO_OPERANDS },
    { "tjnz",   0xe8000000, JMPRET_OPERANDS },
    { "tjz",    0xec000000, JMPRET_OPERANDS },

    { "waitcnt", 0xf8800000, TWO_OPERANDS },
    { "waitpeq", 0xf0000000, TWO_OPERANDS },
    { "waitpne", 0xf4000000, TWO_OPERANDS },
    { "waitvid", 0xfc000000, TWO_OPERANDS },

    { "wrbyte", 0x00000000, TWO_OPERANDS },
    { "wrlong", 0x08000000, TWO_OPERANDS },
    { "wrword", 0x04000000, TWO_OPERANDS },

    { "xor",    0x6c800000, TWO_OPERANDS },
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

};

HwReg hwreg_p2[] = {
    { "pina", 0x1f8, "PINA" },
    { "pinb", 0x1f9, "PINB" },
    { "pinc", 0x1fa, "PINC" },
    { "pind", 0x1fb, "PIND" },
    { "dira", 0x1fc, "DIRA" },
    { "dirb", 0x1fd, "DIRB" },
    { "dirc", 0x1fe, "DIRC" },
    { "dird", 0x1ff, "DIRD" },

};

#define IF_NEVER 0xffc3ffff

InstrModifier modifiers[] = {
    { "if_always", IF_NEVER | (0xf<<18) },
    { "if_never",  IF_NEVER },

    { "if_a",           IF_NEVER | (0x1<<18) },
    { "if_nc_and_nz",   IF_NEVER | (0x1<<18) },
    { "if_nz_and_nc",   IF_NEVER | (0x1<<18) },

    { "if_nc_and_z",    IF_NEVER | (0x2<<18) },
    { "if_z_and_nc",    IF_NEVER | (0x2<<18) },

    { "if_ae",     IF_NEVER | (0x3<<18) },
    { "if_nc",     IF_NEVER | (0x3<<18) },

    { "if_c_and_nz",    IF_NEVER | (0x4<<18) },
    { "if_nz_and_c",    IF_NEVER | (0x4<<18) },

    { "if_ne",     IF_NEVER | (0x5<<18) },
    { "if_nz",     IF_NEVER | (0x5<<18) },

    { "if_c_ne_z", IF_NEVER | (0x6<<18) },
    { "if_z_ne_c", IF_NEVER | (0x6<<18) },

    { "if_nc_or_nz", IF_NEVER | (0x7<<18) },
    { "if_nz_or_nc", IF_NEVER | (0x7<<18) },

    { "if_c_and_z", IF_NEVER | (0x8<<18) },
    { "if_z_and_c", IF_NEVER | (0x8<<18) },

    { "if_c_eq_z", IF_NEVER | (0x9<<18) },
    { "if_z_eq_c", IF_NEVER | (0x9<<18) },

    { "if_e",      IF_NEVER | (0xa<<18) },
    { "if_z",      IF_NEVER | (0xa<<18) },

    { "if_nc_or_z", IF_NEVER | (0xb<<18) },
    { "if_z_or_nc", IF_NEVER | (0xb<<18) },

    { "if_b",      IF_NEVER | (0xc<<18) },
    { "if_c",      IF_NEVER | (0xc<<18) },

    { "if_c_or_nz", IF_NEVER | (0xd<<18) },
    { "if_nz_or_c", IF_NEVER | (0xd<<18) },

    { "if_be",     IF_NEVER | (0xe<<18) },
    { "if_c_or_z", IF_NEVER | (0xe<<18) },
    { "if_z_or_c", IF_NEVER | (0xe<<18) },


    { "wz", (1<<25) },
    { "wc", (1<<24) },
    { "wr", (1<<23) },
    { "nr", ~(1<<23) }

};

void
InitPasm(int prop2)
{
    HwReg *hwreg;
    int cnt, i;

    if (prop2) {
	    hwreg = hwreg_p2;
	    cnt = N_ELEMENTS(hwreg_p2);
    }
    else {
	    hwreg = hwreg_p1;
	    cnt = N_ELEMENTS(hwreg_p1);
    }

    /* add hardware registers */
    for (i = 0; i < cnt; i++) {
        AddSymbol(&reservedWords, hwreg[i].name, SYM_HWREG, (void *)&hwreg[i]);
    }

    /* add instructions */
    for (i = 0; i < N_ELEMENTS(instr); i++) {
        AddSymbol(&pasmWords, instr[i].name, SYM_INSTR, (void *)&instr[i]);
    }

    /* instruction modifiers */
    for (i = 0; i < N_ELEMENTS(modifiers); i++) {
        AddSymbol(&pasmWords, modifiers[i].name, SYM_INSTRMODIFIER, (void *)&modifiers[i]);
    }

}


/*
 * "canonicalize" an identifier, to make sure it
 * does not conflict with any C reserved words
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
    for (ptr = name; *ptr; ptr++)
        ;
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
        } else {
            *ptr = tolower(*ptr);
        }
        ptr++;
    }
}
