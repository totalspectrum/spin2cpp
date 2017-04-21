PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_rx
	andn	DIRA, arg1
L__0001
	mov	_tmp001_, INA
	test	_tmp001_, arg1 wz
 if_ne	jmp	#L__0001
	mov	result1, INA
	and	result1, arg1
_rx_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
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
