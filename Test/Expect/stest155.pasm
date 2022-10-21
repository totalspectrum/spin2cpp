pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_ident
	mov	result1, arg01
	mov	_var02, arg02
	mov	result2, _var02
_ident_ret
	ret

_test
	mov	result1, #0
	mov	_var02, #0
	mov	result2, _var02
	wrlong	result1, ptr__dat__
	add	ptr__dat__, #4
	wrlong	result2, ptr__dat__
	sub	ptr__dat__, #4
_test_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
result2
	long	1
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00, $00, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
