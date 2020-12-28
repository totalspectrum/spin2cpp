pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetchbuf
	wrbyte	outa, ptr__dat__
	rdbyte	_var01, ptr__dat__ wz
 if_ne	mov	_var02, #0
 if_e	mov	_var02, ptr__dat__
	mov	result1, _var02
_fetchbuf_ret
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
	byte	$00, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
	fit	496
