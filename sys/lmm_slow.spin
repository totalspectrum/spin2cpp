LMM_RET
	sub	sp, #4
	rdlong	__pc, sp
	jmp	#LMM_LOOP

LMM_CALL_PTR
	wrlong	__pc, sp
	add	sp, #4
LMM_JUMP_PTR
	mov	__pc, LMM_NEW_PC
	jmp	#LMM_LOOP
LMM_CALL
	wrlong	__pc, sp
	add	sp, #4
LMM_JUMP
	rdlong	__pc, __pc
LMM_LOOP
	rdlong	instr, __pc
	add	__pc, #4
instr
	nop
	jmp	#LMM_LOOP

LMM_CALL_FROM_COG
    wrlong  hubretptr, sp
    add     sp, #4
    jmp  #LMM_LOOP
LMM_CALL_FROM_COG_ret
    ret

LMM_JUMP_ret
LMM_JUMP_PTR_ret
LMM_CALL_ret
LMM_CALL_PTR_ret
LMM_RET_ret
LMM_ra
	long	0	' return address for LMM subroutine calls
__pc	long 0
inc_dest1
	long  (1<<9)
hubretptr
    long @@@hub_ret_to_cog
LMM_NEW_PC
    long   0
