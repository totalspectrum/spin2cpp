PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_foo
	mov	_var_01, #5
L__0002
	add	OUTA, #1
	djnz	_var_01, #L__0002
_foo_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_01
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
