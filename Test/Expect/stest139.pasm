pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fromsbyte
	rdbyte	result1, arg1
	shl	result1, #24
	sar	result1, #24
_fromsbyte_ret
	ret

_fromubyte
	rdbyte	result1, arg1
_fromubyte_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
	fit	496
