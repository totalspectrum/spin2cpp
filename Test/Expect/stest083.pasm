pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_inc
	mov	_var01, objptr
	mov	_var02, #10
LR__0001
	rdlong	_var03, _var01
	add	_var03, arg01
	wrlong	_var03, _var01
	add	_var01, #4
	djnz	_var02, #LR__0001
_inc_ret
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
	long	0[10]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
arg01
	res	1
	fit	496
