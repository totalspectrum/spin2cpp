PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_zcount
	cmps	arg2, arg1 wc,wz
 if_a	mov	_tmp001_, #1
 if_be	neg	_tmp001_, #1
	add	arg2, _tmp001_
LR__0001
	mov	OUTA, arg1
	add	arg1, _tmp001_
	cmps	arg1, arg2 wz
 if_ne	jmp	#LR__0001
_zcount_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
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
