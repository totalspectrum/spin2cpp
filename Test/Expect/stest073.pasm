PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_get
	mov	_get_sum, #0
	mov	_get_i, #0
L__0001
	cmps	_get_i, #9 wc,wz
 if_a	jmp	#L__0003
	mov	get_tmp002_, #496
	add	get_tmp002_, _get_i
	movs	wrcog, get_tmp002_
	movd	wrcog, #get_tmp003_
	call	#wrcog
	add	_get_sum, get_tmp003_
	add	_get_i, #1
	jmp	#L__0001
L__0003
	mov	result1, _get_sum
_get_ret
	ret

_put
	mov	_put_i, #0
L__0004
	cmps	_put_i, #9 wc,wz
 if_a	jmp	#L__0006
	mov	put_tmp001_, #496
	add	put_tmp001_, _put_i
	movs	wrcog, #_put_i
	movd	wrcog, put_tmp001_
	call	#wrcog
	add	_put_i, #1
	jmp	#L__0004
L__0006
_put_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

_get_i
	long	0
_get_sum
	long	0
_put_i
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
get_tmp002_
	long	0
get_tmp003_
	long	0
put_tmp001_
	long	0
result1
	long	0
	fit	496
