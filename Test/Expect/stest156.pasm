pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getbar
	add	ptr__dat__, #4
	rdlong	result1, ptr__dat__
	sub	ptr__dat__, #4
_getbar_ret
	ret

_getbaz
	add	ptr__dat__, #12
	rdlong	result1, ptr__dat__
	sub	ptr__dat__, #12
_getbaz_ret
	ret

_getbazaddr
	mov	result1, ptr__dat__
	add	result1, #12
_getbazaddr_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$01, $00, $00, $00
	long	@@@_dat_ + 20
	byte	$02, $00, $00, $00
	long	@@@_dat_ + 26
	byte	$03, $00, $00, $00, $68, $65, $6c, $6c, $6f, $00, $61, $61, $61, $61, $00, $00
	org	COG_BSS_START
	fit	496
