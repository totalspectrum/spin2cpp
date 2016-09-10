PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_count1
	mov	_var_00, #5
L__0001
	xor	OUTA, #2
	djnz	_var_00, #L__0001
_count1_ret
	ret

_var_00
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
	fit	496
