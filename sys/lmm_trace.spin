LMM_RET
	sub	sp, #4
	rdlong	pc, sp
	jmp	#lmm_set_pc
	
LMM_LOOP
	rdlong	instr, pc
	add	pc, #4
_trmov	mov	trace_data, instr	' DEST is updated as needed
instr
	nop
nextinstr
	add	_trmov, inc_dest1
	djnz	trace_count, #LMM_LOOP
	'' we've run out of trace space; restart the trace
RESTART_TRACE
	movd	_trmov, #trace_data
	mov	trace_firstpc, pc
	mov	trace_count, #128-4	' maximum trace size
	jmp	#LMM_LOOP

LMM_CALL_FROM_COG
    wrlong  hubretptr, sp
    add     sp, #4
    jmp	    #lmm_set_pc
LMM_CALL_FROM_COG_ret
    ret

LMM_JUMP_ret
LMM_CALL_ret
LMM_RA
	long 0	' return address for LMM subroutine calls

	'' this is a 3 long sequence to be copied to CACHE
cache_end_seq
pc	long 0
	call #LMM_JUMP
newpc	long 0

inc_dest1
	long  (1<<9)
hubretptr
    long @@@hub_ret_to_cog
LMM_NEW_PC
    long   0

LMM_CALL
	wrlong	pc, sp
	add	sp, #4
LMM_JUMP
	'' if the actual CALL instruction that triggered this CALL or JUMP is in
	'' the ordinary LMM stream, the return address (found in LMM_RA) will be "nextinstr"
	'' in that case, the program counter to find is in HUB
	muxnz	save_cz, #2			' save Z
	muxc	save_cz, #1			' save C
	
	cmp	LMM_RA, #nextinstr wz
  if_nz	jmp	#already_in_cache

	'' here we were running in ordinary LMM mode
	'' so we need to fetch the new PC from HUB memory
	rdlong	newpc, pc
	add	pc, #4
	
	'' we are going to need 3 longs in cache:
	''    a long for the new pc (part of the LMM jump sequence)
	''    2 longs for a new LMM jump back to oldpc (in case the original
	''    jump was conditional)

	'' finally go set the new pc
	mov   pc, newpc
	jmp   #do_lmm_set_pc
	
already_in_cache
	'' this is where we come for a JMP/CALL that was already in cache
	'' this is a simpler case, we just fetch the new pc from cache
	movs	I_getpc_from_cache, LMM_RA
	add	LMM_RA, #1
I_getpc_from_cache
	mov	pc, 0-0
	jmp	#do_lmm_set_pc
	
lmm_set_pc
	'' save flags
	muxnz	save_cz, #2			' save Z
	muxc	save_cz, #1			' save C
do_lmm_set_pc

	'' restore flags
	shr	save_cz, #1 wc,wz
	jmp	#LMM_LOOP
	
	
	'' see if the pc is already in the trace cache
	mov	lmm_cptr, pc
	shr	lmm_cptr, #2
	and	lmm_cptr, #$F	' 16 trace cache tags
	add	lmm_cptr, #trace_cache_tags
	movs	I_lmm_cmp1, lmm_cptr
	movd	I_lmm_savepc, lmm_cptr
	add	lmm_cptr, #(trace_cache_pc - trace_cache_tags)
	movs	I_lmm_jmpindirect, lmm_cptr
I_lmm_cmp1
	cmp	pc, 0-0 wz
  if_z	jmp	#cache_hit
	mov	lmm_tmp, 0-0

	'' cache miss here
	'' start a new trace
	movd	I_lmm_savetracestart, lmm_cptr
I_lmm_savepc
	mov	0-0, pc	' save pc to cache tags
I_lmm_savetracestart
	mov	0-0, trace_firstpc
cache_hit
	'' restore flags
	shr	save_cz, #1 wz,wc
I_lmm_jmpindirect
	jmp	0-0
	
save_cz
	long	0
lmm_cptr
	long	0
lmm_tmp
trace_cache_tags
	long	0[16]
trace_cache_pc
	long	0[16]
	
trace_data
	long	0[128]
trace_data_end

trace_count
	long	1
trace_firstpc
	long	0
