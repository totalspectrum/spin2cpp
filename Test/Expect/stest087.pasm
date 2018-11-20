pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_update
	mov	_var_01, #0
LR__0001
	cmps	_var_01, #10 wc,wz
 if_ae	jmp	#LR__0002
	rdlong	_var_03, arg1
	add	_var_03, #1
	wrlong	_var_03, arg1
	add	_var_01, #1
	add	arg1, #4
	jmp	#LR__0001
LR__0002
_update_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_01
	res	1
_var_03
	res	1
arg1
	res	1
	fit	496
