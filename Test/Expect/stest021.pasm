PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_waitcycles
	mov	_var_02, CNT
	add	_var_02, arg1
LR__0001
	cmps	CNT, _var_02 wc,wz
 if_b	jmp	#LR__0001
_waitcycles_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_02
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
