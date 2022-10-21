pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_lfs_trunc
	mov	_var01, arg01
	rdlong	result1, _var01
	and	result1, #255
	add	ptr__dat__, #4
	rdlong	arg01, ptr__dat__
	sub	ptr__dat__, #4
	add	result1, arg01
	wrlong	result1, _var01
	add	_var01, #4
	rdlong	result1, _var01
	and	result1, #1
	wrlong	result1, _var01
_lfs_trunc_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
