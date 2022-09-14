LMM_LOOP
    rdlong LMM_i1, __pc
    add    __pc, #4
LMM_i1
    nop
    rdlong LMM_i2, __pc
    add    __pc, #4
LMM_i2
    nop
    rdlong LMM_i3, __pc
    add    __pc, #4
LMM_i3
    nop
    rdlong LMM_i4, __pc
    add    __pc, #4
LMM_i4
    nop
    rdlong LMM_i5, __pc
    add    __pc, #4
LMM_i5
    nop
    rdlong LMM_i6, __pc
    add    __pc, #4
LMM_i6
    nop
    rdlong LMM_i7, __pc
    add    __pc, #4
LMM_i7
    nop
    rdlong LMM_i8, __pc
    add    __pc, #4
LMM_i8
    nop
LMM_jmptop
    jmp    #LMM_LOOP
__pc
    long @@@hubentry
__lr
    long 0
hubretptr
    long @@@hub_ret_to_cog
LMM_NEW_PC
    long   0
    ' fall through
LMM_CALL
    rdlong LMM_NEW_PC, __pc
    add    __pc, #4
LMM_CALL_PTR
    wrlong __pc, sp
    add    sp, #4
LMM_JUMP_PTR
    mov    __pc, LMM_NEW_PC
    jmp    #LMM_LOOP
LMM_JUMP
    rdlong __pc, __pc
    jmp    #LMM_LOOP
LMM_RET
    sub    sp, #4
    rdlong __pc, sp
    jmp    #LMM_LOOP
LMM_CALL_FROM_COG
    wrlong  hubretptr, sp
    add     sp, #4
    jmp  #LMM_LOOP
LMM_CALL_FROM_COG_ret
    ret
    
LMM_CALL_ret
LMM_CALL_PTR_ret
LMM_JUMP_ret
LMM_JUMP_PTR_ret
LMM_RET_ret
LMM_RA
    long	0
    
LMM_FCACHE_LOAD
    rdlong FCOUNT_, __pc
    add    __pc, #4
    mov    ADDR_, __pc
    sub    LMM_ADDR_, __pc
    tjz    LMM_ADDR_, #a_fcachegoaddpc
    movd   a_fcacheldlp, #LMM_FCACHE_START
    shr    FCOUNT_, #2
a_fcacheldlp
    rdlong 0-0, __pc
    add    __pc, #4
    add    a_fcacheldlp,inc_dest1
    djnz   FCOUNT_,#a_fcacheldlp
    '' add in a JMP back out of LMM
    ror    a_fcacheldlp, #9
    movd   a_fcachecopyjmp, a_fcacheldlp
    rol    a_fcacheldlp, #9
a_fcachecopyjmp
    mov    0-0, LMM_jmptop
a_fcachego
    mov    LMM_ADDR_, ADDR_
    jmpret LMM_RETREG,#LMM_FCACHE_START
a_fcachegoaddpc
    add    __pc, FCOUNT_
    jmp    #a_fcachego
LMM_FCACHE_LOAD_ret
    ret
inc_dest1
    long (1<<9)
LMM_LEAVE_CODE
    jmp LMM_RETREG
LMM_ADDR_
    long 0
ADDR_
    long 0
FCOUNT_
    long 0
