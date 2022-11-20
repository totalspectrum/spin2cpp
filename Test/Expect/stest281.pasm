pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_update_stuff
	rdword	result1, arg01
	rdlong	_var01, ptr__dat__
	add	result1, _var01
	wrword	result1, arg01
	mov	result1, #0
_update_stuff_ret
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
arg01
	res	1
	fit	496
