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
	jmp	#handle_C
	jmp	#handle_D
	jmp	#handle_E
	jmp	#handle_F

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
	rol	instr, #14
	jmp	#go_instr

LMM_LOOP
	rdbyte	opcode, pc
	add	pc, #1
	muxnz	save_cz, #2			' save Z
	muxc	save_cz, #1			' save C
	test	opcode, #$80 wz			' check for high bit
  if_z	jmp	#single_byte_decompress
  	test	opcode, #$40 wz
  if_z	jmp	#multi_byte_decompress
  	mov	optemp, opcode
	shr	optemp, #4
	and	optemp, #3
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
optemp2
	long	0

	'' two bytes:
	'' $80 + 6 bits (4 bits instruction, 2 high bits of dest)
	'' 8 bits (3 low bits dest, 5 bits src)
multi_byte_decompress
	mov	optemp, opcode
	and	optemp, #3	' isolate top 2 bits
	shl	optemp, #3	' move them into place
	shr	opcode, #2
	and	opcode, #$f	' opcode contains instruction
	rdbyte	optemp2, pc
	add	pc, #1
	mov	instr, optemp2
	and	optemp2, #$1f
	shr	instr, #5
	and	instr, #7
	or	optemp, instr
	jmp	#decompress_instr
single_byte_decompress
	mov	optemp, opcode
	shr	optemp, #3
	and	optemp, #$7
	mov	optemp2, opcode
	and	optemp2, #$7
	shr	opcode, #6
decompress_instr
	'' enter here with opcode == instruction, optemp == dest, optemp2 == src
	add	opcode, #COMPRESS_TABLE
	movs	c_fetch1, opcode
	add	optemp, #COMPRESS_TABLE
	movs	c_fetch2, optemp
	add	optemp2, #COMPRESS_TABLE
	movs	c_fetch3, optemp2
c_fetch1
	mov	instr, 0-0
	and	instr, INSTR_MASK
c_fetch2
	mov	optemp, 0-0
c_fetch3
	mov	optemp2, 0-0
	and	optemp, DST_MASK
	and	optemp2, SRC_MASK
	or	instr, optemp
	or	instr, optemp2
	jmp	#go_instr
INSTR_MASK
	long	$ffbc_0000
DST_MASK
	long	$0003_fe00
SRC_MASK
	long	$0040_01ff

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
pc	long	0
inc_dest1
	long	(1<<9)
hubretptr
	long	@@@hub_ret_to_cog
LMM_NEW_PC
	long	0
save_cz
	long	0

