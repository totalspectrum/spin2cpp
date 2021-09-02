//
// BRK debugger bytecode stuffs
//
// See the debugger code for more insight.
//
// Copyright 2021 Ada Gottensträter and Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include "spinc.h"
#include "becommon.h"

enum DebugBytecode {

    // Simple codes
    DBC_DONE = 0,
    DBC_ASMMODE = 1, // Switches into PASM mode...
    DBC_IF = 2,
    DBC_IFNOT = 3,
    DBC_COGN = 4,
    DBC_CHAR = 5,
    DBC_STRING = 6,
    DBC_DELAY = 7,

    // Flags
    DBC_FLAG_NOCOMMA = 0x01,
    DBC_FLAG_NOEXPR  = 0x02,
    DBC_FLAG_ARRAY   = 0x10,
    DBC_FLAG_SIGNED  = 0x20,
    // Numeric sizes
    DBC_SIZE_BYTE = 0x04,
    DBC_SIZE_WORD = 0x08,
    DBC_SIZE_LONG = 0x0C,
    // Output type
    DBC_TYPE_STR = 0x20, // Note the overlap with the signed flag
    DBC_TYPE_DEC = 0x40,
    DBC_TYPE_HEX = 0x80,
    DBC_TYPE_BIN = 0xC0,

};

struct DebugFunc {
    const char *name;
    const uint8_t opcode;
};

static struct DebugFunc debug_func_table[] = {

    {"if"               ,DBC_IF},
    {"ifnot"            ,DBC_IFNOT},
    {"dly"              ,DBC_DELAY},

    {"zstr"             ,DBC_TYPE_STR},
    {"lstr"             ,DBC_TYPE_STR|DBC_FLAG_ARRAY},

    {"udec"             ,                DBC_TYPE_DEC              },
    {"udec_byte"        ,                DBC_TYPE_DEC|DBC_SIZE_BYTE},
    {"udec_word"        ,                DBC_TYPE_DEC|DBC_SIZE_WORD},
    {"udec_long"        ,                DBC_TYPE_DEC|DBC_SIZE_LONG},
    {"udec_reg_array"   ,                DBC_TYPE_DEC              |DBC_FLAG_ARRAY},
    {"udec_byte_array"  ,                DBC_TYPE_DEC|DBC_SIZE_BYTE|DBC_FLAG_ARRAY},
    {"udec_word_array"  ,                DBC_TYPE_DEC|DBC_SIZE_WORD|DBC_FLAG_ARRAY},
    {"udec_long_array"  ,                DBC_TYPE_DEC|DBC_SIZE_LONG|DBC_FLAG_ARRAY},

    {"sdec"             ,DBC_FLAG_SIGNED|DBC_TYPE_DEC},
    {"sdec_byte"        ,DBC_FLAG_SIGNED|DBC_TYPE_DEC|DBC_SIZE_BYTE},
    {"sdec_word"        ,DBC_FLAG_SIGNED|DBC_TYPE_DEC|DBC_SIZE_WORD},
    {"sdec_long"        ,DBC_FLAG_SIGNED|DBC_TYPE_DEC|DBC_SIZE_LONG},
    {"sdec_reg_array"   ,DBC_FLAG_SIGNED|DBC_TYPE_DEC              |DBC_FLAG_ARRAY},
    {"sdec_byte_array"  ,DBC_FLAG_SIGNED|DBC_TYPE_DEC|DBC_SIZE_BYTE|DBC_FLAG_ARRAY},
    {"sdec_word_array"  ,DBC_FLAG_SIGNED|DBC_TYPE_DEC|DBC_SIZE_WORD|DBC_FLAG_ARRAY},
    {"sdec_long_array"  ,DBC_FLAG_SIGNED|DBC_TYPE_DEC|DBC_SIZE_LONG|DBC_FLAG_ARRAY},

    
    {"uhex"             ,                DBC_TYPE_HEX              },
    {"uhex_byte"        ,                DBC_TYPE_HEX|DBC_SIZE_BYTE},
    {"uhex_word"        ,                DBC_TYPE_HEX|DBC_SIZE_WORD},
    {"uhex_long"        ,                DBC_TYPE_HEX|DBC_SIZE_LONG},
    {"uhex_reg_array"   ,                DBC_TYPE_HEX              |DBC_FLAG_ARRAY},
    {"uhex_byte_array"  ,                DBC_TYPE_HEX|DBC_SIZE_BYTE|DBC_FLAG_ARRAY},
    {"uhex_word_array"  ,                DBC_TYPE_HEX|DBC_SIZE_WORD|DBC_FLAG_ARRAY},
    {"uhex_long_array"  ,                DBC_TYPE_HEX|DBC_SIZE_LONG|DBC_FLAG_ARRAY},

    {"shex"             ,DBC_FLAG_SIGNED|DBC_TYPE_HEX},
    {"shex_byte"        ,DBC_FLAG_SIGNED|DBC_TYPE_HEX|DBC_SIZE_BYTE},
    {"shex_word"        ,DBC_FLAG_SIGNED|DBC_TYPE_HEX|DBC_SIZE_WORD},
    {"shex_long"        ,DBC_FLAG_SIGNED|DBC_TYPE_HEX|DBC_SIZE_LONG},
    {"shex_reg_array"   ,DBC_FLAG_SIGNED|DBC_TYPE_HEX              |DBC_FLAG_ARRAY},
    {"shex_byte_array"  ,DBC_FLAG_SIGNED|DBC_TYPE_HEX|DBC_SIZE_BYTE|DBC_FLAG_ARRAY},
    {"shex_word_array"  ,DBC_FLAG_SIGNED|DBC_TYPE_HEX|DBC_SIZE_WORD|DBC_FLAG_ARRAY},
    {"shex_long_array"  ,DBC_FLAG_SIGNED|DBC_TYPE_HEX|DBC_SIZE_LONG|DBC_FLAG_ARRAY},

    
    {"ubin"             ,                DBC_TYPE_BIN              },
    {"ubin_byte"        ,                DBC_TYPE_BIN|DBC_SIZE_BYTE},
    {"ubin_word"        ,                DBC_TYPE_BIN|DBC_SIZE_WORD},
    {"ubin_long"        ,                DBC_TYPE_BIN|DBC_SIZE_LONG},
    {"ubin_reg_array"   ,                DBC_TYPE_BIN              |DBC_FLAG_ARRAY},
    {"ubin_byte_array"  ,                DBC_TYPE_BIN|DBC_SIZE_BYTE|DBC_FLAG_ARRAY},
    {"ubin_word_array"  ,                DBC_TYPE_BIN|DBC_SIZE_WORD|DBC_FLAG_ARRAY},
    {"ubin_long_array"  ,                DBC_TYPE_BIN|DBC_SIZE_LONG|DBC_FLAG_ARRAY},

    {"sbin"             ,DBC_FLAG_SIGNED|DBC_TYPE_BIN},
    {"sbin_byte"        ,DBC_FLAG_SIGNED|DBC_TYPE_BIN|DBC_SIZE_BYTE},
    {"sbin_word"        ,DBC_FLAG_SIGNED|DBC_TYPE_BIN|DBC_SIZE_WORD},
    {"sbin_long"        ,DBC_FLAG_SIGNED|DBC_TYPE_BIN|DBC_SIZE_LONG},
    {"sbin_reg_array"   ,DBC_FLAG_SIGNED|DBC_TYPE_BIN              |DBC_FLAG_ARRAY},
    {"sbin_byte_array"  ,DBC_FLAG_SIGNED|DBC_TYPE_BIN|DBC_SIZE_BYTE|DBC_FLAG_ARRAY},
    {"sbin_word_array"  ,DBC_FLAG_SIGNED|DBC_TYPE_BIN|DBC_SIZE_WORD|DBC_FLAG_ARRAY},
    {"sbin_long_array"  ,DBC_FLAG_SIGNED|DBC_TYPE_BIN|DBC_SIZE_LONG|DBC_FLAG_ARRAY},


    {NULL,0}
};

unsigned brkAssigned = 0; // Currently assigned BRK codes (TODO: Is there a place to properly zero this on startup?)
static Flexbuf brkExpr[MAX_BRK];

static void
emitAsmConstant(Flexbuf *f,uint32_t val) {
    if (val < 0x4000) {
        flexbuf_putc(val>>8,f);
        flexbuf_putc(val&255,f);
    } else {
        flexbuf_putc(0b01000000,f);
        flexbuf_putc((val>>0)&255,f);
        flexbuf_putc((val>>8)&255,f);
        flexbuf_putc((val>>16)&255,f);
        flexbuf_putc((val>>24)&255,f);
    }
}

static void
emitAsmRegref(Flexbuf *f,unsigned reg) {
    if (reg >= 1024) ERROR(NULL,"Debug regref out of range!");
    flexbuf_putc(128|(reg>>8),f);
    flexbuf_putc(reg&255,f);
}


int AsmDebug_CodeGen(AST *ast) {
    unsigned brkCode = brkAssigned++;
    if (brkCode >= MAX_BRK) {
        ERROR(ast,"MAX_BRK exceeded!");
        return -1;
    }

    Flexbuf *f = &brkExpr[brkCode];
    flexbuf_init(f,64);

    ASSERT_AST_KIND(ast,AST_BRKDEBUG,return -1;);
    ASSERT_AST_KIND(ast->left,AST_EXPRLIST,return -1;);
    ASSERT_AST_KIND(ast->left->left,AST_LABEL,return -1;); // What is the point of this empty label?
    ASSERT_AST_KIND(ast->left->right,AST_EXPRLIST,return -1;);

    // Add default stuff
    flexbuf_putc(DBC_ASMMODE,f);
    flexbuf_putc(DBC_COGN,f);

    bool needcomma = false;

    for (AST *exprlist = ast->left->right;exprlist;exprlist=exprlist->right) {
        AST *item = exprlist->left;
        switch (item->kind) {
        case AST_STRING: {
            flexbuf_putc(DBC_STRING,f);
            flexbuf_addstr(f,item->d.string);
            flexbuf_putc(0,f);
            needcomma = false;
        } break;
        case AST_INTEGER: {
            flexbuf_putc(DBC_CHAR,f);
            emitAsmConstant(f,item->d.ival);
        } break;
        case AST_FUNCCALL: {
            ASSERT_AST_KIND(item->left,AST_IDENTIFIER,break;);
            const char *name = GetUserIdentifierName(item->left);
            DEBUG(item,"got DEBUG funcall %s",name);
            size_t nameLen = strlen(name);
            bool noExpr = name[nameLen-1] == '_';
            if (noExpr) nameLen--;
            
            struct DebugFunc *func = debug_func_table;
            for (;func->name;func++) if (strlen(func->name) <= nameLen && !strncasecmp(func->name,name,nameLen)) break;

            if (!func->name) {
                ERROR(item,"Unknown debug function %s",name);
                break;
            }
            uint8_t opcode = func->opcode;
            bool simple = !(opcode&0xE0);

            if (simple && noExpr) {
                ERROR(item,"Cannot use underscore on simple functions");
            }
            if (!simple && !needcomma) opcode |= DBC_FLAG_NOCOMMA;
            if (!simple && noExpr) opcode |= DBC_FLAG_NOEXPR;

            int expectedArgs = (func->opcode & DBC_FLAG_ARRAY) ? 2 : 1;
            int gotArgs = 0;
            ASSERT_AST_KIND(item->right,AST_EXPRLIST,break;);
            for (AST *arglist=item->right;arglist;arglist=arglist->right) {
                gotArgs++;
                AST *arg = arglist->left;
                if (arg->kind == AST_IMMHOLDER) {
                    emitAsmConstant(f,EvalPasmExpr(arg->left));
                } else {
                    emitAsmRegref(f,EvalPasmExpr(arg->left));
                }
            }
            if (gotArgs!=expectedArgs) ERROR(item,"%s expects %d args, got %d",name,expectedArgs,gotArgs);

            needcomma = true;
        } break;
        default:
            ERROR(item,"Unhandled node kind %d in DEBUG",item->kind);
            break;
        }
    }

    flexbuf_putc(DBC_DONE,f);

    return brkCode;
}



