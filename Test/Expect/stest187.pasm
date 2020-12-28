pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_onezero
	mov	result1, #1
_onezero_ret
	ret

_multizero
	mov	result3, #1
	mov	result1, #0
	mov	result2, #0
_multizero_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
result2
	long	0
result3
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
