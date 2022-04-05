pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_strlen2
	mov	_var01, #0
LR__0001
	rdbyte	result1, arg01 wz
 if_ne	add	arg01, #1
 if_ne	add	_var01, #1
 if_ne	jmp	#LR__0001
	mov	result1, _var01
_strlen2_ret
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
	fit	496
