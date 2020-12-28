pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_clr
	mov	_var01, #10
	add	objptr, #40
	mov	_var02, objptr
	sub	objptr, #40
LR__0001
	sub	_var02, #4
	mov	_var03, #0
	wrlong	_var03, _var02
	djnz	_var01, #LR__0001
_clr_ret
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
	fit	496
