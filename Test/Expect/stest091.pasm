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
	shl	arg1, #2
	mov	sendchar_index_tmp001_, arg1
	add	sendchar_index_tmp001_, #8
	mov	arg1, arg2
	add	objptr, sendchar_index_tmp001_
	call	#_simplepin_tx
	sub	objptr, sendchar_index_tmp001_
_sendchar_index_ret
	ret

_sendchar_abstract
	mov	_sendchar_abstract_fds, arg1
	mov	arg1, arg2
	mov	sendchar_abstract_tmp002_, objptr
	mov	objptr, _sendchar_abstract_fds
	call	#_simplepin_tx
	mov	objptr, sendchar_abstract_tmp002_
_sendchar_abstract_ret
	ret

_simplepin_tx
	mov	_var_01, #1
	rdlong	_tmp002_, objptr
	shl	_var_01, _tmp002_
	test	arg1, #1 wz
	muxnz	OUTA, _var_01
_simplepin_tx_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[6]
	org	COG_BSS_START
_sendchar_abstract_fds
	res	1
_tmp002_
	res	1
_var_01
	res	1
arg1
	res	1
arg2
	res	1
sendchar_abstract_tmp002_
	res	1
sendchar_index_tmp001_
	res	1
	fit	496
