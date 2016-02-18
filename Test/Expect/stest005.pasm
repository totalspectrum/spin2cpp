DAT
	org	0
stest005_val511
	mov	result_, #511
stest005_val511_ret
	ret

stest005_val512
	mov	result_, imm_512_
stest005_val512_ret
	ret

imm_512_
	long	512
result_
	long	0
