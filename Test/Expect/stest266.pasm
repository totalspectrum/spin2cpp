pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_byteextend1
	rdlong	result1, arg01
	shl	result1, #16
	sar	result1, #16
	wrlong	result1, arg01
	add	arg01, #1
	mov	result1, arg01
_byteextend1_ret
	ret

_byteextend2
	rdlong	_var01, objptr
	shl	_var01, #24
	sar	_var01, #24
	wrlong	_var01, objptr
_byteextend2_ret
	ret

_byteextend3
	add	objptr, #4
	rdbyte	_var01, objptr
	shl	_var01, #24
	sar	_var01, #24
	wrbyte	_var01, objptr
	sub	objptr, #4
	wrlong	_var01, objptr
_byteextend3_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[2]
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
