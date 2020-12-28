pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	shl	arg01, #4
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
	byte	$01, $00, $00, $00, $02, $00, $00, $00, $03, $00, $00, $00, $04, $00, $00, $00
	byte	$05, $00, $00, $00, $06, $00, $00, $00, $07, $00, $00, $00, $08, $00, $00, $00
	byte	$09, $00, $00, $00, $0a, $00, $00, $00, $0b, $00, $00, $00, $0c, $00, $00, $00
	byte	$0d, $00, $00, $00, $0e, $00, $00, $00, $0f, $00, $00, $00, $10, $00, $00, $00
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
