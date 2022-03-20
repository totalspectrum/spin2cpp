pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_simplemul
	mov	_var01, #0
	mov	_var02, #0
LR__0001
	cmps	_var01, arg01 wc
 if_b	add	_var02, arg02
 if_b	add	_var01, #1
 if_b	jmp	#LR__0001
	mov	result1, _var02
_simplemul_ret
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
