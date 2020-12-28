pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_show
	mov	_var01, arg01
	mov	_var02, arg02
	mov	_var03, arg03
	mov	outa, _var02
	mov	outb, _var03
_show_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
