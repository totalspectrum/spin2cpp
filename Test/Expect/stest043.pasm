PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_blah
	mov	_tmp001_, #0
	cmps	arg1, #0 wc,wz
 if_ae	jmp	#LR__0001
	cmps	arg2, #0 wc,wz
 if_b	neg	_tmp001_, #1
LR__0001
	mov	result1, _tmp001_
_blah_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
