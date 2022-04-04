pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_stepif
	rdlong	_var01, arg01 wz
 if_ne	add	arg01, #4
 if_ne	rdlong	arg01, arg01
	mov	result1, arg01
_stepif_ret
	ret

_storeif
	rdlong	_var01, arg01 wz
 if_ne	mov	_var01, arg01
 if_ne	add	arg01, #8
 if_ne	wrlong	_var01, arg01
 if_ne	sub	arg01, #8
	mov	result1, arg01
_storeif_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
