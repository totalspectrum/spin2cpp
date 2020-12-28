pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	shl	arg01, #5
	add	arg01, ptr__dat__
	shl	arg02, #2
	add	arg02, arg01
	rdlong	result1, arg02
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
	byte	$00[1312]
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
