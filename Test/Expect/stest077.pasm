pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_maskit
	and	arg01, #1
	mov	result1, arg01
_maskit_ret
	ret

_mask2
	and	arg01, arg02
	and	arg01, #255
	mov	result1, arg01
_mask2_ret
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
arg02
	res	1
	fit	496
