pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_get
	mov	result1, arg01
	shl	result1, #1
	add	result1, arg01
	shl	result1, #2
	add	result1, ptr__dat__
	add	result1, #4
	rdlong	result1, result1
_get_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$01, $02, $00, $00, $03, $00, $00, $00, $04, $00, $00, $00, $11, $12, $00, $00
	byte	$13, $00, $00, $00, $14, $00, $00, $00
	org	COG_BSS_START
arg01
	res	1
	fit	496
