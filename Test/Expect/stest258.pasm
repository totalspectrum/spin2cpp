pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_allbits
	mov	result1, #0
	mov	result2, #0
LR__0001
	cmps	arg02, #1 wc
 if_ae	rdlong	arg03, arg01
 if_ae	add	arg01, #4
 if_ae	rdlong	arg04, arg01
 if_ae	or	result1, arg03
 if_ae	or	result2, arg04
 if_ae	add	arg01, #4
 if_ae	sub	arg02, #1
 if_ae	jmp	#LR__0001
_allbits_ret
	ret

result1
	long	0
result2
	long	1
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
arg03
	res	1
arg04
	res	1
	fit	496
