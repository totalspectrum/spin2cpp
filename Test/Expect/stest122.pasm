PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_program
	mov	_var_00, #99
	mov	_var_03, objptr
	mov	_var_01, #100
LR__0001
	wrbyte	_var_00, _var_03
	add	_var_03, #1
	djnz	_var_01, #LR__0001
_program_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[25]
	org	COG_BSS_START
_var_00
	res	1
_var_01
	res	1
_var_03
	res	1
	fit	496
