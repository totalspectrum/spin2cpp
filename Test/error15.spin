DAT
	org 0
	call	should_be_imm		' this should give a warning
	call	#should_be_imm
	jmp	is_indirect		' ok
	test	is_indirect, #1
	call	is_indirect
	jmp	should_be_imm
	jmp	#should_be_imm
	djnz	0, should_be_imm_ret	' should give a warning
	jmp	should_be_imm_ret	' this actually should not give a warning
is_indirect
	long	0
is_indirect_ret

should_be_imm
	or	OUTB, #0
should_be_imm_ret
	ret

PUB dummy
