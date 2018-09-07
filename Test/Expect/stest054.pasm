PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_testit
	test	arg1, #4 wz
 if_ne	mov	result1, arg2
 if_e	mov	result1, #0
_testit_ret
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
