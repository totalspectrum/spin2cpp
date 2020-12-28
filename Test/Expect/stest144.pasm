pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getsiz
	mov	result1, #8
_getsiz_ret
	ret

_getx
	rdbyte	result1, arg01
_getx_ret
	ret

_gety
	add	arg01, #1
	rdbyte	result1, arg01
_gety_ret
	ret

_getz
	add	arg01, #4
	rdlong	result1, arg01
_getz_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
