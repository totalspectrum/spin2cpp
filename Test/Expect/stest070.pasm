PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_start
	rdlong	start_tmp001_, #0
	mov	_start__cse__0001, start_tmp001_
	shl	_start__cse__0001, #2
	add	_start__cse__0001, start_tmp001_
	mov	arg1, CNT
	add	arg1, _start__cse__0001
	waitcnt	arg1, #0
_start_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_start__cse__0001
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
start_tmp001_
	res	1
	fit	496
