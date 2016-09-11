PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_strlen2
	mov	_var_02, #0
L__0001
	rdbyte	_tmp001_, arg1 wz
 if_ne	add	arg1, #1
 if_ne	add	_var_02, #1
 if_ne	jmp	#L__0001
	mov	result1, _var_02
_strlen2_ret
	ret

_tmp001_
	long	0
_var_02
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
