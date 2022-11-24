pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_program
	mov	_var01, #99
	add	ptr__dat__, #4
	wrlong	_var01, ptr__dat__
	sub	ptr__dat__, #4
_program_ret
	ret

ptr__dat__
	long	@@@_dat_
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00[132]
	org	COG_BSS_START
_var01
	res	1
	fit	496
