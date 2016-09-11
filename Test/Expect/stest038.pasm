PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test
L__0001
	mov	_var_01, INA
	sar	_var_01, #1
	and	_var_01, #1 wz
 if_ne	jmp	#L__0002
	mov	_tmp001_, INA
	sar	_tmp001_, #2
	test	_tmp001_, #1 wz
 if_ne	jmp	#L__0001
	xor	OUTA, #1
	jmp	#L__0001
L__0002
_test_ret
	ret

_tmp001_
	long	0
_var_01
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
