PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_get
	mov	_var_12, #0
	mov	_var_13, objptr
	mov	_var_11, #0
L__0003
	cmps	_var_11, #10 wc,wz
 if_ae	jmp	#L__0005
	mov	_tmp001_, #_var_01
	add	_tmp001_, _var_11
	rdlong	_tmp003_, _var_13
	movs	wrcog, #_tmp003_
	movd	wrcog, _tmp001_
	call	#wrcog
	mov	_tmp002_, #_var_01
	add	_tmp002_, _var_11
	movs	wrcog, _tmp002_
	movd	wrcog, #_tmp004_
	call	#wrcog
	add	_var_12, _tmp004_
	add	_var_11, #1
	add	_var_13, #4
	jmp	#L__0003
L__0005
	mov	result1, _var_12
_get_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[10]
	org	COG_BSS_START
_tmp001_
	res	1
_tmp002_
	res	1
_tmp003_
	res	1
_tmp004_
	res	1
_var_01
	res	10
_var_11
	res	1
_var_12
	res	1
_var_13
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
