pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetchbuf
	wrbyte	outa, ptr__dat__
	rdbyte	_var01, ptr__dat__ wz
 if_ne	mov	result1, #0
 if_e	mov	result1, ptr__dat__
_fetchbuf_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
	fit	496
