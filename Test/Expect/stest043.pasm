pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	mov	_var01, #0
	cmps	arg01, #0 wc
 if_ae	jmp	#LR__0001
	cmps	arg02, #0 wc
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
arg01
	res	1
arg02
	res	1
	fit	496
