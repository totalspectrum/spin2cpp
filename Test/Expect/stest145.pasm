pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_msg
	mov	msg_tmp001_, arg01
	mov	arg01, #1
	mov	msg_tmp003_, objptr
	mov	objptr, msg_tmp001_
	call	#_simplepin_spin_tx
	mov	objptr, msg_tmp003_
_msg_ret
	ret

_init
	mov	arg01, #8
	wrlong	arg01, ptr__dat__
	mov	init_tmp001_, ptr__dat__
	mov	arg01, #0
	mov	init_tmp003_, objptr
	mov	objptr, init_tmp001_
	call	#_simplepin_spin_tx
	mov	objptr, init_tmp003_
_init_ret
	ret

_simplepin_spin_tx
	mov	_var01, #1
	rdlong	_var02, objptr
	shl	_var01, _var02
	test	arg01, #1 wz
	muxnz	outa, _var01
_simplepin_spin_tx_ret
	ret

objptr
	long	@@@objmem
ptr__dat__
	long	@@@_dat_
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00
objmem
	long	0[0]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
init_tmp001_
	res	1
init_tmp003_
	res	1
msg_tmp001_
	res	1
msg_tmp003_
	res	1
	fit	496
