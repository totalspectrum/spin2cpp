pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_bump1
	rdlong	result1, ptr__dat__
	add	ptr__dat__, #4
	rdlong	arg04, ptr__dat__
	mov	_var02, arg04
	sub	ptr__dat__, #4
	add	result1, #1 wc
	addx	arg04, #0
	wrlong	result1, ptr__dat__
	add	ptr__dat__, #4
	wrlong	arg04, ptr__dat__
	sub	ptr__dat__, #4
	mov	result2, _var02
	mov	_var04, result2
	mov	result2, _var04
_bump1_ret
	ret

_bump2
	rdlong	result1, ptr__dat__
	add	ptr__dat__, #4
	rdlong	result2, ptr__dat__
	sub	ptr__dat__, #4
	add	result1, #1 wc
	addx	result2, #0
	wrlong	result1, ptr__dat__
	add	ptr__dat__, #4
	wrlong	result2, ptr__dat__
	sub	ptr__dat__, #4
_bump2_ret
	ret

_bump3
	rdlong	result1, ptr__dat__
	add	ptr__dat__, #4
	rdlong	arg04, ptr__dat__
	sub	ptr__dat__, #4
	add	result1, #1 wc
	addx	arg04, #0
	wrlong	result1, ptr__dat__
	add	ptr__dat__, #4
	wrlong	arg04, ptr__dat__
	sub	ptr__dat__, #4
	rdlong	result1, ptr__dat__
	mov	_var02, arg04
	mov	result2, _var02
_bump3_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
result2
	long	1
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00, $00, $00, $00, $00
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
