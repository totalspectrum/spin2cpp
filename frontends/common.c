/*
 * Spin to C/C++ translator
 * Copyright 2011-2018 Total Spectrum Software Inc.
 * 
 * +--------------------------------------------------------------------
 * ¦  TERMS OF USE: MIT License
 * +--------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * +--------------------------------------------------------------------
 */
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "common.h"
#include "preprocess.h"
#include "version.h"

/* declarations common to all front ends */

Module *current;
Module *allparse;
Module *globalModule;
SymbolTable *currentTypes;

int gl_p2;
int gl_errors;
int gl_output;
int gl_outputflags;
int gl_nospin;
int gl_gas_dat;
int gl_normalizeIdents;
int gl_debug;
int gl_expand_constants;
int gl_optimize_flags;
int gl_dat_offset;
int gl_printprogress = 0;
int gl_infer_ctypes = 0;
int gl_listing = 0;
int gl_fixedreal = 0;

// bytes and words are unsigned by default, but longs are signed by default
// this is confusing, and someday we should make all of these explicit
// but it's a legacy from Spin
AST *ast_type_word, *ast_type_long, *ast_type_byte;
AST *ast_type_signed_word, *ast_type_signed_byte;
AST *ast_type_unsigned_long;
AST *ast_type_float, *ast_type_string;
AST *ast_type_ptr_long;
AST *ast_type_ptr_word;
AST *ast_type_ptr_byte;
AST *ast_type_ptr_void;
AST *ast_type_generic;
AST *ast_type_void;

const char *gl_progname = "spin2cpp";
char *gl_header1 = NULL;
char *gl_header2 = NULL;

/*
 * allocate a new parser state
 */ 
Module *
NewModule(const char *fullname, int language)
{
    Module *P;
    char *s;
    char *root;

    P = (Module *)calloc(1, sizeof(*P));
    if (!P) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    P->lastLanguage = language;
    
    /* set up the base file name */
    P->fullname = fullname;
    P->basename = strdup(fullname);
    s = strrchr(P->basename, '.');
    if (s) *s = 0;
    root = strrchr(P->basename, '/');
#if defined(WIN32) || defined(WIN64)
    if (!root) {
      root = strrchr(P->basename, '\\');
    }
#endif
    if (!root)
      root = P->basename;
    else
      P->basename = root+1;
    /* set up the class name */
    P->classname = (char *)calloc(1, strlen(P->basename)+1);
    strcpy(P->classname, P->basename);

    /* link the global symbols */
    if (globalModule) {
        P->objsyms.next = &globalModule->objsyms;
    } else if (language == LANG_BASIC) {
        P->objsyms.next = &basicReservedWords;
    } else {
        P->objsyms.next = &spinReservedWords;
    }
    P->body = NULL;
    return P;
}

/*
 * declare constant symbols
 */
static Symbol *
EnterConstant(const char *name, AST *expr)
{
    Symbol *sym;

    if (IsFloatConst(expr)) {
        sym = AddSymbol(&current->objsyms, name, SYM_FLOAT_CONSTANT, (void *)expr);
    } else {
        sym = AddSymbol(&current->objsyms, name, SYM_CONSTANT, (void *)expr);
    }
    return sym;
}

void
DeclareConstants(AST **conlist_ptr)
{
    AST *conlist;
    AST *upper, *ast, *id;
    AST *next;
    int default_val;
    int default_val_ok = 0;
    int n;

    conlist = *conlist_ptr;

    // first do all the simple assignments
    // this is necessary because Spin will sometimes allow out-of-order
    // assignments

    do {
        n = 0; // no assignments yet
        default_val = 0;
        default_val_ok = 1;
        upper = conlist;
        while (upper) {
            next = upper->right;
            if (upper->kind == AST_LISTHOLDER) {
                ast = upper->left;
                if (ast->kind == AST_COMMENTEDNODE)
                    ast = ast->left;

                switch (ast->kind) {
                case AST_ASSIGN:
                    if (IsConstExpr(ast->right)) {
                        EnterConstant(ast->left->d.string, ast->right);
                        n++;
                        // now pull the assignment out so we don't see it again
                        RemoveFromList(conlist_ptr, upper);
                        upper->right = *conlist_ptr;
                        *conlist_ptr = upper;
                        conlist_ptr = &upper->right;
                        conlist = *conlist_ptr;
                    }
                    break;
                case AST_ENUMSET:
                    if (IsConstExpr(ast->left)) {
                        default_val = EvalConstExpr(ast->left);
                        default_val_ok = 1;
                    } else {
                        default_val_ok = 0;
                    }
                    break;
                case AST_ENUMSKIP:
                    if (default_val_ok) {
                        id = ast->left;
                        if (id->kind != AST_IDENTIFIER) {
                            ERROR(ast, "Internal error, expected identifier in constant list");
                        } else {
                            EnterConstant(id->d.string, AstInteger(default_val));
                            default_val += EvalConstExpr(ast->right);
                        }
                        n++;
                        // now pull the assignment out so we don't see it again
                        RemoveFromList(conlist_ptr, upper);
                        upper->right = *conlist_ptr;
                        *conlist_ptr = upper;
                        conlist_ptr = &upper->right;
                        conlist = *conlist_ptr;
                    }
                    break;
                case AST_IDENTIFIER:
                    if (default_val_ok) {
                        EnterConstant(ast->d.string, AstInteger(default_val));
                        default_val++;
                        n++;
                        // now pull the assignment out so we don't see it again
                        RemoveFromList(conlist_ptr, upper);
                        upper->right = *conlist_ptr;
                        *conlist_ptr = upper;
                        conlist_ptr = &upper->right;
                        conlist = *conlist_ptr;
                    }
                    break;
                default:
                    /* do nothing */
                    break;
                }
            }
            upper = next;
        }

    } while (n > 0);

    default_val = 0;
    default_val_ok = 1;
    for (upper = conlist; upper; upper = upper->right) {
        if (upper->kind == AST_LISTHOLDER) {
            ast = upper->left;
            if (ast->kind == AST_COMMENTEDNODE)
                ast = ast->left;
            switch (ast->kind) {
            case AST_ENUMSET:
                default_val = EvalConstExpr(ast->left);
                break;
            case AST_IDENTIFIER:
                EnterConstant(ast->d.string, AstInteger(default_val));
                default_val++;
                break;
            case AST_ENUMSKIP:
                id = ast->left;
                if (id->kind != AST_IDENTIFIER) {
                    ERROR(ast, "Internal error, expected identifier in constant list");
                } else {
                    EnterConstant(id->d.string, AstInteger(default_val));
                    default_val += EvalConstExpr(ast->right);
                }
                break;
            case AST_ASSIGN:
                EnterConstant(ast->left->d.string, ast->right);
                default_val = EvalConstExpr(ast->right) + 1;
                break;
            case AST_COMMENT:
                /* just skip it for now */
                break;
            default:
                ERROR(ast, "Internal error: bad AST value %d", ast->kind);
                break;
            }
        } else {
            ERROR(upper, "Expected list in constant, found %d instead", upper->kind);
        }
    }
}

#if 0
/*
 * transform AST_SRCCOMMENTs into comments with the line data of the 
 * next non-comment AST
 */
static void
TransformSrcCommentsBlock(AST *ast, AST *upper)
{
    while (ast) {
        if (ast->kind == AST_SRCCOMMENT) {
            ast->kind = AST_COMMENT;
            ast->d.string = "+++";
        }
        TransformSrcCommentsBlock(ast->left, ast);
        upper = ast;
        ast = ast->right;
    }
}
#endif

AST *
NewObject(AST *identifier, AST *string)
{
    AST *ast;
    const char *filename;

    if (string) {
        filename = string->d.string;
    } else {
        filename = NULL;
    }

    ast = NewAST(AST_OBJECT, identifier, NULL);
    if (filename) {
        ast->d.ptr = ParseFile(filename);
    }
    return ast;
}

AST *
NewAbstractObject(AST *identifier, AST *string)
{
    return NewObject( NewAST(AST_OBJDECL, identifier, 0), string );
}

void
ERRORHEADER(const char *fileName, int lineno, const char *msg)
{
    if (fileName && lineno)
        fprintf(stderr, "%s(%d) %s: ", fileName, lineno, msg);
    else
        fprintf(stderr, "%s: ", msg);

}

void
ERROR(AST *instr, const char *msg, ...)
{
    va_list args;
    LineInfo *info = GetLineInfo(instr);

    if (info)
        ERRORHEADER(info->fileName, info->lineno, "error");
    else
        ERRORHEADER(NULL, 0, "error");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    gl_errors++;
}

void
SYNTAX_ERROR(const char *msg, ...)
{
    va_list args;

    if (current)
        ERRORHEADER(current->Lptr->fileName, current->Lptr->lineCounter, "error");
    else
        ERRORHEADER(NULL, 0, "error");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    gl_errors++;
}

void
WARNING(AST *instr, const char *msg, ...)
{
    va_list args;
    LineInfo *info = GetLineInfo(instr);

    if (info)
        ERRORHEADER(info->fileName, info->lineno, "warning");
    else
        ERRORHEADER(NULL, 0, "warning: ");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void
ERROR_UNKNOWN_SYMBOL(AST *ast)
{
    const char *name;

    if (ast->kind == AST_IDENTIFIER) {
        name = ast->d.string;
    } else if (ast->kind == AST_VARARGS || ast->kind == AST_VA_START) {
        name = "__vararg";
    } else {
        name = "";
    }
    ERROR(ast, "Unknown symbol %s", name);
    // add a definition for this symbol so we don't get this error again
    if (curfunc) {
        AddLocalVariable(curfunc, ast, NULL, SYM_LOCALVAR);
    }
}

void
Init()
{
    ast_type_long = NewAST(AST_INTTYPE, AstInteger(4), NULL);
    ast_type_word = NewAST(AST_UNSIGNEDTYPE, AstInteger(2), NULL);
    ast_type_byte = NewAST(AST_UNSIGNEDTYPE, AstInteger(1), NULL);

    ast_type_unsigned_long = NewAST(AST_UNSIGNEDTYPE, AstInteger(4), NULL);
    ast_type_signed_word = NewAST(AST_INTTYPE, AstInteger(2), NULL);
    ast_type_signed_byte = NewAST(AST_INTTYPE, AstInteger(1), NULL);

    ast_type_float = NewAST(AST_FLOATTYPE, AstInteger(4), NULL);
    ast_type_generic = NewAST(AST_GENERICTYPE, AstInteger(4), NULL);
    ast_type_void = NewAST(AST_VOIDTYPE, AstInteger(0), NULL);

    ast_type_ptr_long = NewAST(AST_PTRTYPE, ast_type_long, NULL);
    ast_type_ptr_word = NewAST(AST_PTRTYPE, ast_type_word, NULL);
    ast_type_ptr_byte = NewAST(AST_PTRTYPE, ast_type_byte, NULL);
    ast_type_ptr_void = NewAST(AST_PTRTYPE, ast_type_void, NULL);

    // string is pointer to const byte
    ast_type_string = NewAST(AST_PTRTYPE, NewAST(AST_MODIFIER_CONST, ast_type_byte, NULL), NULL);

    initSpinLexer(gl_p2);

    /* fill in the global symbol table */
    InitGlobalModule();
}

const char *
FindLastDirectoryChar(const char *fname)
{
    const char *found = NULL;
    if (!fname) return NULL;
    while (*fname) {
        if (*fname == '/'
#ifdef WIN32
            || *fname == '\\'
#endif
            )
        {
            found = fname;
        }
        fname++;
    }
    return found;
}

//
// use the directory portion of "directory" (if any) and then add
// on the basename
//
char *
ReplaceDirectory(const char *basename, const char *directory)
{
  char *ret = (char *)malloc(strlen(basename) + strlen(directory) + 2);
  char *dot;
  if (!ret) {
    fprintf(stderr, "FATAL: out of memory\n");
    exit(2);
  }
  strcpy(ret, directory);
  dot = (char *)FindLastDirectoryChar(ret);
  if (dot) {
      *dot++ = '/';
  } else {
      dot = ret;
  }
  strcpy(dot, basename);
  return ret;
}

char *
ReplaceExtension(const char *basename, const char *extension)
{
  char *ret = (char *)malloc(strlen(basename) + strlen(extension) + 1);
  char *dot;
  if (!ret) {
    fprintf(stderr, "FATAL: out of memory\n");
    exit(2);
  }
  strcpy(ret, basename);
  dot = strrchr(ret, '/');
  if (!dot) dot = ret;
  dot = strrchr(dot, '.');
  if (dot) *dot = 0;
  strcat(ret, extension);
  return ret;
}

//
// add a propeller checksum to a binary file
// may also pad the image out to form a .eeprom
// image, if eepromSize is non-zero
//
int
DoPropellerChecksum(const char *fname, size_t eepromSize)
{
    FILE *f = fopen(fname, "r+b");
    unsigned char checksum = 0;
    int c, r;
    size_t len;
    size_t padbytes;

    if (gl_p2) return 0; // no checksum required
    
    if (!f) {
        fprintf(stderr, "checksum: ");
        perror(fname);
        return -1;
    }
    fseek(f, 0L, SEEK_END);
    len = ftell(f);  // find length of file
    // pad file to multiple of 4, if necessary
    padbytes = ((len + 3) & ~3) - len;
    if (padbytes) {
        while (padbytes > 0) {
            fputc(0, f);
            padbytes--;
            len++;
        }
    }
    // update header fields
    fseek(f, 8L, SEEK_SET); // seek to 16 bit vbase field
    fputc(len & 0xff, f);
    fputc( (len >> 8) & 0xff, f);
    // update dbase = vbase + 2 * sizeof(int)
    fputc( (len+8) & 0xff, f);
    fputc( ((len+8)>>8) & 0xff, f);
    // update initial program counter
    fputc( 0x18, f );
    fputc( 0x00, f );
    // update dcurr = dbase + 2 * sizeof(int)
    fputc( (len+16) & 0xff, f);
    fputc( ((len+16)>>8) & 0xff, f);

    fseek(f, 0L, SEEK_SET);
    for(;;) {
        c = fgetc(f);
        if (c < 0) break;
        checksum += (unsigned char)c;
    }
    fflush(f);
    checksum = 0x14 - checksum;
    r = fseek(f, 5L, SEEK_SET);
    if (r != 0) {
        perror("fseek");
        return -1;
    }
    //printf("writing checksum 0x%x\n", checksum);
    r = fputc(checksum, f);
    if (r < 0) {
        perror("fputc");
        return -1;
    }
    fflush(f);
    if (eepromSize && eepromSize >= len + 8) {
        fseek(f, 0L, SEEK_END);
        fputc(0xff, f); fputc(0xff, f);
        fputc(0xf9, f); fputc(0xff, f);
        fputc(0xff, f); fputc(0xff, f);
        fputc(0xf9, f); fputc(0xff, f);
        len += 8;
        while (len < eepromSize) {
            fputc(0, f);
            len++;
        }
    }
    fclose(f);
    return 0;
}

//
// check the version
//
void
CheckVersion(const char *str)
{
    int majorVersion = 0;
    int minorVersion = 0;
    int revVersion = 0;

    while (isdigit(*str)) {
        majorVersion = 10 * majorVersion + (*str - '0');
        str++;
    }
    if (*str == '.') {
        str++;
        while (isdigit(*str)) {
            minorVersion = 10 * minorVersion + (*str - '0');
            str++;
        }
    }
    if (*str == '.') {
        str++;
        while (isdigit(*str)) {
            revVersion = 10 * revVersion + (*str - '0');
            str++;
        }
    }

    if (majorVersion < VERSION_MAJOR)
        return; // everything OK
    if (majorVersion == VERSION_MAJOR) {
        if (minorVersion < VERSION_MINOR)
            return; // everything OK
        if (minorVersion == VERSION_MINOR) {
            if (revVersion <= VERSION_REV) {
                return;
            }
        }
    }
    fprintf(stderr, "ERROR: required version %d.%d.%d but current version is %s\n", majorVersion, minorVersion, revVersion, VERSIONSTR);
    exit(1);
}

AST *
NewCommentedInstr(AST *instr)
{
    AST *comment = GetComments();
    AST *ast;

    ast = NewAST(AST_INSTRHOLDER, instr, NULL);
    if (comment && (comment->d.string || comment->kind == AST_SRCCOMMENT)) {
        ast = NewAST(AST_COMMENTEDNODE, ast, comment);
    }
    return ast;
}

/* add a list element together with accumulated comments */
AST *
CommentedListHolder(AST *ast)
{
    AST *comment;

    if (!ast)
        return ast;

    comment = GetComments();

    if (comment) {
        ast = NewAST(AST_COMMENTEDNODE, ast, comment);
    }
    ast = NewAST(AST_LISTHOLDER, ast, NULL);
    return ast;
}

/* determine whether a loop needs a yield, and if so, insert one */
AST *
CheckYield(AST *body)
{
    AST *ast = body;

    if (!body)
        return AstYield();
    while (ast) {
        if (ast->left)
            return body;
        ast = ast->right;
    }
    return AddToList(body, AstYield());
}


/* push/pop current type symbols */
void PushCurrentTypes(void)
{
    SymbolTable *tab = calloc(1, sizeof(SymbolTable));

    tab->next = currentTypes;
    currentTypes = tab;
}

void PopCurrentTypes(void)
{
    if (currentTypes) {
        currentTypes = currentTypes->next;
    }
}

/* check a single declaration for typedefs */
static AST *
CheckOneDeclaration(AST *origdecl)
{
    AST *decl = origdecl;
    AST *ident;
    if (!decl) return decl;

    if (decl->kind == AST_DECLARE_ALIAS) {
        return decl;
    }
    if (decl->kind != AST_DECLARE_VAR) {
        ERROR(decl, "internal error, expected DECLARE_VAR");
        return decl;
    }
    ident = decl->right;
    decl = decl->left;
    if (!decl) {
        return decl;
    }
    if (decl->kind == AST_TYPEDEF) {
        if (ident->kind != AST_IDENTIFIER) {
            ERROR(decl, "needed identifier");
        } else {
            AddSymbol(currentTypes, ident->d.string, SYM_TYPEDEF, decl->left);
        }
        return NULL;
    }
    return origdecl;
}

/* make a declaration; if it's a type then add it to currentTypes */
AST *
MakeDeclaration(AST *origdecl)
{
    AST *decl = origdecl;
    AST *item;

    if (!decl) return decl;
    if (decl->kind != AST_STMTLIST) {
        origdecl = decl = NewAST(AST_STMTLIST, decl, NULL);
    }
    while (decl && decl->kind == AST_STMTLIST) {
        item = CheckOneDeclaration(decl->left);
        if (!item) {
            /* remove this from the list */
            decl->left = NULL;
        }
        decl = decl->right;
    }
    return origdecl;
}
