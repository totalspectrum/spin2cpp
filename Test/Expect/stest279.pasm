pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetch
	add	ptr__dat__, #1
	rdbyte	result1, ptr__dat__
	mov	_var01, #88
	wrbyte	_var01, ptr__dat__
	sub	ptr__dat__, #1
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
	fit	496
