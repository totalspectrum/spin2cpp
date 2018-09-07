PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_inc
	mov	_var_03, objptr
	mov	_var_02, #10
LR__0001
	rdlong	_var_04, _var_03
	add	_var_04, arg1
	wrlong	_var_04, _var_03
	add	_var_03, #4
	djnz	_var_02, #LR__0001
_inc_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[10]
	org	COG_BSS_START
_var_02
	res	1
_var_03
	res	1
_var_04
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
