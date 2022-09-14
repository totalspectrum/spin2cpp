	''
	'' caching LMM
	''
LMM_LOOP
	rdlong	instr, __pc
	add	__pc, #4
	mov	LMM_IN_HUB, #1
instr
	nop
nextinstr
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
	'' to push on the stack
	''
LMM_CALL_PTR
	tjnz	LMM_IN_HUB, #LMM_CALL_PTR_FROM_HUB
	'' call from cache
	jmp	#LMM_do_call_from_cache
LMM_CALL_PTR_FROM_HUB
	wrlong	__pc, sp
	add	sp, #4
	jmp	#LMM_do_jump
	
LMM_CALL
	tjnz	LMM_IN_HUB, #LMM_CALL_FROM_HUB
LMM_CALL_FROM_CACHE
	''retrieve the pc
	movs	I_fetch_CALL_PC, LMM_RA
	add	LMM_RA, #1
I_fetch_CALL_PC
	mov	LMM_NEW_PC, 0-0
LMM_do_call_from_cache
	mov	LMM_tmp, LMM_RA
	sub	LMM_tmp, #LMM_FCACHE_START	' find offset in cache
	shl	LMM_tmp, #2			' convert to HUB address
	add	LMM_tmp, LMM_cache_basepc	' and offset by cache base
	wrlong	LMM_tmp, sp			' push old PC
	add	sp, #4
	jmp	#LMM_set_pc
LMM_CALL_FROM_HUB
	wrlong	__pc, sp
	add	sp, #4
	jmp	#LMM_JUMP_FROM_HUB

	''
	'' similarly for LMM_JUMP, if we are called from
	'' cache the new pc is fetched from cache
	''
LMM_JUMP
	tjnz	LMM_IN_HUB, #LMM_JUMP_FROM_HUB
LMM_JUMP_FROM_CACHE
	movs	I_fetch_JMP_PC, LMM_RA
	add	LMM_RA, #1
I_fetch_JMP_PC
	mov	LMM_NEW_PC, 0-0

LMM_JUMP_PTR
	'' see if new PC is still in cache; if it is, modify the jump
	'' so that it goes directly there
	'' WARNING: we cannot do that for LMM_CALL
	
	'' save C and Z flags
	muxnz	save_cz, #2			' save Z
	muxc	save_cz, #1			' save C
	'' calculate offset into cache
	'' we expect LMM_cache_basepc <= LMM_NEW_PC < LMM_cache_endpc
	cmp	LMM_NEW_PC, LMM_cache_endpc wc,wz
  if_ae	jmp	#not_in_cache
	mov	LMM_tmp, LMM_NEW_PC
	subs	LMM_tmp, LMM_cache_basepc wc,wz
  if_b	jmp	#not_in_cache
  	'' yes, we are in cache
	shr	LMM_tmp, #2  ' convert offset to longs
	sub	LMM_RA, #2   ' back up cog pc to point to jmp/call
	'' change the JMP instruction to go directly where we want
	movd	fixup_jmp, LMM_RA
	add	LMM_tmp, #LMM_FCACHE_START
fixup_jmp
	movs	0-0, LMM_tmp	' update JMP instruction
	'' restore C and Z flags
	shr	save_cz, #1 wc,wz
	
	'' jump to the instruction we just fixed up
	mov	LMM_IN_HUB, #0
	jmp	LMM_RA
	
not_in_cache
	'' restore C and Z flags
	shr	save_cz, #1 wc,wz
	'' and go set the new pc
	jmp	#LMM_set_pc
LMM_JUMP_FROM_HUB
	rdlong	LMM_NEW_PC, __pc
	add	__pc, #4
LMM_do_jump
	'' see if we want to change the cache
	'' we do that only for backwards branches
	'' where the whole code from the new PC to the current PC
	'' will fit in cache
	
	'' save C and Z flags
	muxc    save_cz, #1
	muxnz	save_cz, #2

	'' is this area already in cache?
	cmp   LMM_cache_basepc, LMM_NEW_PC wz
   if_z	jmp   #jump_into_cache
	
	'' calculate size needed in cache
	'' if we cache from LMM_NEW_PC up to and including pc
	'' NOTE: we expect LMM_NEW_PC < pc
  	mov	LMM_tmp, __pc
	sub	LMM_tmp, LMM_NEW_PC wc, wz
	cmp	LMM_tmp, LMM_CACHE_SIZE_BYTES wc,wz
  if_ae	jmp	#no_recache

  	mov	LMM_cache_basepc, LMM_NEW_PC
	mov	LMM_cache_endpc, __pc

	call	#LMM_load_cache

jump_into_cache
	'' restore flags
	shr	save_cz, #1 wc,wz

	'' now jump into the cache
	mov	LMM_IN_HUB, #0
	jmp    	#LMM_FCACHE_START	
no_recache
	' restore flags
	shr	save_cz, #1 wc,wz
	' fall through to LMM_set_pc
LMM_set_pc
	'' OK, actually set the new pc here
	'' really should check here to see if we're jumping into cache
	mov    __pc, LMM_NEW_PC
	jmp    #nextinstr

LMM_JUMP_PTR_ret
LMM_JUMP_ret
LMM_CALL_ret
LMM_CALL_PTR_ret
LMM_PUSH_ret
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
	long	0
LMM_cache_basepc
	long	0		' HUB address of first cache instruction
LMM_cache_endpc
	long	0		' HUB address of last cache instruction
LMM_tmp
	long	0

	'' load the cache area
	'' LMM_cache_basepc and LMM_cache_endpc are already set
LMM_load_cache
	mov	LMM_tmp, LMM_cache_basepc
	mov	FCOUNT_, LMM_cache_endpc
	sub	FCOUNT_, LMM_cache_basepc
	movd   a_fcacheldlp, #LMM_FCACHE_START
	shr    FCOUNT_, #2
a_fcacheldlp
	rdlong 0-0, LMM_tmp
	add    LMM_tmp, #4
	add    a_fcacheldlp,inc_dest1
	djnz   FCOUNT_,#a_fcacheldlp
	'' now add in a JMP out of LMM
	ror    a_fcacheldlp, #9	   	' get the dest into low bits
	movd   a_fcachecopyjmp, a_fcacheldlp
	rol    a_fcacheldlp, #9		' restore the instruction
a_fcachecopyjmp
	mov	0-0, LMM_jmptop
LMM_load_cache_ret
	ret
LMM_jmptop
	jmp	#LMM_end_cache
LMM_end_cache
	'' if we run off the end of the cache we have to jump back
	mov	LMM_IN_HUB, #1
	mov    	__pc, LMM_cache_endpc
	jmp    	#nextinstr
	
FCOUNT_
	long 0
save_cz
	long 0
LMM_CACHE_SIZE_BYTES
	long 4*(LMM_FCACHE_END - LMM_FCACHE_START)
	
