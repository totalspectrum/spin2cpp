PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_fibolp
	mov	_var_03, #1
	mov	_var_r, #1
	mov	_var_02, #0
	sub	arg1, #1 wz
 if_e	jmp	#L__0006
L__0007
	add	_var_02, _var_03
	mov	_tmp003_, _var_02
	mov	_var_02, _var_03
	mov	_var_03, _var_r
	mov	_var_r, _tmp003_
	djnz	arg1, #L__0007
L__0006
	mov	result1, _var_r
_fibolp_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp003_
	res	1
_var_02
	res	1
_var_03
	res	1
_var_r
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
