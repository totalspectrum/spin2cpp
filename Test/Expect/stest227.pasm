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
	mov	result1, arg01
	shl	result1, #1
	add	result1, arg01
	mov	arg01, ptr__dat__
	add	arg01, #4
	add	result1, arg01
	add	arg02, result1
	rdbyte	result1, arg02
_f3_ret
	ret

_f2
	mov	result1, arg01
	shl	result1, #1
	add	result1, arg01
	mov	arg01, ptr__dat__
	add	arg01, #10
	add	result1, arg01
	add	arg02, result1
	rdbyte	result1, arg02
_f2_ret
	ret

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
arg01
	res	1
arg02
	res	1
	fit	496
