pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_check1
	cmps	arg01, arg02 wc,wz
 if_be	mov	result1, #9
 if_a	neg	result1, #1
_check1_ret
	ret

_check2
	cmps	arg01, arg02 wc,wz
 if_be	mov	result1, #9
 if_a	neg	result1, #1
_check2_ret
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
	fit	496
