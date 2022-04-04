pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zeroit
	mov	_var01, #1
	add	arg02, #1
LR__0001
	cmps	_var01, arg02 wc
 if_b	mov	_var02, #0
 if_b	wrbyte	_var02, arg01
 if_b	add	_var01, #1
 if_b	add	arg01, #1
 if_b	jmp	#LR__0001
	mov	result1, #0
_zeroit_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
