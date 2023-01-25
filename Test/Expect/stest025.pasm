pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_test1
	cmp	arg01, #0 wz
 if_e	cmp	arg02, #0 wz
 if_ne	mov	result1, arg03
 if_e	neg	result1, #1
_test1_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
