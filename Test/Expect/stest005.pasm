DAT
	org	0

val511
	mov	result_, #511
val511_ret
	ret

val512
	mov	result_, imm_512_
val512_ret
	ret

imm_512_
	long	512
result_
	long	0
