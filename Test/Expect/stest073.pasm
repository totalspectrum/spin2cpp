PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_get
	mov	_var_sum, #0
	mov	_var_01, #0
L__0001
	cmps	_var_01, #9 wc,wz
 if_a	jmp	#L__0003
	mov	_tmp002_, #496
	add	_tmp002_, _var_01
	movs	wrcog, _tmp002_
	movd	wrcog, #_tmp003_
	call	#wrcog
	add	_var_sum, _tmp003_
	add	_var_01, #1
	jmp	#L__0001
L__0003
	mov	result1, _var_sum
_get_ret
	ret

_put
	mov	_var_01, #0
L__0004
	cmps	_var_01, #9 wc,wz
 if_a	jmp	#L__0006
	mov	_tmp001_, #496
	add	_tmp001_, _var_01
	movs	wrcog, #_var_01
	movd	wrcog, _tmp001_
	call	#wrcog
	add	_var_01, #1
	jmp	#L__0004
L__0006
_put_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

_tmp001_
	long	0
_tmp002_
	long	0
_tmp003_
	long	0
_var_01
	long	0
_var_sum
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
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
