pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	result1, arg01
	shl	result1, #1
	add	result1, arg01
	add	result1, ptr__dat__
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
	byte	$30, $31, $00, $61, $62, $00, $00, $00
	org	COG_BSS_START
arg01
	res	1
	fit	496
