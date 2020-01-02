pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_go
	mov	result1, arg01
	add	result1, arg02
	sub	arg01, arg02
	mov	result2, arg01
	mov	result1, result2
_go_ret
	ret

_addsub
	mov	result1, arg01
	add	result1, arg02
	sub	arg01, arg02
	mov	result2, arg01
_addsub_ret
	ret

result1
	long	0
result2
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
