pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_mul
	mov	_var01, #0
LR__0001
	shr	arg02, #1 wc,wz
 if_b	add	_var01, arg01
	shl	arg01, #1
 if_ne	jmp	#LR__0001
	mov	result1, _var01
_mul_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
