pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	shl	arg01, #4
	add	arg01, ptr__dat__
	add	arg01, #12
	rdlong	result1, arg01
_foo_ret
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
	byte	$61, $62, $63, $64, $00, $00, $00, $00, $00, $00, $00, $00
	long	@@@_dat_ + 32
	byte	$58, $59, $5a, $57, $00, $00, $00, $00, $00, $00, $00, $00
	long	@@@_dat_ + 38
	byte	$68, $65, $6c, $6c, $6f, $00, $67, $6f, $6f, $64, $62, $79, $65, $00, $00, $00
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
