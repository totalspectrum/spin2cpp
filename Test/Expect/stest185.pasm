pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getoff1
	mov	result1, #4
_getoff1_ret
	ret

_getoff2
	mov	result1, #4
_getoff2_ret
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
	fit	496
