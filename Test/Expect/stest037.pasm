PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_fetch
	shl	arg2, #1
	add	arg2, arg1
	rdword	result1, arg2
_fetch_ret
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
	fit	496
