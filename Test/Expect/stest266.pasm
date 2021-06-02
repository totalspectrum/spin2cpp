pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_byteextend
	rdlong	_var01, arg01
	shl	_var01, #16
	sar	_var01, #16
	wrlong	_var01, arg01
_byteextend_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
