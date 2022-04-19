pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_onetwo
	mov	result2, #2
	mov	result1, #1
_onetwo_ret
	ret

result1
	long	0
result2
	long	1
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
