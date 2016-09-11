PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_tx
	mov	_var_02, #4
L__0002
	mov	OUTA, arg1
	add	arg1, #1
	djnz	_var_02, #L__0002
_tx_ret
	ret

_str
	mov	_str_s, arg1
L__0005
	mov	str_tmp001_, _str_s
	add	_str_s, #1
	rdbyte	_str_c, str_tmp001_ wz
 if_e	jmp	#L__0006
	mov	arg1, _str_c
	call	#_tx
	jmp	#L__0005
L__0006
_str_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_str_c
	res	1
_str_s
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
str_tmp001_
	res	1
	fit	496
