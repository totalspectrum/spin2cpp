pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_get
	mov	_var01, #0
	mov	_var02, #0
LR__0001
	mov	_var03, _var02
	or	_var03, #496
	'.live	_var04
	movs	wrcog, _var03
	movd	wrcog, #_var04
	call	#wrcog
	add	_var01, _var04
	add	_var02, #1
	cmps	_var02, #10 wc
 if_b	jmp	#LR__0001
	mov	result1, _var01
_get_ret
	ret

_put
	mov	_var01, #0
LR__0010
	mov	_var02, _var01
	or	_var02, #496
	'.live	_var01
	movs	wrcog, #_var01
	movd	wrcog, _var02
	call	#wrcog
	add	_var01, #1
	cmps	_var01, #10 wc
 if_b	jmp	#LR__0010
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
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
	fit	496
