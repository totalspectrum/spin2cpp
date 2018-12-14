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

_simplepin_tx
	mov	_var01, #1
	rdlong	_var02, objptr
	shl	_var01, _var02
	test	arg01, #1 wz
	muxnz	outa, _var01
_simplepin_tx_ret
	ret

objptr
	long	@@@objmem
ptr__dat__
	long	@@@_dat_
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00[132]
objmem
	long	0[1]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
	fit	496
