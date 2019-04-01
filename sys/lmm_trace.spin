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
LMM_ra
	long	0	' return address for LMM subroutine calls
pc	long 0
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
	rdlong	pc, pc
	'' fall through
lmm_set_pc
	jmp	#LMM_LOOP
	
	'' save Z
	muxnz	savez, #1
	
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
	shr	savez, #1 wz
I_lmm_jmpindirect
	jmp	0-0
	
savez
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
