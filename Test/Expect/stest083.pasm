PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_inc
	mov	_var_02, objptr
	mov	_var_01, #10
LR__0001
	rdlong	_var_03, _var_02
	add	_var_03, arg1
	wrlong	_var_03, _var_02
	add	_var_02, #4
	djnz	_var_01, #LR__0001
_inc_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[10]
	org	COG_BSS_START
_var_01
	res	1
_var_02
	res	1
_var_03
	res	1
arg1
	res	1
	fit	496
