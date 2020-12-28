pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	mov	result1, #3
_blah_ret
	ret

_bar
	add	arg01, #1
	mov	result1, arg01
_bar_ret
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
