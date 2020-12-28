pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah1
	mov	_var01, #1
	mov	_var01, #2
	mov	result1, _var01
_blah1_ret
	ret

_blah2
	mov	result1, #2
_blah2_ret
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
_var01
	res	1
	fit	496
