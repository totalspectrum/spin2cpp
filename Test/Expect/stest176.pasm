pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fillbuf
LR__0001
	mov	_var01, #49
	wrbyte	_var01, arg01
	shr	arg02, #1
	cmp	arg02, #1 wc
	add	arg01, #1
 if_ae	jmp	#LR__0001
	mov	_var01, #0
	wrbyte	_var01, arg01
	mov	result1, #0
_fillbuf_ret
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
