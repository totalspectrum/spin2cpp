pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_dbl64
	add	arg02, arg02 wc
	addx	arg01, arg01
	mov	result2, arg02
	mov	result1, arg01
_dbl64_ret
	ret

_quad64
	add	arg02, arg02 wc
	addx	arg01, arg01
	add	arg02, arg02 wc
	addx	arg01, arg01
	mov	result2, arg02
	mov	result1, arg01
_quad64_ret
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
	fit	496
