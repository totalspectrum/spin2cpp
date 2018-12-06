pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zcount
	cmps	arg2, arg1 wc,wz
 if_a	mov	_var01, #1
 if_be	neg	_var01, #1
	add	arg2, _var01
LR__0001
	mov	outa, arg1
	add	arg1, _var01
	cmp	arg1, arg2 wz
 if_ne	jmp	#LR__0001
_zcount_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
