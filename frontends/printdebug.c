/*
 * Printf based debug code
 * Copyright (c) 2011-2023 Total Spectrum Software Inc.
 * See the file COPYING for terms of use.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "spinc.h"

extern AST *genPrintf(AST *);

#define DBG_BITS_DELAY 0x1000

static struct s_dbgfmt {
    const char *name;
    const char *cfmt;
    int bits;
} dbgfmt[] = {
    { "uchar#", "%c", 0 },
    { "zstr", "%s", 0 },
    { "udec", "%u", 0 },
    { "fdec", "%g", 0 },
    { "udec_byte", "%03u", 8 },
    { "udec_word", "%05u", 16 },
    { "udec_long", "%09u", 0 },
    { "sdec", "%d", 0 },
    { "sdec_byte", "%3d", -8 },
    { "sdec_word", "%5d", -16 },
    { "sdec_long", "%9d", 0 },
    { "uhex", "$%x", 0 },
    { "uhex_byte", "$%02x", 8 },
    { "uhex_word", "$%04x", 16 },
    { "uhex_long", "$%08x", 0 },
    { "ubin", "%%%b", 0 },
    { "ubin_byte", "%%%08b", 8 },
    { "ubin_word", "%%%016b", 16 },
    { "ubin_long", "%%%032b", 0 },
    { "bool", "%B", 0 },
    { "dly", "%.0s", DBG_BITS_DELAY },
    { 0, 0, 0 }
};

static AST *GetFormatForDebug(struct flexbuf *fb, const char *itemname_orig, AST *args, int needcomma)
{
    char itemname[128];
    struct s_dbgfmt *ptr = &dbgfmt[0];
    AST *arg;
    AST *outlist = NULL;
    int len;
    int output_name = 1;
    const char *idname;
    int bits = 0;
    
    len = strlen(itemname_orig);
    if (len > sizeof(itemname)-1) {
        WARNING(args, "Unhandled debug format %s", itemname_orig);
        return NULL;
    }
    strcpy(itemname, itemname_orig);
    if (itemname[len-1] == '_') {
        output_name = 0;
        itemname[len-1] = 0;
    }
    while (ptr && ptr->name) {
        if (!strcasecmp(ptr->name, itemname)) {
            break;
        }
        ptr++;
    }
    if (!ptr->name) {
        WARNING(args, "Unhandled debug format %s", itemname_orig);
        return NULL;
    }
    bits = ptr->bits;
    if (bits == DBG_BITS_DELAY) {
        output_name = 0;
    }
    // now output all the arguments
    while (args) {
        arg = args->left;
        args = args->right;
    
        if (needcomma) {
            flexbuf_addstr(fb, ", ");
        }
        if (output_name) {
            idname = GetExprString(arg);
            flexbuf_printf(fb, "%s = %s", idname, ptr->cfmt);
        } else {
            flexbuf_addstr(fb, ptr->cfmt);
        }
        needcomma = 1;
        if (bits == DBG_BITS_DELAY) {
            // cause a delay
            arg = NewAST(AST_FUNCCALL, AstIdentifier("_waitms"),
                         NewAST(AST_EXPRLIST, arg, NULL));
        } else if (bits & 0x1f) {
            if (bits < 0) {
                arg = AstOperator(K_SIGNEXTEND, arg, AstInteger(-bits));
            } else {
                arg = AstOperator(K_ZEROEXTEND, arg, AstInteger(bits));
            }
        }
        arg = NewAST(AST_EXPRLIST, arg, NULL);
        outlist = AddToList(outlist, arg);
    }
    return outlist;
}

AST *
BuildDebugList(AST *exprlist, AST *dbgmask)
{
    if (!gl_debug) {
        return NULL;
    }
    if (gl_brkdebug) {
        AST *debug = NewAST(AST_BRKDEBUG, exprlist, dbgmask);
        return debug;
    }
    return NewAST(AST_PRINTDEBUG, exprlist, dbgmask);
}

AST *
CreatePrintfDebug(AST *exprlist, AST *dbgmask)
{
    AST *outlist = NULL;
    AST *item;
    AST *printf_call;
    AST *sub;
    struct flexbuf fb;
    const char *fmtstr;
    int needcomma = 0;
    
    if (0 != const_or_default(current, "DEBUG_DISABLE", 0)) {
        return NULL;
    }
    if (dbgmask) {
        uint32_t mask = const_or_default(current, "DEBUG_MASK", -1);
        uint32_t select = (uint32_t)EvalConstExpr(dbgmask);
        if (!(mask & (1u<<select))) {
            return NULL;
        }
    }
    flexbuf_init(&fb, 1024);

    if (exprlist && exprlist->left && exprlist->left->kind == AST_LABEL) {   
        flexbuf_addstr(&fb, "Cog%d  ");
        outlist = NewAST(AST_FUNCCALL,
                         AstIdentifier("cogid"),
                         NULL);
        outlist = NewAST(AST_EXPRLIST, outlist, NULL);
        exprlist = exprlist->right;
    } else {
        outlist = NULL;
    }
    while (exprlist && exprlist->kind == AST_EXPRLIST) {
        item = exprlist->left;
        exprlist = exprlist->right;
        if (!item) continue;
        if (item->kind == AST_STRING) {
            sub = NewAST(AST_STRINGPTR, item, NULL);
            sub = NewAST(AST_EXPRLIST, sub, NULL);
            outlist = AddToList(outlist, sub);
            flexbuf_addstr(&fb, "%s");
            needcomma=0;
        } else if (item->kind == AST_FUNCCALL) {
            const char *name = GetUserIdentifierName(item->left);
            AST *newarg;
            
            item = item->right; /* the parameter list */
            newarg = GetFormatForDebug(&fb, name, item, needcomma);
            if (newarg) {
                needcomma = 1;
                outlist = AddToList(outlist, newarg);
            }
        }
    }
    flexbuf_addstr(&fb, "\r\n");
    flexbuf_addchar(&fb, 0);
    fmtstr = flexbuf_get(&fb);
    flexbuf_delete(&fb);

    sub = AstStringPtr(fmtstr);
    sub = NewAST(AST_EXPRLIST, sub, NULL);
    outlist = AddToList(sub, outlist);
    printf_call = NewAST(AST_PRINT, NULL, NULL);
    printf_call = NewAST(AST_FUNCCALL, printf_call, outlist);
    return genPrintf(printf_call);
}
