//
// BRK debugger bytecode stuffs
//
// See the debugger code for more insight.
//
// Copyright 2021-2023 Ada Gottensträter and Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include "spinc.h"
#include "becommon.h"
#include <stdlib.h>

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
    DBC_TYPE_STR = 0x20 | DBC_SIZE_BYTE, 
    DBC_TYPE_FLP = 0x20, // Note the overlap with the signed flag and the string type
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

    {"fdec"             ,DBC_TYPE_FLP|DBC_SIZE_LONG},
    {"fdec_reg_array"   ,DBC_TYPE_FLP              |DBC_FLAG_ARRAY},
    {"fdec_array"       ,DBC_TYPE_FLP|DBC_SIZE_LONG|DBC_FLAG_ARRAY},

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

    {"uchar#"           ,DBC_CHAR},

    {NULL,0}
};

unsigned brkAssigned = 1; // Currently assigned BRK codes (zero is internally reserved)
static Flexbuf brkExpr[MAX_BRK];

static void
emitAsmConstant(Flexbuf *f,uint32_t val) {
    if (val < 0x4000) {
        DEBUG(NULL,"Emitting short constant arg %d",val);
        flexbuf_putc(val>>8,f);
        flexbuf_putc(val&255,f);
    } else {
        DEBUG(NULL,"Emitting long constant arg %d",val);
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
    DEBUG(NULL,"Emitting register arg %d",reg);
    flexbuf_putc(128|(reg>>8),f);
    flexbuf_putc(reg&255,f);
}

int Pasm_DebugEval(AST *arg, int argNum, int *addr, void *ignored) {
    if (arg->kind == AST_IMMHOLDER) {
        *addr = EvalPasmExpr(arg->left);
        return PASM_EVAL_ISCONST;
    }
    *addr = EvalPasmExpr(arg);
    return PASM_EVAL_ISREG;
}

int AsmDebug_CodeGen(AST *ast, BackendDebugEval evalFunc, void *evalArg) {

    /* check for DEBUG_DISABLE */
    if (const_or_default(current, "DEBUG_DISABLE", 0) != 0) {
        return -1;
    }


    ASSERT_AST_KIND(ast,AST_BRKDEBUG,return -1;);
    if (!ast->left) {
        // if no exprlist, this is a paren-less DEBUG and should trigger the interactive debugger
        return 0;
    }
    ASSERT_AST_KIND(ast->left,AST_EXPRLIST,return -1;);

    unsigned brkCode = brkAssigned++;
    AST *exprbase;
    int regNum = 0;
    bool do_cogn = false;

    /* sanity check on codes */
    if (brkCode >= MAX_BRK) {
        ERROR(ast,"MAX_BRK exceeded!");
        return -1;
    }

    Flexbuf *f = &brkExpr[brkCode];
    flexbuf_init(f,64);

    if (ast->left->left->kind == AST_LABEL) {
        // the parser inserts a LABEL token if a COGn label is
        // required
        exprbase = ast->left->right;
        do_cogn = true;
    } else {
        exprbase = ast->left;
    }
    ASSERT_AST_KIND(exprbase,AST_EXPRLIST,return -1;);

    // Add default stuff
    flexbuf_putc(DBC_ASMMODE,f);
    if (do_cogn) flexbuf_putc(DBC_COGN,f);

    bool needcomma = false;

    DEBUG(ast,"Building DEBUG code for BRK #%d",brkCode);

    for (AST *exprlist = exprbase;exprlist;exprlist=exprlist->right) {
        AST *item = exprlist->left;
        if(!item) {
            WARNING(exprlist,"NULL item in debug exprlist?!?!");
            continue;
        }
        if (item->kind == AST_EXPRLIST && !item->right) {
            // single item expression list is OK
            item = item->left;
        }
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
                if (gotArgs == 0) {
                    flexbuf_putc(opcode,f);
                    DEBUG(item,"Emitting DEBUG opcode %02X",opcode);
                }
                gotArgs++;
                AST *arg = arglist->left;
                int addrKind, addr;
                
                if (gotArgs==1 && !simple && !noExpr) {
                    // TOOD: Yoink expression string from source buffer
                    AST *exprAst = arg;
                    if (exprAst->kind==AST_IMMHOLDER) {
                        flexbuf_putc('#',f);
                        exprAst = exprAst->left;
                    }
                    if (IsIdentifier(exprAst)) {
                        const char *expr = GetUserIdentifierName(exprAst);
                        flexbuf_addstr(f,expr);
                    } else {
                        PrintExpr(f, exprAst, PRINTEXPR_DEBUG);
                    }
                    flexbuf_putc(0,f);
                }

                addrKind = (*evalFunc)(arg, regNum, &addr, evalArg);
                switch (addrKind) {
                case PASM_EVAL_ISCONST:
                    emitAsmConstant(f, addr);
                    break;
                case PASM_EVAL_ISREG:
                    emitAsmRegref(f, addr);
                    regNum++;
                    break;
                case PASM_EVAL_ISREG_2:
                case PASM_EVAL_ISREG_3:
                case PASM_EVAL_ISREG_4:
                    emitAsmRegref(f, addr);
                    regNum++;
                    addr++;
                    for (int n = PASM_EVAL_ISREG; n < addrKind; n++) {
                        if (gotArgs == expectedArgs) {
                            opcode &= ~DBC_FLAG_NOCOMMA;
                            flexbuf_putc(opcode|DBC_FLAG_NOEXPR, f);
                            gotArgs = 1;
                        } else {
                            gotArgs++;
                        }
                        emitAsmRegref(f, addr);
                        addr++; regNum++;
                    }
                    needcomma = true;
                    break;
                default:
                    ERROR(arg, "Unknown address kind returned by back end");
                    break;
                }
                if (gotArgs == expectedArgs) {
                    // consumed them all
                    gotArgs = 0;
                    needcomma = true;
                    opcode &= ~DBC_FLAG_NOCOMMA;
                }
            }
            if (gotArgs) {
                ERROR(item,"%s expects %d args, got %d",name,expectedArgs,gotArgs);
            }
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

#include "sys/p2_brkdebug.spin.h"

extern int spinyyparse(void); // Very cool

static void patch_long(char *where,int32_t val) {
    *where++ = (val>>0)&255;
    *where++ = (val>>8)&255;
    *where++ = (val>>16)&255;
    *where++ = (val>>24)&255;
}


Flexbuf CompileBrkDebugger(size_t appsize) {
    Flexbuf f;
    flexbuf_init(&f,16*1024);

    if (!gl_p2) ERROR(NULL,"BRK debug is only available on P2");

    // Get stuff
    Module *T = GetTopLevelModule();
    uint32_t clkfreq = const_or_default(T,"__clkfreq_con",10000000);
    uint32_t clkmode = const_or_default(T,"__clkmode_con",0);
    DEBUG(NULL,"Debugger gets CLKMODE %08X and CLKFREQ %d",clkmode,clkfreq);
    uint32_t millisecond = (clkfreq/1000)-6; // This is how PNut calculates it...
    ////uint32_t txmode = ((clkfreq/(const_or_default(T,"DEBUG_BAUD",2000000)>>6))<<(16-6))|(8-1); // Also from PNut
    if (!gl_default_baud) gl_default_baud = 2000000;
    //uint32_t txmode = ((clkfreq/(const_or_default(T,"DEBUG_BAUD",gl_default_baud)>>6))<<(16-6))|(8-1); // Also from PNut

    // Compile debugger blob
    int old_errors = gl_errors;
    gl_errors = 0; // Have to do this to not segfault when there've been errors
    gl_caseSensitive = false;
    Module *D = NewModule("__brkdebug__",LANG_SPIN_SPIN2);
    current = D;
    D->Lptr = (LexStream *)calloc(sizeof(*systemModule->Lptr), 1);
    D->Lptr->flags |= LEXSTREAM_FLAG_NOSRC;
    strToLex(D->Lptr, (const char *)sys_p2_brkdebug_spin, "__brkdebug__", LANG_SPIN_SPIN2);
    spinyyparse();
    ProcessModule(D);
    // We good now?
    PrintDataBlock(&f,D->datblock,NULL,NULL);
    gl_errors = old_errors;
    // Patch parameters (ugly hardcoded offsets!)
    int txpin = const_or_default(T,"DEBUG_PIN_TX",const_or_default(T,"DEBUG_PIN",62));
    int rxpin = const_or_default(T,"DEBUG_PIN_RX",63);
    {
        char *buf = flexbuf_peek(&f);
        patch_long(buf+0x0D4,clkfreq);
        patch_long(buf+0x0D8,clkmode&~3); // Clock mode with RCFAST
        patch_long(buf+0x0DC,clkmode); // Clock mode
        patch_long(buf+0x0E0,const_or_default(T,"DEBUG_DELAY",0)*millisecond); // Debug delay
        patch_long(buf+0x0E4,appsize); // Application size
        patch_long(buf+0x0E8,(const_or_default(T,"DEBUG_COGS",0xFF)&255)|0x20030000); // Enabled cogs (and something idk)
        patch_long(buf+0x140,txpin); // TX Pin
        patch_long(buf+0x144,rxpin|(FindSymbol(&T->objsyms,"DEBUG_TIMESTAMP") ? 1u<<31 : 0)); // RX Pin and timestamp flag
        patch_long(buf+0x148,const_or_default(T,"DEBUG_BAUD",gl_default_baud));

    }

    // Build the actual data table
    Flexbuf tab;
    flexbuf_init(&tab,16*1024);
    // Build offsets first
    unsigned pos = (brkAssigned)*2;
    for (unsigned i=1;i<brkAssigned;i++) {
        flexbuf_putc((pos>>0)&255,&tab);
        flexbuf_putc((pos>>8)&255,&tab);
        pos += flexbuf_curlen(&brkExpr[i]);
    }
    // Now copy the bytecode
    for (unsigned i=1;i<brkAssigned;i++) {
        flexbuf_concat(&tab,&brkExpr[i]);
    }

    // Append table
    size_t dataLen = flexbuf_curlen(&tab) + 2;
    if (dataLen+0xFC000 > 0xFEC00) ERROR(NULL,"BRK debug data too big!");
    flexbuf_putc((dataLen>>0)&255,&f);
    flexbuf_putc((dataLen>>8)&255,&f);
    flexbuf_concat(&f,&tab);
    flexbuf_delete(&tab);

    return f;
}

