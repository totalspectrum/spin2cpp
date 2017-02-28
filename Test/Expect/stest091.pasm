PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_sendchar1
	add	objptr, #4
	call	#_simplepin_tx
	sub	objptr, #4
_sendchar1_ret
	ret

_sendchar2
	add	objptr, #16
	call	#_simplepin_tx
	sub	objptr, #16
_sendchar2_ret
	ret

_sendchar_index
	mov	sendchar_index_tmp002_, arg1
	mov	arg1, arg2
	shl	sendchar_index_tmp002_, #2
	add	sendchar_index_tmp002_, #8
	add	objptr, sendchar_index_tmp002_
	call	#_simplepin_tx
	sub	objptr, sendchar_index_tmp002_
_sendchar_index_ret
	ret

_simplepin_tx
	mov	_var_02, #1
	rdlong	_tmp002_, objptr
	shl	_var_02, _tmp002_
	test	arg1, #1 wz
	muxnz	OUTA, _var_02
_simplepin_tx_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[6]
	org	COG_BSS_START
_tmp002_
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
sendchar_index_tmp002_
	res	1
	fit	496
