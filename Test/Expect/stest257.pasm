pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetch2
_fetch1
	rdlong	result1, arg01
	add	arg01, #4
	rdlong	result2, arg01
_fetch1_ret
_fetch2_ret
	ret


result1
	long	0
result2
	long	1
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
