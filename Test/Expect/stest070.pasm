PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

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

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
start_tmp002_
	res	1
start_tmp003_
	res	1
	fit	496
