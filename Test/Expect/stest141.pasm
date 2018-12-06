pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_mylongset
	mov	_var01, arg1
	cmp	arg3, #0 wz
 if_e	jmp	#LR__0002
LR__0001
	wrlong	arg2, _var01
	add	_var01, #4
	djnz	arg3, #LR__0001
LR__0002
	mov	result1, arg1
_mylongset_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
	fit	496
