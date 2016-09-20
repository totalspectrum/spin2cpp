PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_inc
	mov	_var_02, #0
L__0002
	cmps	_var_02, #9 wc,wz
 if_a	jmp	#L__0004
	mov	_var_03, objptr
	mov	_tmp003_, _var_02
	shl	_tmp003_, #2
	add	_var_03, _tmp003_
	rdlong	_tmp001_, _var_03
	add	_tmp001_, arg1
	wrlong	_tmp001_, _var_03
	add	_var_02, #1
	jmp	#L__0002
L__0004
_inc_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	res	10
	org	COG_BSS_START
_tmp001_
	res	1
_tmp003_
	res	1
_var_02
	res	1
_var_03
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
