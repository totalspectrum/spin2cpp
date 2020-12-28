pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_dummy
	mov	result1, imm_1024_
_dummy_ret
	ret

__lockreg
	long	0
imm_1024_
	long	1024
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
