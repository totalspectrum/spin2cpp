PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_main
	mov	_var_01, CNT
	mov	_var_02, imm_512_
L__0005
	xor	OUTA, #1
	djnz	_var_02, #L__0005
	mov	_tmp001_, CNT
	sub	_tmp001_, _var_01
	mov	DIRA, _tmp001_
_main_ret
	ret

imm_512_
	long	512
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
	res	1
_var_01
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
