pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_update
	mov	_var01, #0
LR__0001
	cmps	_var01, #10 wc
 if_b	rdlong	_var02, arg01
 if_b	add	_var02, #1
 if_b	wrlong	_var02, arg01
 if_b	add	_var01, #1
 if_b	add	arg01, #4
 if_b	jmp	#LR__0001
_update_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
	fit	496
