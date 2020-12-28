pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_val511
	mov	result1, #511
_val511_ret
	ret

_val512
	mov	result1, imm_512_
_val512_ret
	ret

__lockreg
	long	0
imm_512_
	long	512
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
