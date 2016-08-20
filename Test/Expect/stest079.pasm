PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_tx
	mov	_var__idx__0001, #4
L__0002
	mov	OUTA, arg1
	add	arg1, #1
	djnz	_var__idx__0001, #L__0002
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

_str_c
	long	0
_str_s
	long	0
_var__idx__0001
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
str_tmp001_
	long	0
	fit	496
