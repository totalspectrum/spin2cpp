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
	'' fall through if non-zero
	'' we've run out of trace space; flush the trace cache
FLUSH_TRACE_CACHE
#ifdef NEVER
	'' clear all cache tags
	movd	flshc_lp, #trace_cache_tags
	mov	cnt, #16*2	' clear tags and saved pcs
flshc_lp
	mov	0-0, #0
	add	flshc_lp, #1
	add	flshc_lp, inc_dest1
	djnz	cnt, #flshc_lp
#endif

	movd	_trmov, #trace_data
	mov	trace_count, #120	' maximum trace size
	jmp	#lmm_set_pc

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
	mov   cacheptr, _trmov
	shr   cacheptr, #9		' extract current cache pointer from instruction in loop
	movd  _trmov2, cacheptr
	movs  _trmov2, #cache_end_seq
	mov   lpcnt, #3

L_copy_lp
_trmov2
	mov	0-0, 0-0	' copy from cache_end_seq into cache
	add	_trmov2, inc_dest1
	add	_trmov2, #1
	sub	trace_count, #1	' 1 fewer instruction left in trace
	djnz	lpcnt, #L_copy_lp

	mov   pc, newpc


	'' did we run out of room in the cache?
	'' if so, flush it and start over
	cmps	trace_count, #4 wc,wz
  if_be	jmp	#restore_flags_and_RESTART_TRACE
	
  	'' restore original cache ptr
	mov	cacheptr, _trmov2
	shr	cacheptr, #9
	movd	_trmov, cacheptr

	'' finally go set the new pc
	jmp   #do_lmm_set_pc
	
already_in_cache
	'' this is where we come for a JMP/CALL that was already in cache
	'' this is a simpler case, we just fetch the new pc from cache
	movs	I_getpc_from_cache, LMM_RA
	add	LMM_RA, #1
I_getpc_from_cache
	mov	pc, 0-0
	jmp	#do_lmm_set_pc

restore_flags_and_RESTART_TRACE
	shr	save_cz, #1 wc,wz
	jmp	#FLUSH_TRACE_CACHE
	
lmm_set_pc
	'' save flags
	muxnz	save_cz, #2			' save Z
	muxc	save_cz, #1			' save C
do_lmm_set_pc

	'' FIXME: continue with the regular LMM here, since we have no valid trace in cache
	shr	save_cz, #1 wz, wc
	jmp	#LMM_LOOP

	'' see if the pc is already in the trace cache
	mov	lmm_cptr, pc
	shr	lmm_cptr, #2	' ignore low bits of pc, these will always be 0
	and	lmm_cptr, #$F	' 16 trace cache tags
	add	lmm_cptr, #trace_cache_tags

	'' set up to test against cache tag
	movs	I_lmm_cmp1, lmm_cptr
	'' set up to save new tag if we have a miss
	movd	I_lmm_savetag, lmm_cptr
	'' go to next the actual cache pc
	add	lmm_cptr, #(trace_cache_pcs - trace_cache_tags)
	'' set up to save the trace pc start
	movd	I_lmm_savetracestart, lmm_cptr
	'' set up to jump to the right place if there is a hit
	movs	I_lmm_jmpindirect, lmm_cptr


I_lmm_cmp1
	cmp	pc, 0-0 wz	' compare against current cache tag
#ifdef NEVER
if_z	jmp	#cache_hit
#else
	nop
#endif
	'' cache miss here
	'' get the cache pointer from the LMM loop
	mov   cacheptr, _trmov
	shr   cacheptr, #9		' extract current cache pointer from instruction in loop
I_lmm_savetag
	mov	0-0, pc		' save pc as new cache tag
I_lmm_savetracestart
	mov	0-0, cacheptr	' save pointer to cache data

	'' and we have to continue with the regular LMM here, since we have no valid trace in cache
	shr	save_cz, #1 wz, wc
	jmp	#LMM_LOOP
	
cache_hit
	'' restore flags
	shr	save_cz, #1 wz,wc

	'' now jump into the cache
I_lmm_jmpindirect
	jmp	0-0
	
save_cz
	long	0
lmm_cptr
	long	0
cacheptr
	long	0
trace_cache_tags
	long	0[16]
trace_cache_pcs
	long	0[16]
	
trace_data
	long	0[128]
trace_data_end

trace_count
	long	1
trace_firstpc
	long	0
lpcnt
	long	0
