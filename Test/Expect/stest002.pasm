pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
_foo_ret
	ret

_bar
_bar_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
