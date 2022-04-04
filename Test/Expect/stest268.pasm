pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_testit
	mov	_var01, imm_598_
	add	ptr__dat__, #4
	wrbyte	_var01, ptr__dat__
	rdbyte	_var01, ptr__dat__
	sub	ptr__dat__, #4
	wrlong	_var01, ptr__dat__
_testit_ret
	ret

imm_598_
	long	598
ptr__dat__
	long	@@@_dat_
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00, $00, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
	fit	496
