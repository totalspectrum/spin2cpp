pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_set1
	mov	_var01, #18
LR__0001
	rdlong	_var02, arg01
	sar	_var02, arg02
	wrlong	_var02, arg01
	add	arg01, #4
	djnz	_var01, #LR__0001
_set1_ret
	ret

_set2
	mov	_var01, #18
LR__0010
	add	arg01, #4
	mov	_var02, arg01
	rdlong	_var03, _var02
	sar	_var03, arg02
	wrlong	_var03, _var02
	djnz	_var01, #LR__0010
_set2_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
