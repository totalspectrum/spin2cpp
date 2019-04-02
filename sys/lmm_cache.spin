#define LMM_CACHE_SIZE 64

	''
	'' caching LMM
	''
LMM_LOOP
	rdlong	instr, pc
	add	pc, #4
	mov	LMM_IN_HUB, #1
instr
	nop
	jmp	#LMM_LOOP

LMM_CALL_FROM_COG
    wrlong  hubretptr, sp
    add     sp, #4
    jmp  #LMM_LOOP
LMM_CALL_FROM_COG_ret
    ret

LMM_RET
	sub	sp, #4
	rdlong	LMM_NEW_PC, sp
	jmp	#LMM_set_pc

	''
	'' complication for LMM_CALL: if we are called from
	'' cache rather than HUB, we need to calculate the pc
	''
LMM_CALL
	tjnz	LMM_IN_HUB, #LMM_CALL_FROM_HUB
	'' potential optimization: we could replace the LMM_CALL
	'' instruction in cache with one that jumps directly to
	'' LMM_CALL_FROM_CACHE
LMM_CALL_FROM_CACHE
	mov	LMM_tmp, LMM_RA
	sub	LMM_tmp, #LMM_cache_area	' find offset in cache
	shl	LMM_tmp, #2			' convert to HUB address
	add	LMM_tmp, LMM_cache_basepc	' and offset by cache base
	wrlong	LMM_tmp, sp			' push old PC
	add	sp, #4
	jmp	#LMM_JUMP_FROM_CACHE
LMM_CALL_FROM_HUB
	wrlong	pc, sp
	add	sp, #4
	jmp	#LMM_JUMP_FROM_HUB

	''
	'' similarly for LMM_JUMP, if we are called from
	'' cache the new pc is fetched from cache
	''
LMM_JUMP
	tjnz	LMM_IN_HUB, #LMM_JUMP_FROM_HUB
LMM_JUMP_FROM_CACHE
	movs	I_fetch_PC, LMM_RA
	add	LMM_RA, #1
I_fetch_PC
	mov	LMM_NEW_PC, 0-0
	jmp	#LMM_set_pc
LMM_JUMP_FROM_HUB
	rdlong	LMM_NEW_PC, pc
LMM_set_pc
	'' OK, actually set the new pc here
	mov    pc, LMM_NEW_PC
	jmp    #LMM_LOOP

LMM_JUMP_ret
LMM_CALL_ret
LMM_RET_ret
LMM_RA
	long	0	' return address for LMM subroutine calls
pc	long 0
inc_dest1
	long  (1<<9)
hubretptr
	long @@@hub_ret_to_cog
LMM_NEW_PC
	long   0
LMM_IN_HUB
	long 0
LMM_cache_basepc
	long	0		' HUB address of first cache instruction
LMM_tmp
	long	0
LMM_cache_area
	long	0[LMM_CACHE_SIZE]
	'' if we run off the end of the cache we have to jump back
	mov    LMM_NEW_PC, #LMM_CACHE_SIZE
	shl    LMM_NEW_PC, #2
	add    LMM_NEW_PC, LMM_cache_basepc
	jmp    #LMM_set_pc
