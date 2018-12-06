pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	mov	_var01, #0
	cmps	arg1, #0 wc,wz
 if_ae	jmp	#LR__0001
	cmps	arg2, #0 wc,wz
 if_b	neg	_var01, #1
LR__0001
	mov	result1, _var01
_blah_ret
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
	fit	496
