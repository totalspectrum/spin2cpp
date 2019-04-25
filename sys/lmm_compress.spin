	''
	'' decoder for compressed instructions
	''
	'' format:
	'' $Fx yy zz ww : rotated instruction with COND==$F (if_always)
	'' $Ey yy: escape: jump to yyy in COG memory
	'' $D0 yyyy yyyy: full 32 bit instruction
	'' $Dx yyyy: conditional jump to address yyyy (x<>0)
	'' $Cx yyyy: call subroutine address yyyy (x should be 0 for now, other values reserved)
	'' %10mm mmdd ddds ssss:
	''           compressed instruction; 4 bits for the instruction, 5 for each of src and dest,
	''           select the most commonly used instructions, sources, and destinations
	''
	'' %0add dsss: a selects most common 2 instructions, ddd, sss most common 8 sources/destinations
	''
getword
	rdbyte	LMM_NEW_PC, pc
	add	pc, #1
	rdbyte	optemp, pc
	add	pc, #1
	shl	optemp, #8
	or	LMM_NEW_PC, optemp
getword_ret
	ret
	
compress_optable
	long	handle_C
	long	handle_D
	long	handle_E
	long	handle_F

handle_C
	call	#getword	' fetch word into LMM_NEW_PC
	shr	save_cz, #1 wz, wc	' restore C and Z
	jmp	#LMM_CALL_PTR	' and go do it as a subroutine
handle_D
	call	#getword	' fetch new PC
	cmp	opcode, #$D0 wz
  if_z	jmp	#single_instr
  	and	opcode, #$F	' isolate condition code
	xor	opcode, #$F	' flip bits for andn use
	shl	opcode, #18	' move to condition part
	mov	instr, indirect_jmp_instr
	andn	instr, opcode
	jmp	#go_instr
indirect_jmp_instr
	mov	pc, LMM_NEW_PC

single_instr
	mov	instr, LMM_NEW_PC
	call	#getword
	shl	LMM_NEW_PC, #16
	or	instr, LMM_NEW_PC
	jmp	#go_instr
handle_E
	rdbyte	optemp, pc
	add	pc, #1
	and	opcode, #$1
	shl	opcode, #8
	or	optemp, opcode
	shr	save_cz, #1 wz, wc	' restore C and Z
	jmpret	LMM_ra, optemp+0
handle_F
	sub	pc, #1			' back up
	call	#getword
	mov	instr, LMM_NEW_PC
	call	#getword
	shl	LMM_NEW_PC, #16
	or	instr, LMM_NEW_PC
	rol	instr, #18
	jmp	#go_instr

LMM_LOOP
	rdbyte	opcode, pc
	add	pc, #1
	muxnz	save_cz, #2			' save Z
	muxc	save_cz, #1			' save C
	test	opcode, #$80 wz			' check for high bit
  if_z	jmp	#single_byte_compress
  	test	opcode, #$40 wz
  if_z	jmp	#multi_byte_compress
  	mov	optemp, opcode
	shr	optemp, #6
	add	optemp, #compress_optable
	jmp	optemp+0
	
go_instr
	shr	save_cz, #1 wz, wc
instr
	nop
	jmp	#LMM_LOOP
opcode
	long	0
optemp
	long	0
	
LMM_RET
	sub	sp, #4
	rdlong	pc, sp
	jmp	#LMM_LOOP

LMM_CALL_PTR
	wrlong	pc, sp
	add	sp, #4
	mov	pc, LMM_NEW_PC
	jmp	#LMM_LOOP
LMM_CALL
	wrlong	pc, sp
	add	sp, #4
LMM_JUMP
	rdlong	pc, pc
	jmp	#LMM_LOOP

LMM_CALL_FROM_COG
	wrlong  hubretptr, sp
	add     sp, #4
	jmp  #LMM_LOOP
LMM_CALL_FROM_COG_ret
	ret

LMM_JUMP_ret
LMM_CALL_ret
LMM_CALL_PTR_ret
LMM_RET_ret
LMM_ra
	long	0	' return address for LMM subroutine calls
pc	long 0
inc_dest1
	long  (1<<9)
hubretptr
	long @@@hub_ret_to_cog
LMM_NEW_PC
	long   0
