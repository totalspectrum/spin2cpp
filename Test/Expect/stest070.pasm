DAT
	org	0

_start
	rdlong	start_tmp003_, #0
	mov	start_tmp002_, start_tmp003_
	shl	start_tmp002_, #2
	add	start_tmp002_, start_tmp003_
	mov	arg1, CNT
	add	arg1, start_tmp002_
	waitcnt	arg1, #0
_start_ret
	ret

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
start_tmp002_
	long	0
start_tmp003_
	long	0
	fit	496
