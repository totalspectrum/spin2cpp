pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_mylen
	mov	result1, arg01
LR__0001
	rdbyte	_var01, result1 wz
 if_ne	add	result1, #1
 if_ne	jmp	#LR__0001
	sub	result1, arg01
_mylen_ret
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
