con
	T_NONE = 0
	T_FIRST = 256
	T_SECOND = 257
	T_THIRD = 258
	S_FOO = 0
	S_BAR = 1
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_get257
	mov	result1, #257
_get257_ret
	ret

_get1
	mov	result1, #1
_get1_ret
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
