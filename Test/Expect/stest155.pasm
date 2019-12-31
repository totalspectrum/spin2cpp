pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_ident
	mov	_ident_x, arg01
	mov	_ident_x + 1, arg02
	mov	result2, _ident_x + 1
	mov	result1, _ident_x
_ident_ret
	ret

_test
	mov	_ident_x, #0
	mov	_ident_x + 1, #0
	mov	result2, #0
	mov	result1, #0
_test_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

result1
	long	0
result2
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_ident_x
	res	2
_tmp004_
	res	1
_tmp006_
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
