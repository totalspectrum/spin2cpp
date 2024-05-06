pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetch
	mov	_var01, ptr__dat__
	add	_var01, #1
	rdbyte	result1, _var01
	mov	_var02, #88
	wrbyte	_var02, _var01
_fetch_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$41, $42, $43, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	byte	$00[64]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
	fit	496
