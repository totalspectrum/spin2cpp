con
	_ = 1
	__ = 2
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	result1, #1
	add	ptr__dat__, #1
	rdbyte	_var01, ptr__dat__
	sub	ptr__dat__, #1
	add	result1, _var01
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
	byte	$01, $02, $00, $00, $00, $03, $04, $00, $06, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
	fit	496
