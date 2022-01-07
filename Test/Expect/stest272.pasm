pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	rdlong	_var01, ptr__dat__
	mov	_var02, _var01
	add	_var02, #1
	wrlong	_var02, ptr__dat__
	rdbyte	result1, _var01
_blah_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	long	@@@_dat_ + 4
	byte	$68, $65, $6c, $6c, $6f, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
	fit	496
