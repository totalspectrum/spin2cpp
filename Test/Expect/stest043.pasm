PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_blah
	mov	_tmp001_, #0
	cmps	arg1, #0 wc,wz
 if_ae	jmp	#L__0001
	cmps	arg2, #0 wc,wz
 if_b	neg	_tmp001_, #1
L__0001
	mov	result1, _tmp001_
_blah_ret
	ret

_tmp001_
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
	fit	496
