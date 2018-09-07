PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_addone
	mov	result1, #3
_addone_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
	fit	496
