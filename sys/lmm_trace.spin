LMM_RET
	muxnz	save_cz, #2			' save Z
	muxc	save_cz, #1			' save C
	sub	sp, #4
	rdlong	__newpc, sp
	jmp	#lmm_set_newpc
	
LMM_LOOP
	rdlong	instr, __pc
	add	__pc, #4
	mov	lmm_flags, #0
_trmov	mov	LMM_FCACHE_START, instr	' DEST is updated as needed
	add	_trmov, inc_dest1
instr
	nop
nextinstr
	djnz	trace_count, #LMM_LOOP
	'' fall through if non-zero
	'' we've run out of trace space; flush the trace cache
FLUSH_TRACE_CACHE
	'' clear all cache tags
	movd	flshc_lp, #trace_cache_tags
	mov	lpcnt, #16*2	' clear tags and saved pcs
flshc_lp
	mov	0-0, #0
	add	flshc_lp, inc_dest1
	djnz	lpcnt, #flshc_lp

	movd	_trmov, #LMM_FCACHE_START
	mov	trace_count, TRACE_SIZE		' maximum trace size
	sub	trace_count, #3			' leave room to close trace
	muxnz	save_cz, #2			' save Z
	muxc	save_cz, #1			' save C
	jmp	#LMM_LOOP

LMM_CALL_FROM_COG
    wrlong  hubretptr, sp
    add     sp, #4

    '' if called from COG, just flush and restart the cache
    jmp		#FLUSH_TRACE_CACHE
LMM_CALL_FROM_COG_ret
    ret

LMM_JUMP_PTR_ret
LMM_JUMP_ret
LMM_PUSH_ret
LMM_RET_ret
LMM_RA
	long 0	' return address for LMM subroutine calls

cache_end_seq
	call #LMM_JUMP
__newpc	long 0
__pc	long 0

inc_dest1
	long  (1<<9)
hubretptr
    long @@@hub_ret_to_cog
LMM_NEW_PC
    long   0

	'' This version of LMM does not support LMM_CALL
	'' instead it needs a sequence of 2 instructions:
	'' LMM_PUSH and then LMM_JUMP
	'' LMM_PUSH will push the return address onto the stack
LMM_PUSH
	'' if the actual CALL instruction that triggered this CALL or JUMP is in
	'' the ordinary LMM stream, the return address (found in LMM_RA) will be "nextinstr"
	'' in that case, the program counter to find is in HUB
	muxnz	save_cz, #2			' save Z
	muxc	save_cz, #1			' save C


	cmp	LMM_RA, #nextinstr wz
  if_nz	jmp	#push_from_cache

  	'' push from HUB code; need to copy to cache
  	rdlong	instr, __pc
	add	__pc, #4
	call	#emit_instr
	jmp	#do_push
push_from_cache
	movs	I_push_copy, LMM_RA
	add	LMM_RA, #1
I_push_copy
	mov	instr, 0-0
do_push
	shr	save_cz, #1 wc,wz
	wrlong	instr, sp
	add	sp, #4
	jmp	LMM_RA

LMM_JUMP_PTR
	muxnz save_cz, #2
	muxc  save_cz, #1
	mov   __newpc, LMM_NEW_PC
	jmp   #lmm_set_newpc
	
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
	rdlong	__newpc, __pc
	add	__pc, #4
	'' and copy it to cache
	mov    instr, __newpc
	call   #emit_instr
	
	'' we can skip the check for running in cache, we already
	'' know we aren't
	jmp	#close_cache_and_set_pc

	'' general routine for copying "newpc" to "pc"
	'' if we were called from HUB, close out the current
	'' cache line
lmm_set_newpc
	cmp	LMM_RA, #nextinstr wz
  if_z	jmp	#close_cache_and_set_pc
  	mov	__pc, __newpc
	jmp	#lmm_set_pc
	
close_cache_and_set_pc
	'' close out the current trace cache line
	call	 #close_cache_line	
	mov   __pc, __newpc

	'' finally go set the new pc
	jmp   #lmm_set_pc
	
already_in_cache
	'' this is where we come for a JMP/CALL that was already in cache
	'' this is a simpler case, we just fetch the new pc from cache
	movs	I_getpc_from_cache, LMM_RA
	add	LMM_RA, #1
I_getpc_from_cache
	mov	__pc, 0-0

	' now, here's an interesting thing we could do: if the jump target
	' is in cache, we can re-write the jmp instruction itself to
	' go directly there and bypass this whole rigamarole
	or	lmm_flags, #1
	jmp	#lmm_set_pc

lmm_set_pc
	'' see if the pc is already in the trace cache
	mov	lmm_cptr, __pc
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
	cmp	__pc, 0-0 wz	' compare against current cache tag

if_z	jmp	#cache_hit

	'' cache miss here
	'' get the cache pointer from the LMM loop
	mov   cacheptr, _trmov
	shr   cacheptr, #9		' extract current cache pointer from instruction in loop
	and   cacheptr, #$1ff		' FIXME? probably only needed to make reg contents pretty for debug
I_lmm_savetag
	mov	0-0, __pc		' save pc as new cache tag
I_lmm_savetracestart
	mov	0-0, cacheptr	' save pointer to cache data

	'' and we have to continue with the regular LMM here, since we have no valid trace in cache
	shr	save_cz, #1 wz, wc
	jmp	#nextinstr
	
cache_hit
	'' if lmm_flags &1 is non-zero, then the jump instruction was in
	'' cache, and so is the destination, so we can re-write the jmp
	'' so it goes from cache to cache
	test	lmm_flags, #1 wz
  if_z	jmp	#skip_rewrite
  	sub	LMM_RA, #2	' point back to the original instruction
	''
	'' this is a little bit tricky: I_lmm_jmpindirect points to the pointer to the actual PC
	'' so we need to do a double indirection to fetch it
	''
	movs	I_dblfetch, I_lmm_jmpindirect
	movd	I_rewrite, LMM_RA
I_dblfetch
	movs	I_rewrite, 0-0
	mov	lmm_flags, #0
I_rewrite
	movs	0-0, #I_lmm_jmpindirect		' dest comes from LMM_RA, src from new PC target


skip_rewrite
	'' restore flags
	shr	save_cz, #1 wz,wc

	'' now jump into the cache
I_lmm_jmpindirect
	jmp	0-0
	

	''
	'' copy the instruction at "opcode" to the cache
	''
emit_instr
	mov	_trmov2, _trmov
	sub	trace_count, #1
_trmov2
	mov	0-0, 0-0
	add	_trmov, inc_dest1
	mins	trace_count, #1
emit_instr_ret
	ret
	
	''
	'' close out the current trace cache line
	'' we should optimize and see if the previous branch
	'' was conditional, but for now we just always add
	'' a jump back to the original pc
	''
close_cache_line
	mov	instr, cache_end_seq
	call	#emit_instr
	mov	instr, __pc
	call	#emit_instr
close_cache_line_ret
	ret
	
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

TRACE_SIZE
	long	LMM_FCACHE_END - LMM_FCACHE_START
trace_count
	long	1
trace_firstpc
	long	0
lpcnt
	long	0
lmm_flags
	long	0
