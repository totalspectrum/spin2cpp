pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	_var01, #99
	mov	_var02, #0
LR__0001
	cmp	_var02, #10 wz
 if_e	mov	_var01, _var02
	add	_var02, #1
	cmps	_var02, #21 wc
 if_b	jmp	#LR__0001
	mov	outa, _var01
	mov	outb, _var02
_main_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
	fit	496
