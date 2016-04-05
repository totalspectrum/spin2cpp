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
	mov	__system__strsize_r, #0
L__90040
	rdbyte	_system__strsize_tmp001_, arg1 wz
 if_ne	add	__system__strsize_r, #1
 if_ne	add	arg1, #1
 if_ne	jmp	#L__90040
	mov	result1, __system__strsize_r
__system__strsize_ret
	ret

__system__strsize_r
	long	0
_system__strsize_tmp001_
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
