pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fillbuf
LR__0001
	mov	result1, #49
	wrbyte	result1, arg01
	shr	arg02, #1
	cmp	arg02, #1 wc
	add	arg01, #1
 if_ae	jmp	#LR__0001
	mov	result1, #0
	wrbyte	result1, arg01
_fillbuf_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
