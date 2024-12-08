pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	_var03, arg04
	mov	_var01, arg02
	mov	_var02, arg03
	mov	_var04, arg01
	add	_var04, #_var01
	'.live	result1
	movs	wrcog, _var04
	movd	wrcog, #result1
	call	#wrcog
_demo_ret
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
arg01
	res	1
arg02
	res	1
arg03
	res	1
arg04
	res	1
	fit	496
