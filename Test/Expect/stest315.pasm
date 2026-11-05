pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zcheck2
_zcheck1
	cmp	arg01, #0 wz
 if_ne	mov	result1, arg02
 if_ne	mov	result2, arg03
 if_e	mov	result1, arg04
 if_e	mov	result2, arg05
_zcheck1_ret
_zcheck2_ret
	ret


_zcheck3
	cmp	arg01, #0 wz
 if_ne	mov	result1, #0
 if_ne	mov	result2, #0
 if_e	mov	result1, arg04
 if_e	mov	result2, arg05
_zcheck3_ret
	ret

_zcheck4
	cmp	arg01, #0 wz
 if_ne	mov	result1, arg02
 if_ne	mov	result2, arg03
 if_e	mov	result1, #0
 if_e	mov	result2, #0
_zcheck4_ret
	ret

result1
	long	0
result2
	long	1
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
arg03
	res	1
arg04
	res	1
arg05
	res	1
	fit	496
