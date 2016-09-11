PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_get
	mov	_var_12, #0
	mov	_var_11, #0
L__0001
	cmps	_var_11, #9 wc,wz
 if_a	jmp	#L__0003
	mov	_tmp001_, #_var_01
	mov	_tmp002_, _var_11
	add	_tmp001_, _tmp002_
	mov	_tmp003_, objptr
	mov	_tmp004_, _var_11
	shl	_tmp004_, #2
	add	_tmp003_, _tmp004_
	rdlong	_tmp005_, _tmp003_
	movs	wrcog, #_tmp005_
	movd	wrcog, _tmp001_
	call	#wrcog
	mov	_tmp002_, #_var_01
	add	_tmp002_, _var_11
	movs	wrcog, _tmp002_
	movd	wrcog, #_tmp004_
	call	#wrcog
	add	_var_12, _tmp004_
	add	_var_11, #1
	jmp	#L__0001
L__0003
	mov	result1, _var_12
_get_ret
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
_tmp004_
	long	0
_tmp005_
	long	0
_var_01
	long	0[10]
_var_11
	long	0
_var_12
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	res	10
	org	COG_BSS_START
	fit	496
