pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	rdlong	outa, arg01
_foo_ret
	ret

_main
	rdlong	outa, ptr__dat__
	mov	result1, #0
_main_ret
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
	byte	$00, $01, $00, $00, $00, $02, $00, $00, $00, $03, $00, $00
	org	COG_BSS_START
arg01
	res	1
	fit	496
