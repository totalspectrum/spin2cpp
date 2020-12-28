con
	y = 10000
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_gety
	mov	result1, imm_10000_
_gety_ret
	ret

_userdef_spin_start
_userdef_spin_start_ret
	ret

__lockreg
	long	0
imm_10000_
	long	10000
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
