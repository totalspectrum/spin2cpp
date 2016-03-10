DAT
	org	0

_mylen
	call	#__system__strsize
_mylen_ret
	ret

__system__strsize
	mov	__system__strsize_r, #0
L_019_
	rdbyte	_system__strsize_tmp001_, arg1_ wz
 if_ne	add	__system__strsize_r, #1
 if_ne	add	arg1_, #1
 if_ne	jmp	#L_019_
	mov	result_, __system__strsize_r
__system__strsize_ret
	ret

__system__strsize_r
	long	0
_system__strsize_tmp001_
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
