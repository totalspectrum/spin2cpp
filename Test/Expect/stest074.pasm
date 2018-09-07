PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_get
	mov	_var_12, #0
	mov	_var_13, objptr
	mov	_var_11, #0
LR__0001
	mov	_tmp002_, _var_11
	add	_tmp002_, #_var_01
	rdlong	_tmp004_, _var_13
	'.live	_tmp004_
	movs	wrcog, #_tmp004_
	movd	wrcog, _tmp002_
	call	#wrcog
	mov	_tmp003_, _var_11
	add	_tmp003_, #_var_01
	'.live	_tmp005_
	movs	wrcog, _tmp003_
	movd	wrcog, #_tmp005_
	call	#wrcog
	add	_var_12, _tmp005_
	add	_var_11, #1
	add	_var_13, #4
	cmps	_var_11, #10 wc,wz
 if_b	jmp	#LR__0001
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
_tmp002_
	res	1
_tmp003_
	res	1
_tmp004_
	res	1
_tmp005_
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
