pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	rdlong	result1, ptr__dat__
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
	byte	$01, $00, $00, $00
	org	COG_BSS_START
	fit	496
