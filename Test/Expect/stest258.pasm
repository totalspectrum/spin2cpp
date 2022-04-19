pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_allbits
	mov	_allbits_r_0001, #0
	mov	_allbits_r_0001_01, #0
LR__0001
	cmps	arg02, #1 wc
 if_b	jmp	#LR__0002
	mov	result1, _allbits_r_0001
	mov	result2, _allbits_r_0001_01
	rdlong	arg03, arg01
	add	arg01, #4
	rdlong	arg04, arg01
	or	result1, arg03
	or	result2, arg04
	mov	_allbits_r_0001, result1
	mov	_allbits_r_0001_01, result2
	add	arg01, #4
	sub	arg02, #1
	jmp	#LR__0001
LR__0002
	mov	result2, _allbits_r_0001_01
	mov	result1, _allbits_r_0001
_allbits_ret
	ret

result1
	long	0
result2
	long	1
COG_BSS_START
	fit	496
	org	COG_BSS_START
_allbits_r_0001
	res	1
_allbits_r_0001_01
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
