pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_send
	rev	arg01, #24
	add	objptr, #4
	rdlong	_var01, objptr
	sub	objptr, #4
	mov	_var02, #1
	shl	_var02, _var01
	mov	_var03, #8
LR__0001
	shr	arg01, #1 wc
	muxc	outb, _var02
	djnz	_var03, #LR__0001
	mov	_var04, #1
	shl	_var04, _var01
	or	outb, _var04
_send_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
objmem
	long	0[2]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
arg01
	res	1
	fit	496
