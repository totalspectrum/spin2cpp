pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_f1
	add	arg01, ptr__dat__
	rdbyte	result1, arg01
_f1_ret
	ret

_f3
	mov	_var01, arg01
	shl	_var01, #1
	add	_var01, arg01
	add	ptr__dat__, #4
	add	_var01, ptr__dat__
	add	arg02, _var01
	rdbyte	result1, arg02
	sub	ptr__dat__, #4
_f3_ret
	ret

_f2
	mov	_var01, arg01
	shl	_var01, #1
	add	_var01, arg01
	add	ptr__dat__, #10
	add	_var01, ptr__dat__
	add	arg02, _var01
	rdbyte	result1, arg02
	sub	ptr__dat__, #10
_f2_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$11, $22, $33, $44, $aa, $bb, $cc, $dd, $ee, $ff, $81, $82, $83, $91, $92, $93
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
