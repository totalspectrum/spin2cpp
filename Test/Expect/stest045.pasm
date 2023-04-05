pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_strlen2
	mov	result1, #0
LR__0001
	rdbyte	_var01, arg01 wz
 if_ne	add	arg01, #1
 if_ne	add	result1, #1
 if_ne	jmp	#LR__0001
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
