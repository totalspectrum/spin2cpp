PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_get
	mov	_var_sum, #0
	mov	_var_01, #0
L__0001
	mov	_tmp002_, #496
	add	_tmp002_, _var_01
	movs	wrcog, _tmp002_
	movd	wrcog, #_tmp003_
	call	#wrcog
	add	_var_sum, _tmp003_
	add	_var_01, #1
	cmps	_var_01, #10 wc,wz
 if_b	jmp	#L__0001
	mov	result1, _var_sum
_get_ret
	ret

_put
	mov	_var_01, #0
L__0004
	mov	_tmp001_, #496
	add	_tmp001_, _var_01
	movs	wrcog, #_var_01
	movd	wrcog, _tmp001_
	call	#wrcog
	add	_var_01, #1
	cmps	_var_01, #10 wc,wz
 if_b	jmp	#L__0004
_put_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
	res	1
_tmp002_
	res	1
_tmp003_
	res	1
_var_01
	res	1
_var_sum
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
