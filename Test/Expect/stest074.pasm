pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_get
	mov	_var01, #0
	mov	_var02, objptr
	mov	_var03, #0
LR__0001
	mov	_var04, _var03
	add	_var04, #_get_a
	rdlong	_var05, _var02
	'.live	_var05
	movs	wrcog, #_var05
	movd	wrcog, _var04
	call	#wrcog
	mov	_var06, _var03
	add	_var06, #_get_a
	'.live	_var07
	movs	wrcog, _var06
	movd	wrcog, #_var07
	call	#wrcog
	add	_var01, _var07
	add	_var03, #1
	add	_var02, #4
	cmps	_var03, #10 wc,wz
 if_b	jmp	#LR__0001
	mov	result1, _var01
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
_get_a
	res	10
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
_var05
	res	1
_var06
	res	1
_var07
	res	1
	fit	496
