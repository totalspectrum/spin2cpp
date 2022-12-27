pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_myfill
	cmp	arg03, #0 wz
 if_e	jmp	#LR__0002
LR__0001
	wrlong	arg02, arg01
	add	arg01, #4
	djnz	arg03, #LR__0001
LR__0002
_myfill_ret
	ret

_fillzero
	mov	arg03, arg02 wz
	mov	arg02, #0
 if_e	jmp	#LR__0011
LR__0010
	wrlong	arg02, arg01
	add	arg01, #4
	djnz	arg03, #LR__0010
LR__0011
_fillzero_ret
	ret

_fillone
	mov	arg03, arg02 wz
	neg	arg02, #1
 if_e	jmp	#LR__0021
LR__0020
	wrlong	arg02, arg01
	add	arg01, #4
	djnz	arg03, #LR__0020
LR__0021
_fillone_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
