pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	rdbyte	result1, ptr__dat__
	shl	result1, #8
	or	result1, #1
_blah_ret
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
	byte	$00, $00, $00, $00
	org	COG_BSS_START
	fit	496
