pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_rx
	andn	dira, arg1
LR__0001
	mov	_tmp001_, ina
	test	_tmp001_, arg1 wz
 if_ne	jmp	#LR__0001
	mov	result1, ina
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
	fit	496
