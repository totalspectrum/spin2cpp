PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_myfill
	cmps	arg3, #0 wz
 if_e	jmp	#L__0005
L__0006
	wrlong	arg2, arg1
	add	arg1, #4
	djnz	arg3, #L__0006
L__0005
_myfill_ret
	ret

_fillzero
	mov	arg3, arg2
	mov	arg2, #0
	call	#_myfill
_fillzero_ret
	ret

_fillone
	mov	arg3, arg2
	neg	arg2, #1
	call	#_myfill
_fillone_ret
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
	fit	496
