PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_copy1
	mov	_var_04, #0
	sub	arg3, #1
	cmps	arg3, #0 wc,wz
 if_a	mov	_tmp002_, #1
 if_be	neg	_tmp002_, #1
	add	arg3, _tmp002_
LR__0001
	rdbyte	_tmp008_, arg2
	add	arg2, #1
	wrbyte	_tmp008_, arg1
	add	_var_04, _tmp002_
	cmps	_var_04, arg3 wz
	add	arg1, #1
 if_ne	jmp	#LR__0001
_copy1_ret
	ret

_copy2
	cmps	arg3, #0 wz
 if_e	jmp	#LR__0003
LR__0002
	rdbyte	_tmp005_, arg2
	add	arg2, #1
	wrbyte	_tmp005_, arg1
	add	arg1, #1
	djnz	arg3, #LR__0002
LR__0003
_copy2_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp002_
	res	1
_tmp005_
	res	1
_tmp008_
	res	1
_var_04
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
	fit	496
