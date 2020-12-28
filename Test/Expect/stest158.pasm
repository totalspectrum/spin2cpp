pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	outb, arg01
_foo_ret
	ret

_get1
	rdlong	result1, ptr__dat__
_get1_ret
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
	long	@@@_dat_ + 4
	byte	$00, $00, $00, $00
	long	@@@_foo
	org	COG_BSS_START
arg01
	res	1
	fit	496
