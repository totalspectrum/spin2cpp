PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_mylen
	call	#__system__strsize
_mylen_ret
	ret

__system__strsize
	mov	_var_r, #0
L__90040
	rdbyte	_tmp001_, arg1 wz
 if_ne	add	_var_r, #1
 if_ne	add	arg1, #1
 if_ne	jmp	#L__90040
	mov	result1, _var_r
__system__strsize_ret
	ret

_tmp001_
	long	0
_var_r
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
