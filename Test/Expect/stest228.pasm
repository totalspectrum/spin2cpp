pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_f1
	rdword	result1, ptr__dat__
	shl	result1, #16
	sar	result1, #16
_f1_ret
	ret

_f2
	add	ptr__dat__, #4
	rdword	result1, ptr__dat__
	sub	ptr__dat__, #4
	shl	result1, #16
	sar	result1, #16
_f2_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$01, $00, $00, $00, $00, $00, $80, $3f
	org	COG_BSS_START
	fit	496
