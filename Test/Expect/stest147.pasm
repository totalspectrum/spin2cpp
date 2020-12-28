pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_program
	mov	_var01, #2
	add	_var01, objptr
	mov	_var02, #1
	add	objptr, #100
	add	_var02, objptr
	add	objptr, #100
	mov	_var03, objptr
	mov	_var04, #9
	sub	objptr, #200
LR__0001
	mov	_var05, #0
	wrbyte	_var05, _var01
	mov	_var05, #1
	wrbyte	_var05, _var02
	mov	_var05, #2
	wrbyte	_var05, _var03
	add	_var01, #1
	add	_var02, #1
	add	_var03, #1
	djnz	_var04, #LR__0001
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
	long	0[75]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
_var05
	res	1
	fit	496
