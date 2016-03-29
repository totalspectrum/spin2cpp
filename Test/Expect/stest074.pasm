PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry
	mov	arg1, par wz
	call	#_get
cogexit
	cogid	arg1
	cogstop	arg1

_get
	mov	_get_sum, #0
	mov	_get_i, #0
L__0001
	cmps	_get_i, #9 wc,wz
 if_a	jmp	#L__0003
	mov	get_tmp001_, #_get_a
	mov	get_tmp002_, _get_i
	add	get_tmp001_, get_tmp002_
	mov	get_tmp003_, objptr
	mov	get_tmp004_, _get_i
	shl	get_tmp004_, #2
	add	get_tmp003_, get_tmp004_
	rdlong	arg1, get_tmp003_
	movd	wrcog, get_tmp001_
	call	#wrcog
	mov	get_tmp002_, #_get_a
	add	get_tmp002_, _get_i
	movs	rdcog, get_tmp002_
	call	#rdcog
	add	_get_sum, result1
	add	_get_i, #1
	jmp	#L__0001
L__0003
	mov	result1, _get_sum
_get_ret
	ret
rdcog
    mov    result1, 0-0
rdcog_ret
    ret
wrcog
    mov    0-0, arg1
wrcog_ret
    ret

_get_a
	long	0[10]
_get_i
	long	0
_get_sum
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
get_tmp001_
	long	0
get_tmp002_
	long	0
get_tmp003_
	long	0
get_tmp004_
	long	0
objptr
	long	@@@objmem
result1
	long	0
	fit	496
hubexit
	jmp	#cogexit
	long
objmem
	long	$00000000
	long	$00000000
	long	$00000000
	long	$00000000
	long	$00000000
	long	$00000000
	long	$00000000
	long	$00000000
	long	$00000000
	long	$00000000
