pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zcount
	cmps	arg02, arg01 wc
	negc	_var01, #1
	add	arg02, _var01
LR__0001
	mov	outa, arg01
	add	arg01, _var01
	cmp	arg01, arg02 wz
 if_ne	jmp	#LR__0001
_zcount_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
