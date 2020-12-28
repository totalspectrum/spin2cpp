pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_get
	mov	_var01, arg01
	shl	_var01, #1
	add	_var01, arg01
	shl	_var01, #2
	add	_var01, ptr__dat__
	add	_var01, #4
	rdlong	result1, _var01
_get_ret
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
	byte	$01, $02, $00, $00, $03, $00, $00, $00, $04, $00, $00, $00, $11, $12, $00, $00
	byte	$13, $00, $00, $00, $14, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
