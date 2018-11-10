PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_foo
	rdlong	OUTA, arg1
_foo_ret
	ret

_main
	rdlong	OUTA, ptr__dat__
	mov	result1, #0
_main_ret
	ret

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
arg1
	res	1
	fit	496
