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

_var_01
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
