pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_onetwo
	mov	result1, #1
	mov	result2, #2
_onetwo_ret
	ret

result1
	long	0
result2
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
