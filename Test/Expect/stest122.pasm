pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_program
	mov	_var01, #99
	mov	_var02, objptr
	mov	_var03, #100
LR__0001
	wrbyte	_var01, _var02
	add	_var02, #1
	djnz	_var03, #LR__0001
_program_ret
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
	long	0[25]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
	fit	496
