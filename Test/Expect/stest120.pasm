pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_sumit
	cmp	arg1, arg2 wz
 if_ne	mov	result1, arg3
 if_ne	jmp	#_sumit_ret
same
	sub	arg1, arg2
	mov	result1, arg1
_sumit_ret
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
	fit	496
