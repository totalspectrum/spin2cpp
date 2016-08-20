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
	mov	_tmp001_, #_get_a
	mov	_tmp002_, _get_i
	add	_tmp001_, _tmp002_
	mov	_tmp003_, objptr
	mov	_tmp004_, _get_i
	shl	_tmp004_, #2
	add	_tmp003_, _tmp004_
	rdlong	_tmp005_, _tmp003_
	movs	wrcog, #_tmp005_
	movd	wrcog, _tmp001_
	call	#wrcog
	mov	_tmp002_, #_get_a
	add	_tmp002_, _get_i
	movs	wrcog, _tmp002_
	movd	wrcog, #_tmp004_
	call	#wrcog
	add	_get_sum, _tmp004_
	add	_get_i, #1
	jmp	#L__0001
L__0003
	mov	result1, _get_sum
_get_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

_get_a
	long	0[10]
_get_i
	long	0
_get_sum
	long	0
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
	fit	496
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
