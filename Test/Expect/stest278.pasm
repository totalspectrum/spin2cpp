pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_bump1
	rdlong	result1, ptr__dat__
	add	ptr__dat__, #4
	rdlong	result2, ptr__dat__
	mov	_var01, result1
	mov	_var02, result2
	sub	ptr__dat__, #4
	add	result1, #1 wc
	addx	result2, #0
	wrlong	result1, ptr__dat__
	add	ptr__dat__, #4
	wrlong	result2, ptr__dat__
	sub	ptr__dat__, #4
	mov	result1, _var01
	mov	result2, _var02
_bump1_ret
	ret

_bump3
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
arg01
	res	1
arg02
	res	1
arg03
	res	1
arg04
	res	1
	fit	496
