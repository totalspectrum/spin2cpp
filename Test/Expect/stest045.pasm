PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_strlen2
	mov	_var_02, #0
LR__0001
	rdbyte	_tmp001_, arg1 wz
 if_ne	add	arg1, #1
 if_ne	add	_var_02, #1
 if_ne	jmp	#LR__0001
	mov	result1, _var_02
_strlen2_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
	res	1
_var_02
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
