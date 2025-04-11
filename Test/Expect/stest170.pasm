pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_setptr2
	mov	setptr2_tmp004_, arg01
	mov	setptr2_tmp001_, #44
	call	#_nextidx
	shl	result1, #2
	add	setptr2_tmp001_, result1
	mov	arg01, setptr2_tmp004_
	add	objptr, setptr2_tmp001_
	call	#_pinobj_tx
	sub	objptr, setptr2_tmp001_
_setptr2_ret
	ret

_nextidx
	add	objptr, #40
	rdlong	result1, objptr
	mov	_var01, result1
	add	_var01, #1
	wrlong	_var01, objptr
	sub	objptr, #40
_nextidx_ret
	ret

_pinobj_tx
	mov	_var01, #1
	rdlong	_var02, objptr
	shl	_var01, _var02
	test	arg01, #1 wz
	muxnz	outa, _var01
_pinobj_tx_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[21]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
setptr2_tmp001_
	res	1
setptr2_tmp004_
	res	1
	fit	496
