PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_val511
	mov	result1, #511
_val511_ret
	ret

_val512
	mov	result1, imm_512_
_val512_ret
	ret

imm_512_
	long	512
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
	fit	496
