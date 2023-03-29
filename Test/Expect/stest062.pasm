pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_mylen
	mov	_var01, arg01
LR__0001
	rdbyte	result1, _var01 wz
 if_ne	add	_var01, #1
 if_ne	jmp	#LR__0001
	sub	_var01, arg01
	mov	result1, _var01
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
