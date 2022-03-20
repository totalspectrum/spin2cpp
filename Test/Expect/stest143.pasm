pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_sum1
	add	arg01, arg02
	mov	result1, arg01
_sum1_ret
	ret

_sum2
	add	arg01, arg02
	mov	result1, arg01
_sum2_ret
	ret

_sum3
	add	arg01, arg02
	mov	result1, arg01
_sum3_ret
	ret

_sum4
	cmps	arg02, #0 wc
 if_b	mov	result1, #0
 if_ae	add	arg01, arg02
 if_ae	mov	result1, arg01
_sum4_ret
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
